// exp0003_kdtree-voxel1m — fork of exp0002 with kd-tree NN and finer voxels.
//
// Same frame-to-frame point-to-point ICP and constant-velocity init.
// Two coupled changes:
//   1. Voxel size: 2.0m -> 1.0m (~4x more points per scan).
//   2. NN: brute-force -> deterministic median-split kd-tree built once per
//      target frame, queried per source point per ICP iteration.
//
// Hypothesis: 03 (country road, 47.6%) was starved on correspondences at
// 2.0m voxels. Finer voxels + tree-NN should expose more geometric features
// without busting the wall-clock. Probably also small wins everywhere from
// better correspondence density.

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

// 3x3 row-major.
struct Mat3 {
    double m[9];
    static Mat3 identity() {
        Mat3 r{}; r.m[0] = r.m[4] = r.m[8] = 1.0; return r;
    }
};

// 4x4 row-major SE(3).
struct Mat4 {
    double m[16];
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

static Mat3 mat3_transpose(const Mat3 &a) {
    Mat3 c{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) c.m[i * 3 + j] = a.m[j * 3 + i];
    return c;
}

static double mat3_det(const Mat3 &a) {
    return a.m[0] * (a.m[4] * a.m[8] - a.m[5] * a.m[7])
         - a.m[1] * (a.m[3] * a.m[8] - a.m[5] * a.m[6])
         + a.m[2] * (a.m[3] * a.m[7] - a.m[4] * a.m[6]);
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

// SE(3) inverse: [R|t] -> [R^T | -R^T t].
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

static Mat4 mat4_from_R_t(const Mat3 &R, const Vec3 &t) {
    Mat4 T = Mat4::identity();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) T.m[i * 4 + j] = R.m[i * 3 + j];
    T.m[3]  = t.x; T.m[7]  = t.y; T.m[11] = t.z;
    return T;
}

static Vec3 mat4_apply(const Mat4 &T, const Vec3 &p) {
    Vec3 q;
    q.x = T.m[0] * p.x + T.m[1] * p.y + T.m[2]  * p.z + T.m[3];
    q.y = T.m[4] * p.x + T.m[5] * p.y + T.m[6]  * p.z + T.m[7];
    q.z = T.m[8] * p.x + T.m[9] * p.y + T.m[10] * p.z + T.m[11];
    return q;
}

// -------------------- 3x3 symmetric Jacobi eigensolver --------------------
// Computes the eigendecomposition of a 3x3 symmetric matrix using cyclic
// Jacobi rotations. Deterministic: fixed sweep order, fixed iteration count.
// Returns eigenvalues in lam[3] and eigenvectors as columns of V (row-major).
static void jacobi_eigen_sym_3x3(const Mat3 &A_in, Mat3 &V_out, double lam[3]) {
    double A[3][3];
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) A[i][j] = A_in.m[i * 3 + j];
    // Symmetrize defensively.
    A[0][1] = A[1][0] = 0.5 * (A[0][1] + A[1][0]);
    A[0][2] = A[2][0] = 0.5 * (A[0][2] + A[2][0]);
    A[1][2] = A[2][1] = 0.5 * (A[1][2] + A[2][1]);

