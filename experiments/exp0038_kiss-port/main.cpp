// exp0038_kiss-port — structural port of KISS-ICP (Vizzo et al. 2023).
//
// Reference: https://github.com/PRBonn/kiss-icp/tree/main/cpp/kiss_icp/core
// Notes from source-code reading saved in journal/papers.md.
//
// Major architectural changes vs all prior experiments:
//   * VoxelHashMap (deterministic std::map) REPLACES sliding-window deque + kd-tree.
//     The voxel grid IS the spatial index; NN search probes 27 neighboring voxels.
//   * Linearized point-to-point ICP (3D residual = src - tgt) with Geman-McClure
//     kernel weighting; 6x6 normal equations solved via Cholesky each iteration.
//   * AdaptiveThreshold τ_t = sqrt(Σ(δ_i²)/N) tracking deviation from const-vel
//     prediction. max_corr_dist = 3·τ, GM kernel scale κ = τ (same σ for both).
//   * Two-stage voxelization: frame_downsample at 0.5·v (stored in map),
//     source at 1.5·v (ICP input, COARSER than map — opposite of my exp0008
//     finding, but consistent with KISS-ICP's design).
//   * Range filter: drop points beyond 100 m (KITTI sensor max useful range).
//
// Kept from previous experiments:
//   * KITTI per-point +0.205° vertical-angle correction (exp0025).
//   * Constant-velocity initialization (exp0002).
//   * Hand-rolled SE(3) math, no external deps.
//
// Removed:
//   * Sliding-window deque (replaced by spatial voxel grid)
//   * KdTree (replaced by 27-voxel probe)
//   * Motion-aware trim (kernel handles outliers)
//   * Kabsch closed-form (replaced by linearized GN)
//   * Per-iter MAX_DIST schedule (single adaptive threshold)
//
// Per user guidance: this is the FIRST experiment in a 3-5 experiment block
// for KISS-ICP. A regression vs exp0034's 0.6658% on the first try doesn't
// invalidate the architecture; will tune over subsequent experiments before
// judging.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

// -------------------- math primitives --------------------

struct Vec3 {
    double x, y, z;
};

struct Mat3 {
    double m[9];  // row-major
    static Mat3 identity() {
        Mat3 r{}; r.m[0] = r.m[4] = r.m[8] = 1.0; return r;
    }
};

struct Mat4 {
    double m[16];  // row-major
    static Mat4 identity() {
        Mat4 r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0; return r;
    }
};

static Mat3 mat3_mul(const Mat3 &a, const Mat3 &b) {
    Mat3 c{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            double s = 0;
            for (int k = 0; k < 3; ++k) s += a.m[i * 3 + k] * b.m[k * 3 + j];
            c.m[i * 3 + j] = s;
        }
    return c;
}

static Mat4 mat4_mul(const Mat4 &a, const Mat4 &b) {
    Mat4 c{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            double s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[i * 4 + k] * b.m[k * 4 + j];
            c.m[i * 4 + j] = s;
        }
    return c;
}

static Mat4 mat4_inverse_se3(const Mat4 &T) {
    Mat4 inv{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) inv.m[i * 4 + j] = T.m[j * 4 + i];
    for (int i = 0; i < 3; ++i) {
        double s = 0;
        for (int k = 0; k < 3; ++k) s += inv.m[i * 4 + k] * T.m[k * 4 + 3];
        inv.m[i * 4 + 3] = -s;
    }
    inv.m[15] = 1.0;
    return inv;
}

static Vec3 mat4_apply(const Mat4 &T, const Vec3 &p) {
    Vec3 q;
    q.x = T.m[0] * p.x + T.m[1] * p.y + T.m[2]  * p.z + T.m[3];
    q.y = T.m[4] * p.x + T.m[5] * p.y + T.m[6]  * p.z + T.m[7];
    q.z = T.m[8] * p.x + T.m[9] * p.y + T.m[10] * p.z + T.m[11];
    return q;
}

// SE(3) exponential: exp([rho; phi]) -> SE(3) matrix
// phi is the so(3) axis-angle vector. Uses Rodrigues for R, and the left
// Jacobian V to compute the translation part: t = V * rho.
//
// V = I + (1-cos(θ))/θ² [phi]_× + (θ - sin(θ))/θ³ [phi]_×²  for θ != 0
// V = I  for θ = 0
static Mat4 se3_exp(double rho_x, double rho_y, double rho_z,
                    double phi_x, double phi_y, double phi_z) {
    double theta = std::sqrt(phi_x * phi_x + phi_y * phi_y + phi_z * phi_z);
    Mat4 T = Mat4::identity();

    if (theta < 1e-12) {
        // Pure translation; R = I, V = I.
        T.m[3]  = rho_x;
        T.m[7]  = rho_y;
        T.m[11] = rho_z;
        return T;
    }

    // Rodrigues for R.
    double inv_theta = 1.0 / theta;
    double ax = phi_x * inv_theta, ay = phi_y * inv_theta, az = phi_z * inv_theta;
    double c = std::cos(theta), s = std::sin(theta), v = 1.0 - c;

    // R = I + sin(θ) [a]_× + (1-cos(θ)) [a]_×²
    T.m[0] = c + ax * ax * v;
    T.m[1] = ax * ay * v - az * s;
    T.m[2] = ax * az * v + ay * s;
    T.m[4] = ay * ax * v + az * s;
    T.m[5] = c + ay * ay * v;
    T.m[6] = ay * az * v - ax * s;
    T.m[8] = az * ax * v - ay * s;
    T.m[9] = az * ay * v + ax * s;
    T.m[10] = c + az * az * v;

    // V for t = V * rho.
    // A = (1 - cos(θ))/θ², B = (θ - sin(θ))/θ³
    double A = v / (theta * theta);                          // (1 - cos θ)/θ²
    double B = (theta - s) / (theta * theta * theta);        // (θ - sin θ)/θ³
    // V = I + A [phi]_× + B [phi]_×²
    // [phi]_× = [[0,-pz,py],[pz,0,-px],[-py,px,0]]
    double px = phi_x, py = phi_y, pz = phi_z;
    double sk[9] = { 0,    -pz,   py,
                     pz,    0,   -px,
                    -py,   px,    0 };
    // sk² (using the identity [v]_×² = v vᵀ - |v|² I)
    double p2 = theta * theta;
    double sk2[9] = {
        px * px - p2, px * py,       px * pz,
        py * px,       py * py - p2, py * pz,
        pz * px,       pz * py,       pz * pz - p2
    };
    double V[9];
    for (int i = 0; i < 9; ++i) V[i] = (i % 4 == 0 ? 1.0 : 0.0) + A * sk[i] + B * sk2[i];

    // t = V * rho
    T.m[3]  = V[0] * rho_x + V[1] * rho_y + V[2] * rho_z;
    T.m[7]  = V[3] * rho_x + V[4] * rho_y + V[5] * rho_z;
    T.m[11] = V[6] * rho_x + V[7] * rho_y + V[8] * rho_z;
    return T;
}

// -------------------- 6x6 Cholesky solver --------------------
// Symmetric positive-definite. Solves A x = b. Returns false if not PD.
static bool cholesky6_solve(const double A[36], const double b[6], double x[6]) {
    double L[36] = {0};
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j <= i; ++j) {
            double s = A[i * 6 + j];
            for (int k = 0; k < j; ++k) s -= L[i * 6 + k] * L[j * 6 + k];
            if (i == j) {
                if (s <= 0) return false;
                L[i * 6 + i] = std::sqrt(s);
            } else {
                L[i * 6 + j] = s / L[j * 6 + j];
            }
        }
    }
    double y[6];
    for (int i = 0; i < 6; ++i) {
        double s = b[i];
        for (int k = 0; k < i; ++k) s -= L[i * 6 + k] * y[k];
        y[i] = s / L[i * 6 + i];
    }
    for (int i = 5; i >= 0; --i) {
        double s = y[i];
        for (int k = i + 1; k < 6; ++k) s -= L[k * 6 + i] * x[k];
        x[i] = s / L[i * 6 + i];
    }
    return true;
}

// -------------------- KITTI scan correction (+0.205° vertical-angle bias) --------------------
static void correct_kitti_scan(std::vector<Vec3> &pts) {
    constexpr double THETA = 0.205 * 3.14159265358979323846 / 180.0;
    const double cos_a = std::cos(THETA);
    const double sin_a = std::sin(THETA);
    const double k     = 1.0 - cos_a;
    for (auto &pt : pts) {
        double horiz_norm = std::sqrt(pt.x * pt.x + pt.y * pt.y);
        if (horiz_norm < 1e-12) continue;
        double ux =  pt.y / horiz_norm;
        double uy = -pt.x / horiz_norm;
        double tx = pt.x, ty = pt.y, tz = pt.z;
        double cross_x =  uy * tz;
        double cross_y = -ux * tz;
        double cross_z =  ux * ty - uy * tx;
        double dot = ux * tx + uy * ty;
        pt.x = cos_a * tx + sin_a * cross_x + k * dot * ux;
        pt.y = cos_a * ty + sin_a * cross_y + k * dot * uy;
        pt.z = cos_a * tz + sin_a * cross_z;
    }
}