    double V[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

    for (int sweep = 0; sweep < 50; ++sweep) {
        double off = std::fabs(A[0][1]) + std::fabs(A[0][2]) + std::fabs(A[1][2]);
        if (off < 1e-18) break;
        // Fixed cyclic pivot order: (0,1), (0,2), (1,2).
        for (int pi = 0; pi < 3; ++pi) {
            int p, q;
            if      (pi == 0) { p = 0; q = 1; }
            else if (pi == 1) { p = 0; q = 2; }
            else              { p = 1; q = 2; }
            double apq = A[p][q];
            if (std::fabs(apq) < 1e-18) continue;
            double app = A[p][p], aqq = A[q][q];
            double theta = (aqq - app) / (2.0 * apq);
            double t;
            if (std::fabs(theta) > 1e15) {
                t = 1.0 / (2.0 * theta);
            } else {
                double sign_theta = (theta >= 0) ? 1.0 : -1.0;
                t = sign_theta / (std::fabs(theta) + std::sqrt(theta * theta + 1.0));
            }
            double c = 1.0 / std::sqrt(t * t + 1.0);
            double s = t * c;
            // Update diagonal.
            A[p][p] = app - t * apq;
            A[q][q] = aqq + t * apq;
            A[p][q] = A[q][p] = 0.0;
            // Update other rows/columns.
            for (int r = 0; r < 3; ++r) {
                if (r != p && r != q) {
                    double arp = A[r][p], arq = A[r][q];
                    A[r][p] = A[p][r] = c * arp - s * arq;
                    A[r][q] = A[q][r] = s * arp + c * arq;
                }
            }
            // Accumulate eigenvectors.
            for (int r = 0; r < 3; ++r) {
                double vrp = V[r][p], vrq = V[r][q];
                V[r][p] = c * vrp - s * vrq;
                V[r][q] = s * vrp + c * vrq;
            }
        }
    }

    lam[0] = A[0][0]; lam[1] = A[1][1]; lam[2] = A[2][2];

    // Sort eigenvalues descending; permute eigenvectors accordingly.
    // Deterministic sort: lexicographic on (-lam, original index).
    std::array<int, 3> idx = {0, 1, 2};
    std::sort(idx.begin(), idx.end(),
              [&](int a, int b) {
                  if (lam[a] != lam[b]) return lam[a] > lam[b];
                  return a < b;
              });
    double lam_sorted[3];
    Mat3 V_sorted{};
    for (int k = 0; k < 3; ++k) {
        lam_sorted[k] = lam[idx[k]];
        for (int r = 0; r < 3; ++r) V_sorted.m[r * 3 + k] = V[r][idx[k]];
    }
    for (int k = 0; k < 3; ++k) lam[k] = lam_sorted[k];
    V_out = V_sorted;
}

// -------------------- voxel downsample --------------------

static std::vector<Vec3> voxel_downsample(const std::vector<Vec3> &pts, double v) {
    std::map<std::tuple<int64_t, int64_t, int64_t>, std::array<double, 4>> acc;
    // acc[key] = {sum_x, sum_y, sum_z, count}
    double inv = 1.0 / v;
    for (const auto &p : pts) {
        int64_t ix = static_cast<int64_t>(std::floor(p.x * inv));
        int64_t iy = static_cast<int64_t>(std::floor(p.y * inv));
        int64_t iz = static_cast<int64_t>(std::floor(p.z * inv));
        auto key = std::make_tuple(ix, iy, iz);
        auto it = acc.find(key);
        if (it == acc.end()) {
            acc[key] = {p.x, p.y, p.z, 1.0};
        } else {
            it->second[0] += p.x;
            it->second[1] += p.y;
            it->second[2] += p.z;
            it->second[3] += 1.0;
        }
    }
    std::vector<Vec3> out;
    out.reserve(acc.size());
    for (const auto &kv : acc) {
        double n = kv.second[3];
        out.push_back({kv.second[0] / n, kv.second[1] / n, kv.second[2] / n});
    }
    return out;
}

// -------------------- KD-tree NN --------------------
//
// Median-split kd-tree, built once per target frame, queried per source point
// per ICP iteration. Deterministic: ties in the median sort are broken by
// original point index, so the tree topology depends only on the input data
// (not on memory addresses or threading). Single-NN search with squared-dist
// gating to drop matches beyond max_dist.

struct KdNode {
    int    left  = -1;
    int    right = -1;
    int    pt_idx = -1;
    int    axis  = 0;
    double split = 0.0;
};

class KdTree {
public:
    std::vector<KdNode> nodes;
    const std::vector<Vec3> *pts;

    explicit KdTree(const std::vector<Vec3> &pts_in) : pts(&pts_in) {
        if (pts->empty()) return;
        std::vector<int> idx(pts->size());
        for (size_t i = 0; i < pts->size(); ++i) idx[i] = static_cast<int>(i);
        nodes.reserve(pts->size());
        build(idx, 0, static_cast<int>(idx.size()), 0);
    }

    int find_nn(const Vec3 &q, double max_dist_sq) const {
        if (nodes.empty()) return -1;
        int    best_idx = -1;
        double best_d2  = max_dist_sq;
        search(0, q, best_idx, best_d2);
        return best_idx;
    }

private:
    static double coord(const Vec3 &p, int axis) {
        return axis == 0 ? p.x : (axis == 1 ? p.y : p.z);
    }

    int build(std::vector<int> &idx, int lo, int hi, int depth) {
        if (lo >= hi) return -1;
        int axis = depth % 3;
        // Deterministic sort: primary = coord on axis, secondary = original index.
        std::sort(idx.begin() + lo, idx.begin() + hi,
                  [&](int a, int b) {
                      double va = coord((*pts)[a], axis);
                      double vb = coord((*pts)[b], axis);
                      if (va != vb) return va < vb;
                      return a < b;
                  });
        int mid = lo + (hi - lo) / 2;
        int my_node = static_cast<int>(nodes.size());
        nodes.push_back({});
        KdNode n;
        n.axis  = axis;
        n.pt_idx = idx[mid];
        n.split = coord((*pts)[idx[mid]], axis);
        n.left  = build(idx, lo, mid, depth + 1);
        n.right = build(idx, mid + 1, hi, depth + 1);
        nodes[my_node] = n;
        return my_node;
    }

    void search(int node_idx, const Vec3 &q, int &best_idx, double &best_d2) const {
        if (node_idx < 0) return;
        const KdNode &n = nodes[node_idx];
        const Vec3   &p = (*pts)[n.pt_idx];
        double dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
        double d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < best_d2) { best_d2 = d2; best_idx = n.pt_idx; }
        double diff = coord(q, n.axis) - n.split;
        int first = (diff < 0) ? n.left  : n.right;
        int other = (diff < 0) ? n.right : n.left;
        search(first, q, best_idx, best_d2);
        if (diff * diff < best_d2) search(other, q, best_idx, best_d2);
    }
};

// -------------------- ICP (point-to-point, kd-tree NN) --------------------

struct IcpResult {
    Mat4 T;
    int iters;
    int n_corrs;
};