// -------------------- range filter --------------------
static void range_filter(std::vector<Vec3> &pts, double max_range, double min_range) {
    double max_sq = max_range * max_range;
    double min_sq = min_range * min_range;
    std::vector<Vec3> out;
    out.reserve(pts.size());
    for (const auto &p : pts) {
        double r2 = p.x * p.x + p.y * p.y + p.z * p.z;
        if (r2 < max_sq && r2 > min_sq) out.push_back(p);
    }
    pts = std::move(out);
}

// -------------------- voxel downsample (first-point-seen, like KISS-ICP) --------------------
static std::vector<Vec3> voxel_downsample(const std::vector<Vec3> &pts, double v) {
    std::map<std::tuple<int64_t, int64_t, int64_t>, Vec3> grid;
    double inv = 1.0 / v;
    for (const auto &p : pts) {
        int64_t ix = static_cast<int64_t>(std::floor(p.x * inv));
        int64_t iy = static_cast<int64_t>(std::floor(p.y * inv));
        int64_t iz = static_cast<int64_t>(std::floor(p.z * inv));
        auto key = std::make_tuple(ix, iy, iz);
        if (grid.find(key) == grid.end()) {
            grid[key] = p;
        }
    }
    std::vector<Vec3> out;
    out.reserve(grid.size());
    for (const auto &kv : grid) out.push_back(kv.second);
    return out;
}

// -------------------- VoxelHashMap (KISS-ICP-style local map) --------------------
class VoxelHashMap {
public:
    using Voxel = std::tuple<int64_t, int64_t, int64_t>;

    explicit VoxelHashMap(double voxel_size, double max_distance, size_t max_points_per_voxel)
        : voxel_size_(voxel_size),
          max_distance_(max_distance),
          max_points_per_voxel_(max_points_per_voxel) {
        sub_voxel_dist_ = std::sqrt(voxel_size * voxel_size / static_cast<double>(max_points_per_voxel));
    }

    bool empty() const { return map_.empty(); }

    Voxel point_to_voxel(const Vec3 &p) const {
        double inv = 1.0 / voxel_size_;
        return std::make_tuple(static_cast<int64_t>(std::floor(p.x * inv)),
                               static_cast<int64_t>(std::floor(p.y * inv)),
                               static_cast<int64_t>(std::floor(p.z * inv)));
    }

    // Add points (already in world frame). Skips voxels at capacity AND skips points
    // within sub_voxel_dist of an existing point in the same voxel.
    void add_points(const std::vector<Vec3> &points) {
        for (const auto &p : points) {
            Voxel v = point_to_voxel(p);
            auto it = map_.find(v);
            if (it == map_.end()) {
                std::vector<Vec3> cell;
                cell.reserve(max_points_per_voxel_);
                cell.push_back(p);
                map_[v] = std::move(cell);
            } else {
                auto &cell = it->second;
                if (cell.size() >= max_points_per_voxel_) continue;
                bool too_close = false;
                double thr2 = sub_voxel_dist_ * sub_voxel_dist_;
                for (const auto &q : cell) {
                    double dx = q.x - p.x, dy = q.y - p.y, dz = q.z - p.z;
                    if (dx * dx + dy * dy + dz * dz < thr2) { too_close = true; break; }
                }
                if (!too_close) cell.push_back(p);
            }
        }
    }

    // Remove voxels whose first point is beyond max_distance from origin.
    void remove_far(const Vec3 &origin) {
        double maxd2 = max_distance_ * max_distance_;
        for (auto it = map_.begin(); it != map_.end();) {
            const auto &cell = it->second;
            if (cell.empty()) { it = map_.erase(it); continue; }
            const Vec3 &p = cell.front();
            double dx = p.x - origin.x, dy = p.y - origin.y, dz = p.z - origin.z;
            if (dx * dx + dy * dy + dz * dz >= maxd2) it = map_.erase(it);
            else ++it;
        }
    }