static IcpResult icp(const std::vector<Vec3> &source, const std::vector<Vec3> &target,
                     double max_dist, int max_iter, double tol_t, double tol_r,
                     const Mat4 &init) {
    Mat4 T = init;
    double max_dist_sq = max_dist * max_dist;
    KdTree tree(target);  // built once, queried per iter per source point
    int iter;
    int last_n_corrs = 0;
    for (iter = 0; iter < max_iter; ++iter) {
        // Transform source by current T, then find NN in target via kd-tree.
        std::vector<std::pair<Vec3, Vec3>> corrs;
        corrs.reserve(source.size());
        for (const auto &sp : source) {
            Vec3 sp_t = mat4_apply(T, sp);
            int nn_idx = tree.find_nn(sp_t, max_dist_sq);
            if (nn_idx >= 0) corrs.push_back({sp, target[nn_idx]});
        }
        last_n_corrs = static_cast<int>(corrs.size());
        if (corrs.size() < 20) break;

        // Compute centroids (deterministic sum: input order preserved).
        Vec3 sc{0, 0, 0}, tc{0, 0, 0};
        for (const auto &c : corrs) {
            sc.x += c.first.x;  sc.y += c.first.y;  sc.z += c.first.z;
            tc.x += c.second.x; tc.y += c.second.y; tc.z += c.second.z;
        }
        double n = static_cast<double>(corrs.size());
        sc.x /= n; sc.y /= n; sc.z /= n;
        tc.x /= n; tc.y /= n; tc.z /= n;

        // H = sum (s - sc)(t - tc)^T  (3x3).
        Mat3 H{};
        for (const auto &c : corrs) {
            double sx = c.first.x  - sc.x, sy = c.first.y  - sc.y, sz = c.first.z  - sc.z;
            double tx = c.second.x - tc.x, ty = c.second.y - tc.y, tz = c.second.z - tc.z;
            H.m[0] += sx * tx; H.m[1] += sx * ty; H.m[2] += sx * tz;
            H.m[3] += sy * tx; H.m[4] += sy * ty; H.m[5] += sy * tz;
            H.m[6] += sz * tx; H.m[7] += sz * ty; H.m[8] += sz * tz;
        }

        // SVD of H via eigendecomposition of H^T H = V Σ² V^T.
        Mat3 HtH = mat3_mul(mat3_transpose(H), H);
        Mat3 V; double lam[3];
        jacobi_eigen_sym_3x3(HtH, V, lam);

        // Σ from eigenvalues (clamp to nonneg).
        double sigma[3] = {
            std::sqrt(std::max(0.0, lam[0])),
            std::sqrt(std::max(0.0, lam[1])),
            std::sqrt(std::max(0.0, lam[2])),
        };

        // U = H V Σ^-1 (column by column). Guard against tiny sigma.
        Mat3 U{};
        for (int k = 0; k < 3; ++k) {
            double inv_s = (sigma[k] > 1e-12) ? (1.0 / sigma[k]) : 0.0;
            double vc[3] = {V.m[0 * 3 + k], V.m[1 * 3 + k], V.m[2 * 3 + k]};
            double hv[3];
            hv[0] = H.m[0] * vc[0] + H.m[1] * vc[1] + H.m[2] * vc[2];
            hv[1] = H.m[3] * vc[0] + H.m[4] * vc[1] + H.m[5] * vc[2];
            hv[2] = H.m[6] * vc[0] + H.m[7] * vc[1] + H.m[8] * vc[2];
            U.m[0 * 3 + k] = hv[0] * inv_s;
            U.m[1 * 3 + k] = hv[1] * inv_s;
            U.m[2 * 3 + k] = hv[2] * inv_s;
        }

        // R = V * diag(1, 1, det(V U^T)) * U^T.
        Mat3 Ut = mat3_transpose(U);
        Mat3 VUt = mat3_mul(V, Ut);
        double d = mat3_det(VUt);
        double sign = (d > 0) ? 1.0 : -1.0;
        Mat3 D = Mat3::identity();
        D.m[8] = sign;
        Mat3 R_delta = mat3_mul(mat3_mul(V, D), Ut);

        // t = tc - R * sc.
        Vec3 t_delta;
        t_delta.x = tc.x - (R_delta.m[0] * sc.x + R_delta.m[1] * sc.y + R_delta.m[2] * sc.z);
        t_delta.y = tc.y - (R_delta.m[3] * sc.x + R_delta.m[4] * sc.y + R_delta.m[5] * sc.z);
        t_delta.z = tc.z - (R_delta.m[6] * sc.x + R_delta.m[7] * sc.y + R_delta.m[8] * sc.z);

        Mat4 dT = mat4_from_R_t(R_delta, t_delta);
        Mat4 T_new = dT;

        // Convergence check: how much did pose change?
        double dt = std::sqrt(
            (T_new.m[3]  - T.m[3])  * (T_new.m[3]  - T.m[3])  +
            (T_new.m[7]  - T.m[7])  * (T_new.m[7]  - T.m[7])  +
            (T_new.m[11] - T.m[11]) * (T_new.m[11] - T.m[11]));
        // Rotation change: angle from trace of R_new R^T.
        double trace = 0.0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                trace += T_new.m[i * 4 + j] * T.m[i * 4 + j];
        // For pure rotation matrices A B^T, trace = 1 + 2cos(theta). Clamp safely.
        double cos_arg = 0.5 * (trace - 1.0);
        if (cos_arg >  1.0) cos_arg =  1.0;
        if (cos_arg < -1.0) cos_arg = -1.0;
        double dr = std::acos(cos_arg);

        T = T_new;
        if (dt < tol_t && dr < tol_r) break;
    }
    return {T, iter + 1, last_n_corrs};
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
    if (!f) {
        std::fprintf(stderr, "could not open %s\n", calib_path.c_str());
        std::exit(2);
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.rfind("Tr:", 0) == 0 || line.rfind("Tr ", 0) == 0) {
            std::istringstream iss(line.substr(3));
            Mat4 Tr = Mat4::identity();
            for (int i = 0; i < 12; ++i) {
                double v;
                if (!(iss >> v)) {
                    std::fprintf(stderr, "calib Tr parse error\n");
                    std::exit(2);
                }
                int row = i / 4;
                int col = i % 4;
                Tr.m[row * 4 + col] = v;
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
        // Trim leading spaces.
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
        // Strip trailing whitespace and quotes.
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

    // ICP / downsample hyperparameters.
    constexpr double VOXEL_SIZE  = 1.0;       // m
    constexpr double MAX_DIST    = 3.0;       // m, NN gating
    constexpr int    MAX_ITER    = 20;
    constexpr double TOL_T       = 1e-3;      // m
    constexpr double TOL_R       = 1e-3;      // rad

    std::ofstream out(output_path);
    if (!out) {
        std::fprintf(stderr, "could not open output %s\n", output_path.c_str());
        return 4;
    }

    // M[i] = T^W_V_i, with M[0] = Tr (velo frame at i=0 in world=cam0 coords).
    Mat4 M = Tr;
    // First pose output: T^W_C[0] = M @ Tr^-1 = Tr @ Tr^-1 = identity.
    write_pose_line(out, mat4_mul(M, Tr_inv));

    // Load frame 0.
    char fname[32];
    std::snprintf(fname, sizeof(fname), "%06d.bin", 0);
    std::string p0 = sequence_dir + "/velodyne/" + fname;
    std::vector<Vec3> prev_raw = read_kitti_bin(p0);
    std::vector<Vec3> prev = voxel_downsample(prev_raw, VOXEL_SIZE);

    // Constant-velocity carryover. For i=1 we have no prior motion, so use
    // identity — same as exp0001 on that single transition.
    Mat4 prev_delta = Mat4::identity();

    for (long i = 1; i < n_frames; ++i) {
        std::snprintf(fname, sizeof(fname), "%06ld.bin", i);
        std::string p = sequence_dir + "/velodyne/" + fname;
        std::vector<Vec3> curr_raw = read_kitti_bin(p);
        std::vector<Vec3> curr = voxel_downsample(curr_raw, VOXEL_SIZE);

        // source = current frame's points (in V_i), target = previous (in V_{i-1}).
        // ICP returns T such that T @ p_curr ≈ p_prev, i.e. T = T^V_{i-1}_V_i = delta_M.
        // Init with the previous Δpose (constant-velocity assumption).
        IcpResult r = icp(curr, prev, MAX_DIST, MAX_ITER, TOL_T, TOL_R, prev_delta);
        Mat4 delta_M = r.T;
        prev_delta = delta_M;

        M = mat4_mul(M, delta_M);
        write_pose_line(out, mat4_mul(M, Tr_inv));

        // Log progress sparsely so harness logs aren't overwhelmed.
        if (i % 200 == 0 || i == n_frames - 1) {
            std::fprintf(stderr, "frame %ld/%ld  prev=%zu curr=%zu  iters=%d corrs=%d\n",
                         i, n_frames - 1, prev.size(), curr.size(), r.iters, r.n_corrs);
        }

        prev = std::move(curr);
    }

    out.close();
    return 0;
}