    void update(const std::vector<Vec3> &points_world, const Vec3 &origin) {
        add_points(points_world);
        remove_far(origin);
    }

    // Find closest neighbor in the 27 voxels around query (3x3x3).
    // Returns true and sets out_point + out_dist if any point is within max_dist_sq.
    bool find_nn(const Vec3 &q, double max_dist_sq, Vec3 &out_point, double &out_dist_sq) const {
        if (map_.empty()) return false;
        Voxel v = point_to_voxel(q);
        int64_t cx = std::get<0>(v), cy = std::get<1>(v), cz = std::get<2>(v);
        double best = max_dist_sq;
        bool found = false;
        Vec3 best_pt{0, 0, 0};
        for (int64_t dx = -1; dx <= 1; ++dx)
        for (int64_t dy = -1; dy <= 1; ++dy)
        for (int64_t dz = -1; dz <= 1; ++dz) {
            Voxel nb = std::make_tuple(cx + dx, cy + dy, cz + dz);
            auto it = map_.find(nb);
            if (it == map_.end()) continue;
            for (const auto &p : it->second) {
                double ddx = p.x - q.x, ddy = p.y - q.y, ddz = p.z - q.z;
                double d2 = ddx * ddx + ddy * ddy + ddz * ddz;
                if (d2 < best) { best = d2; best_pt = p; found = true; }
            }
        }
        if (found) { out_point = best_pt; out_dist_sq = best; }
        return found;
    }

private:
    double voxel_size_;
    double max_distance_;
    size_t max_points_per_voxel_;
    double sub_voxel_dist_;
    std::map<Voxel, std::vector<Vec3>> map_;  // deterministic iteration
};

// -------------------- AdaptiveThreshold --------------------
class AdaptiveThreshold {
public:
    AdaptiveThreshold(double initial_threshold, double min_motion_threshold, double max_range)
        : min_motion_threshold_(min_motion_threshold),
          max_range_(max_range),
          model_sse_(initial_threshold * initial_threshold),
          num_samples_(1) {}

    // model_deviation = inv(predicted_pose) * actual_pose  (a small SE(3) error)
    void update(const Mat4 &model_deviation) {
        // Rotation angle from trace.
        double trace = model_deviation.m[0] + model_deviation.m[5] + model_deviation.m[10];
        double cos_arg = 0.5 * (trace - 1.0);
        if (cos_arg >  1.0) cos_arg =  1.0;
        if (cos_arg < -1.0) cos_arg = -1.0;
        double theta = std::acos(cos_arg);
        double delta_rot = 2.0 * max_range_ * std::sin(theta * 0.5);
        double delta_trans = std::sqrt(model_deviation.m[3] * model_deviation.m[3] +
                                       model_deviation.m[7] * model_deviation.m[7] +
                                       model_deviation.m[11] * model_deviation.m[11]);
        double model_error = delta_trans + delta_rot;
        if (model_error > min_motion_threshold_) {
            model_sse_ += model_error * model_error;
            num_samples_++;
        }
    }

    double compute() const {
        return std::sqrt(model_sse_ / static_cast<double>(num_samples_));
    }

private:
    double min_motion_threshold_;
    double max_range_;
    double model_sse_;
    int    num_samples_;
};

// -------------------- ICP (linearized point-to-point, GM kernel) --------------------
struct IcpResult {
    Mat4 T;
    int  iters;
    int  n_corrs;
};

static IcpResult align_points_to_map(const std::vector<Vec3> &frame,
                                     const VoxelHashMap &voxel_map,
                                     const Mat4 &initial_guess,
                                     double max_correspondence_distance,
                                     double kernel_scale,
                                     int max_iter,
                                     double convergence_tol) {
    if (voxel_map.empty()) return {initial_guess, 0, 0};

    // Transform source by initial guess.
    std::vector<Vec3> source(frame.size());
    for (size_t i = 0; i < frame.size(); ++i) source[i] = mat4_apply(initial_guess, frame[i]);

    Mat4 T_icp = Mat4::identity();
    double max_dist_sq = max_correspondence_distance * max_correspondence_distance;
    double k_sq = kernel_scale * kernel_scale;

    int iter;
    int last_n_corrs = 0;
    for (iter = 0; iter < max_iter; ++iter) {
        // Build 6x6 normal equations directly (no per-corr storage).
        double AtA[36] = {0};
        double Atb[6]  = {0};
        int n_used = 0;
        for (const auto &sp : source) {
            Vec3 tp; double d2;
            if (!voxel_map.find_nn(sp, max_dist_sq, tp, d2)) continue;
            // residual r = sp - tp (3-vector)
            double rx = sp.x - tp.x;
            double ry = sp.y - tp.y;
            double rz = sp.z - tp.z;
            double r2 = d2;  // ||r||² == d² since r = sp - tp = -(tp - sp)
            // Geman-McClure weight: w = κ² / (κ + r²)²
            double denom = kernel_scale + r2;
            double w = k_sq / (denom * denom);
            // Jacobian J_r = [I3 | -[sp]_×]  (3×6)
            //   Row 0: [1, 0, 0,  0,  sp.z, -sp.y]
            //   Row 1: [0, 1, 0, -sp.z, 0,  sp.x]
            //   Row 2: [0, 0, 1,  sp.y, -sp.x, 0]
            // Order of xi: [rho_x, rho_y, rho_z, phi_x, phi_y, phi_z]
            double Jr[3][6] = {
                {1, 0, 0,    0,    sp.z, -sp.y},
                {0, 1, 0, -sp.z,    0,    sp.x},
                {0, 0, 1,  sp.y, -sp.x,    0}
            };
            // Accumulate AtA += J^T · w · J and Atb += J^T · w · r.
            for (int a = 0; a < 6; ++a) {
                double Jaw[3] = {w * Jr[0][a], w * Jr[1][a], w * Jr[2][a]};
                for (int b = a; b < 6; ++b) {
                    double s = Jaw[0] * Jr[0][b] + Jaw[1] * Jr[1][b] + Jaw[2] * Jr[2][b];
                    AtA[a * 6 + b] += s;
                }
                Atb[a] += Jaw[0] * rx + Jaw[1] * ry + Jaw[2] * rz;
            }
            ++n_used;
        }
        last_n_corrs = n_used;
        if (n_used < 6) break;

        // Mirror upper to lower.
        for (int i = 0; i < 6; ++i)
            for (int j = i + 1; j < 6; ++j)
                AtA[j * 6 + i] = AtA[i * 6 + j];

        // Solve A · dx = -b (we want dx that minimizes ||r + J·dx||²_w → -A^-1 b).
        double neg_b[6];
        for (int i = 0; i < 6; ++i) neg_b[i] = -Atb[i];
        double dx[6];
        if (!cholesky6_solve(AtA, neg_b, dx)) break;

        // dx = [rho_x, rho_y, rho_z, phi_x, phi_y, phi_z]
        Mat4 estimation = se3_exp(dx[0], dx[1], dx[2], dx[3], dx[4], dx[5]);

        // T_new = estimation * T_curr (left multiply, same as KISS-ICP).
        // Apply to source for next iter.
        for (auto &sp : source) sp = mat4_apply(estimation, sp);
        T_icp = mat4_mul(estimation, T_icp);

        // Convergence on ||dx||.
        double dx_norm = std::sqrt(dx[0] * dx[0] + dx[1] * dx[1] + dx[2] * dx[2] +
                                   dx[3] * dx[3] + dx[4] * dx[4] + dx[5] * dx[5]);
        if (dx_norm < convergence_tol) break;
    }

    Mat4 final_T = mat4_mul(T_icp, initial_guess);
    return {final_T, iter + 1, last_n_corrs};
}

// -------------------- KITTI .bin reader --------------------
static std::vector<Vec3> read_kitti_bin(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "could not open %s\n", path.c_str());
        return {};
    }
    f.seekg(0, std::ios::end);
    std::streamsize bytes = f.tellg();
    f.seekg(0);
    size_t n = static_cast<size_t>(bytes) / (sizeof(float) * 4);
    std::vector<float> buf(n * 4);
    f.read(reinterpret_cast<char *>(buf.data()), bytes);
    std::vector<Vec3> pts;
    pts.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        pts.push_back({buf[i * 4 + 0], buf[i * 4 + 1], buf[i * 4 + 2]});
    }
    return pts;
}

// -------------------- calib parser (extracts Tr) --------------------
static Mat4 read_tr_from_calib(const std::string &calib_path) {
    std::ifstream f(calib_path);
    if (!f) { std::fprintf(stderr, "could not open %s\n", calib_path.c_str()); std::exit(2); }
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("Tr:", 0) == 0 || line.rfind("Tr ", 0) == 0) {
            std::istringstream iss(line.substr(3));
            Mat4 Tr = Mat4::identity();
            for (int i = 0; i < 12; ++i) {
                double v;
                if (!(iss >> v)) { std::fprintf(stderr, "calib Tr parse error\n"); std::exit(2); }
                Tr.m[(i / 4) * 4 + (i % 4)] = v;
            }
            return Tr;
        }
    }
    std::fprintf(stderr, "no Tr line in calib %s\n", calib_path.c_str());
    std::exit(2);
}

// -------------------- minimal YAML get --------------------
static std::string yaml_get(const std::string &path, const std::string &key) {
    std::ifstream f(path);
    std::string line;
    while (std::getline(f, line)) {
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        if (line.compare(start, key.size(), key) != 0) continue;
        size_t after = start + key.size();
        while (after < line.size() && (line[after] == ' ' || line[after] == '\t')) ++after;
        if (after >= line.size() || line[after] != ':') continue;
        ++after;
        while (after < line.size() && (line[after] == ' ' || line[after] == '\t' ||
                                       line[after] == '"' || line[after] == '\'')) ++after;
        std::string val = line.substr(after);
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t' ||
                                val.back() == '\n' || val.back() == '\r' ||
                                val.back() == '"' || val.back() == '\'')) val.pop_back();
        return val;
    }
    return "";
}

static long count_lines(const std::string &path) {
    std::ifstream f(path);
    if (!f) return -1;
    std::string line;
    long n = 0;
    while (std::getline(f, line)) ++n;
    return n;
}

// -------------------- output writer --------------------
static void write_pose_line(std::ofstream &f, const Mat4 &T) {
    char buf[512];
    int len = std::snprintf(buf, sizeof(buf),
        "%.9e %.9e %.9e %.9e %.9e %.9e %.9e %.9e %.9e %.9e %.9e %.9e\n",
        T.m[0],  T.m[1],  T.m[2],  T.m[3],
        T.m[4],  T.m[5],  T.m[6],  T.m[7],
        T.m[8],  T.m[9],  T.m[10], T.m[11]);
    f.write(buf, len);
}

// -------------------- main --------------------
int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <config_path>\n", argv[0]);
        return 1;
    }
    std::string cfg = argv[1];

    std::string sequence_dir   = yaml_get(cfg, "sequence_dir");
    std::string output_path    = yaml_get(cfg, "output_path");
    std::string timestamps_p   = yaml_get(cfg, "timestamps_path");
    std::string calib_path     = yaml_get(cfg, "calib_path");
    if (sequence_dir.empty() || output_path.empty() ||
        timestamps_p.empty() || calib_path.empty()) {
        std::fprintf(stderr, "config missing required keys\n");
        return 2;
    }

    long n_frames = count_lines(timestamps_p);
    if (n_frames <= 0) {
        std::fprintf(stderr, "could not count frames from %s\n", timestamps_p.c_str());
        return 2;
    }

    Mat4 Tr     = read_tr_from_calib(calib_path);
    Mat4 Tr_inv = mat4_inverse_se3(Tr);

    // KISS-ICP config defaults (from kiss_icp/pipeline/KissICP.hpp).
    constexpr double VOXEL_SIZE         = 1.0;     // m, base map voxel
    constexpr double VOXEL_SIZE_MAP     = 0.5;     // α·v: stored-in-map voxel size
    constexpr double VOXEL_SIZE_SRC     = 1.5;     // β·v: ICP source voxel size
    constexpr double MAX_RANGE          = 100.0;   // m, range filter + map prune distance
    constexpr double MIN_RANGE          = 0.0;     // m
    constexpr size_t MAX_POINTS_PER_VOX = 20;
    constexpr double INITIAL_THRESHOLD  = 2.0;     // m, adaptive τ_0
    constexpr double MIN_MOTION_TH      = 0.1;     // m, δ_min for σ accumulation
    constexpr int    MAX_ITER           = 500;
    constexpr double CONVERGENCE_TOL    = 1e-4;

    std::ofstream out(output_path);
    if (!out) {
        std::fprintf(stderr, "could not open output %s\n", output_path.c_str());
        return 4;
    }

    // Pose tracking in LiDAR frame. Initial pose = identity (KISS-ICP convention:
    // world frame = LiDAR frame at t=0). Output is in camera frame via Tr.
    Mat4 last_pose  = Mat4::identity();
    Mat4 last_delta = Mat4::identity();

    VoxelHashMap local_map(VOXEL_SIZE, MAX_RANGE, MAX_POINTS_PER_VOX);
    AdaptiveThreshold adaptive_threshold(INITIAL_THRESHOLD, MIN_MOTION_TH, MAX_RANGE);

    // Output the first pose: T^W_C[0] = last_pose · Tr_inv (with last_pose = I) = Tr_inv,
    // but KITTI convention has T^W_C[0] = identity. KISS-ICP's last_pose tracks LiDAR
    // frame; the camera-frame conversion uses M[i] = last_pose, T^W_C[i] = M[i] · Tr_inv.
    // Since the LiDAR-frame world = LiDAR at t=0, and Tr maps velo→cam, the output is
    // T^W_C[i] = last_pose · Tr (not Tr_inv). Wait, see notes in exp0001:
    //   M[i] = T^W_V[i]    with W = cam0 frame at t=0 in older experiments
    //   In KISS-ICP, world = LiDAR-frame at t=0, so M[i] is in a different world.
    // To match KITTI's eval (poses are camera-frame, world = cam0 at t=0):
    //   Express M_cam[i] = Tr · M_lidar[i] · Tr_inv
    //   So output = Tr · last_pose · Tr_inv

    char fname[32];

    for (long i = 0; i < n_frames; ++i) {
        std::snprintf(fname, sizeof(fname), "%06ld.bin", i);
        std::string p = sequence_dir + "/velodyne/" + fname;
        std::vector<Vec3> scan = read_kitti_bin(p);
        if (scan.empty()) {
            std::fprintf(stderr, "empty scan at %s\n", p.c_str());
            return 3;
        }

        // KITTI vertical-angle correction (intrinsic re-calibration).
        correct_kitti_scan(scan);

        // Range filter.
        range_filter(scan, MAX_RANGE, MIN_RANGE);

        // Two-stage voxelization (KISS-ICP design).
        std::vector<Vec3> frame_downsample = voxel_downsample(scan, VOXEL_SIZE_MAP);
        std::vector<Vec3> source            = voxel_downsample(frame_downsample, VOXEL_SIZE_SRC);

        if (i == 0) {
            // No motion yet — just seed the map at identity, output identity, continue.
            // Transform frame_downsample by last_pose (identity) and add to map.
            local_map.add_points(frame_downsample);
            write_pose_line(out, mat4_mul(Tr, mat4_mul(last_pose, Tr_inv)));
            continue;
        }

        // Constant-velocity prediction.
        Mat4 initial_guess = mat4_mul(last_pose, last_delta);

        // Adaptive threshold.
        double sigma = adaptive_threshold.compute();
        double max_corr_dist = 3.0 * sigma;

        // Run ICP.
        IcpResult r = align_points_to_map(source, local_map, initial_guess,
                                          max_corr_dist, sigma,
                                          MAX_ITER, CONVERGENCE_TOL);
        Mat4 new_pose = r.T;

        // Update adaptive threshold from CV deviation.
        Mat4 model_deviation = mat4_mul(mat4_inverse_se3(initial_guess), new_pose);
        adaptive_threshold.update(model_deviation);

        // Update local map: transform frame_downsample (in LiDAR frame) to world, add,
        // and prune voxels far from origin (current LiDAR position).
        std::vector<Vec3> fd_world(frame_downsample.size());
        for (size_t k = 0; k < frame_downsample.size(); ++k) {
            fd_world[k] = mat4_apply(new_pose, frame_downsample[k]);
        }
        Vec3 origin{new_pose.m[3], new_pose.m[7], new_pose.m[11]};
        local_map.update(fd_world, origin);

        // Update pose tracking.
        last_delta = mat4_mul(mat4_inverse_se3(last_pose), new_pose);
        last_pose  = new_pose;

        // Output: M_cam[i] = Tr · M_lidar[i] · Tr_inv
        write_pose_line(out, mat4_mul(Tr, mat4_mul(last_pose, Tr_inv)));

        if (i % 200 == 0 || i == n_frames - 1) {
            std::fprintf(stderr, "frame %ld/%ld  src=%zu fd=%zu  sigma=%.3f  iters=%d corrs=%d\n",
                         i, n_frames - 1, source.size(), frame_downsample.size(),
                         sigma, r.iters, r.n_corrs);
        }
    }

    out.close();
    return 0;
}
