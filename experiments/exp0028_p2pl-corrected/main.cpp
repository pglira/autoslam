// exp0028_p2pl-corrected — retry point-to-plane with the wins from exp0025/0027.
//
// exp0012 ran p2pl on the pre-scan-correction baseline and regressed.
// Since then exp0025 added the KITTI +0.205° per-point correction (a
// big win) and exp0027 tightened MAX_DIST_TIGHT to 1.0m. With cleaner
// geometry post-correction, normal estimation should be more reliable;
// the cleaner residual distribution should make p2pl's surface-aware
// update actually pay off.
//
// Stack on top of exp0012's p2pl machinery:
//  + correct_kitti_scan() applied to every loaded scan
//  + MAX_DIST_TIGHT 1.5 -> 1.0m
//  + k=8 PCA normals (unchanged from exp0012)
//  + 6x6 Cholesky / Rodrigues update (unchanged)
//  + K=20 window, voxel 1.0m map / 0.5m source (unchanged)
//  + no trim block (kept as in exp0012 for cleanest one-variable test)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
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

    // Find k nearest neighbors. Results in `out` as (dist_sq, idx) pairs,
    // sorted ascending by dist_sq, deterministically (lex tiebreak on idx).
    void find_knn(const Vec3 &q, int k, std::vector<std::pair<double, int>> &out) const {
        out.clear();
        if (nodes.empty() || k <= 0) return;
        // Max-heap of size <= k. Comparator: prioritize larger dist_sq at top.
        // For determinism on equal distances, prefer larger index at top (so smaller idx survives).
        auto heap_cmp = [](const std::pair<double, int> &a, const std::pair<double, int> &b) {
            if (a.first != b.first) return a.first < b.first;
            return a.second < b.second;
        };
        std::vector<std::pair<double, int>> heap;
        heap.reserve(static_cast<size_t>(k));
        search_knn(0, q, k, heap, heap_cmp);
        // Sort ascending for downstream use; deterministic (lex on (d2, idx)).
        std::sort(heap.begin(), heap.end(),
                  [](const std::pair<double, int> &a, const std::pair<double, int> &b) {
                      if (a.first != b.first) return a.first < b.first;
                      return a.second < b.second;
                  });
        out = std::move(heap);
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

    template <typename Cmp>
    void search_knn(int node_idx, const Vec3 &q, int k,
                    std::vector<std::pair<double, int>> &heap, Cmp &cmp) const {
        if (node_idx < 0) return;
        const KdNode &n = nodes[node_idx];
        const Vec3   &p = (*pts)[n.pt_idx];
        double dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
        double d2 = dx * dx + dy * dy + dz * dz;
        if (static_cast<int>(heap.size()) < k) {
            heap.push_back({d2, n.pt_idx});
            std::push_heap(heap.begin(), heap.end(), cmp);
        } else if (cmp(std::make_pair(d2, n.pt_idx), heap.front())) {
            std::pop_heap(heap.begin(), heap.end(), cmp);
            heap.back() = {d2, n.pt_idx};
            std::push_heap(heap.begin(), heap.end(), cmp);
        }
        double diff = coord(q, n.axis) - n.split;
        int first = (diff < 0) ? n.left  : n.right;
        int other = (diff < 0) ? n.right : n.left;
        search_knn(first, q, k, heap, cmp);
        double cutoff = (static_cast<int>(heap.size()) < k) ? std::numeric_limits<double>::infinity()
                                                            : heap.front().first;
        if (diff * diff < cutoff) search_knn(other, q, k, heap, cmp);
    }
};

// -------------------- Normal estimation (PCA on k nearest neighbors) --------------------

static std::vector<Vec3> estimate_normals(const std::vector<Vec3> &pts, int k) {
    std::vector<Vec3> normals(pts.size(), Vec3{0, 0, 1});
    if (pts.size() < static_cast<size_t>(k)) return normals;
    KdTree tree(pts);
    std::vector<std::pair<double, int>> knn;
    for (size_t i = 0; i < pts.size(); ++i) {
        tree.find_knn(pts[i], k, knn);
        if (knn.size() < 3) continue;
        // Centroid of the neighborhood.
        Vec3 c{0, 0, 0};
        for (const auto &nb : knn) {
            const Vec3 &p = pts[nb.second];
            c.x += p.x; c.y += p.y; c.z += p.z;
        }
        double inv = 1.0 / static_cast<double>(knn.size());
        c.x *= inv; c.y *= inv; c.z *= inv;
        // Covariance matrix (3x3 symmetric).
        Mat3 C{};
        for (const auto &nb : knn) {
            const Vec3 &p = pts[nb.second];
            double dx = p.x - c.x, dy = p.y - c.y, dz = p.z - c.z;
            C.m[0] += dx * dx; C.m[1] += dx * dy; C.m[2] += dx * dz;
            C.m[4] += dy * dy; C.m[5] += dy * dz;
            C.m[8] += dz * dz;
        }
        C.m[3] = C.m[1]; C.m[6] = C.m[2]; C.m[7] = C.m[5];
        // Eigendecomp: smallest eigenvector is the normal.
        Mat3 V; double lam[3];
        jacobi_eigen_sym_3x3(C, V, lam);
        // lam sorted descending; smallest is lam[2], eigenvector is V column 2.
        Vec3 n{V.m[0 * 3 + 2], V.m[1 * 3 + 2], V.m[2 * 3 + 2]};
        double nn = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (nn > 1e-12) { n.x /= nn; n.y /= nn; n.z /= nn; }
        normals[i] = n;
    }
    return normals;
}

// -------------------- 6x6 Cholesky solver --------------------
// A is symmetric, positive (semi-)definite. Returns false if not PD.
// Solves A x = b. A and b are not modified.
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
    // Forward solve L y = b.
    double y[6];
    for (int i = 0; i < 6; ++i) {
        double s = b[i];
        for (int k = 0; k < i; ++k) s -= L[i * 6 + k] * y[k];
        y[i] = s / L[i * 6 + i];
    }
    // Backward solve L^T x = y.
    for (int i = 5; i >= 0; --i) {
        double s = y[i];
        for (int k = i + 1; k < 6; ++k) s -= L[k * 6 + i] * x[k];
        x[i] = s / L[i * 6 + i];
    }
    return true;
}

// Rodrigues: build rotation matrix from axis-angle vector (phi = axis * angle).
static Mat3 rodrigues(double px, double py, double pz) {
    double theta = std::sqrt(px * px + py * py + pz * pz);
    if (theta < 1e-12) return Mat3::identity();
    double inv = 1.0 / theta;
    double ax = px * inv, ay = py * inv, az = pz * inv;
    double c = std::cos(theta), s = std::sin(theta), v = 1.0 - c;
    Mat3 R{};
    R.m[0] = c + ax * ax * v;
    R.m[1] = ax * ay * v - az * s;
    R.m[2] = ax * az * v + ay * s;
    R.m[3] = ay * ax * v + az * s;
    R.m[4] = c + ay * ay * v;
    R.m[5] = ay * az * v - ax * s;
    R.m[6] = az * ax * v - ay * s;
    R.m[7] = az * ay * v + ax * s;
    R.m[8] = c + az * az * v;
    return R;
}

// -------------------- ICP (point-to-plane, kd-tree NN) --------------------

struct IcpResult {
    Mat4 T;
    int iters;
    int n_corrs;
};

// Point-to-plane ICP. target_nrm has one normal per target point.
// Linearized 6-DoF Gauss-Newton: for each correspondence with target point t
// and normal n, and current-transformed source point s_t = T_curr * s, the
// residual is r = n . (s_t - t). The Jacobian row is J = [(s_t x n)^T, n^T]
// for the perturbation xi = (phi, rho) (rotation; translation). The update
// is T_new = exp(xi) * T_curr (left multiply).
static IcpResult icp_p2pl(const std::vector<Vec3> &source,
                          const std::vector<Vec3> &target,
                          const std::vector<Vec3> &target_nrm,
                          double max_dist_loose, double max_dist_tight,
                          int max_iter, double tol_t, double tol_r,
                          const Mat4 &init) {
    Mat4 T = init;
    double max_dist_sq_loose = max_dist_loose * max_dist_loose;
    double max_dist_sq_tight = max_dist_tight * max_dist_tight;
    KdTree tree(target);  // built once, queried per iter per source point
    int iter;
    int last_n_corrs = 0;
    for (iter = 0; iter < max_iter; ++iter) {
        double max_dist_sq = (iter == 0) ? max_dist_sq_loose : max_dist_sq_tight;
        // For each source point, find NN in target. Drop matches beyond the gate.
        // Accumulate the 6x6 normal-equations directly (no need to store all corrs).
        double AtA[36] = {0};
        double Atb[6]  = {0};
        int n_used = 0;
        for (const auto &sp : source) {
            Vec3 sp_t = mat4_apply(T, sp);
            int nn_idx = tree.find_nn(sp_t, max_dist_sq);
            if (nn_idx < 0) continue;
            const Vec3 &tp = target[nn_idx];
            const Vec3 &n  = target_nrm[nn_idx];
            // Point-to-plane residual scalar.
            double r = n.x * (sp_t.x - tp.x) + n.y * (sp_t.y - tp.y) + n.z * (sp_t.z - tp.z);
            // Jacobian row J = [(s_t x n)^T, n^T]  (size 6).
            double j0 = sp_t.y * n.z - sp_t.z * n.y;
            double j1 = sp_t.z * n.x - sp_t.x * n.z;
            double j2 = sp_t.x * n.y - sp_t.y * n.x;
            double j3 = n.x, j4 = n.y, j5 = n.z;
            // Accumulate symmetric A^T A (upper triangle filled, mirror later).
            AtA[0]  += j0 * j0;
            AtA[1]  += j0 * j1; AtA[7]  += j1 * j1;
            AtA[2]  += j0 * j2; AtA[8]  += j1 * j2; AtA[14] += j2 * j2;
            AtA[3]  += j0 * j3; AtA[9]  += j1 * j3; AtA[15] += j2 * j3; AtA[21] += j3 * j3;
            AtA[4]  += j0 * j4; AtA[10] += j1 * j4; AtA[16] += j2 * j4; AtA[22] += j3 * j4; AtA[28] += j4 * j4;
            AtA[5]  += j0 * j5; AtA[11] += j1 * j5; AtA[17] += j2 * j5; AtA[23] += j3 * j5; AtA[29] += j4 * j5; AtA[35] += j5 * j5;
            // A^T b (b = -r).
            Atb[0] += -j0 * r;
            Atb[1] += -j1 * r;
            Atb[2] += -j2 * r;
            Atb[3] += -j3 * r;
            Atb[4] += -j4 * r;
            Atb[5] += -j5 * r;
            ++n_used;
        }
        last_n_corrs = n_used;
        if (n_used < 20) break;
        // Mirror upper to lower for the 6x6 symmetric.
        for (int i = 0; i < 6; ++i) for (int j = i + 1; j < 6; ++j) AtA[j * 6 + i] = AtA[i * 6 + j];
        // Solve via Cholesky.
        double xi[6];
        if (!cholesky6_solve(AtA, Atb, xi)) break;  // ill-conditioned; abort
        // xi = [phi_x, phi_y, phi_z, rho_x, rho_y, rho_z]
        Mat3 R_step = rodrigues(xi[0], xi[1], xi[2]);
        Vec3 t_step{xi[3], xi[4], xi[5]};
        // Build T_new = [R_step | t_step] * T_curr (left multiply).
        Mat4 R_step_mat = mat4_from_R_t(R_step, t_step);
        Mat4 T_new = mat4_mul(R_step_mat, T);

        // Convergence: step magnitude.
        double dt = std::sqrt(t_step.x * t_step.x + t_step.y * t_step.y + t_step.z * t_step.z);
        double dr = std::sqrt(xi[0] * xi[0] + xi[1] * xi[1] + xi[2] * xi[2]);

        T = T_new;
        if (dt < tol_t && dr < tol_r) break;
    }
    return {T, iter + 1, last_n_corrs};
}

// -------------------- KITTI scan vertical-angle recalibration --------------------
// Per-point intrinsic correction for the KITTI HDL-64E. Rotate each point by
// +0.205° around the perpendicular-to-pt horizontal axis. Origin: Deschaud
// (IMLS-SLAM, 2018); used by CT-ICP and KISS-ICP. Formula matches the
// `_correct_kitti_scan` function in KISS-ICP.
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
    constexpr double VOXEL_SIZE       = 1.0;  // m, used for map storage
    constexpr double VOXEL_SIZE_SRC   = 0.5;  // m, used for ICP source (current scan)
    constexpr double MAX_DIST_LOOSE   = 3.0;  // m, NN gating on ICP iter 0
    constexpr double MAX_DIST_TIGHT   = 1.0;  // m, NN gating on iter 1+ (tightened post-scan-correction)
    constexpr int    NORMAL_K         = 8;    // # nearest neighbors for normal PCA
    constexpr int    MAX_ITER         = 20;
    constexpr double TOL_T       = 1e-3;      // m
    constexpr double TOL_R       = 1e-3;      // rad
    constexpr size_t WINDOW_SIZE = 20;        // sliding-window frame count

    std::ofstream out(output_path);
    if (!out) {
        std::fprintf(stderr, "could not open output %s\n", output_path.c_str());
        return 4;
    }

    // M[i] = T^W_V_i, with M[0] = Tr (velo frame at i=0 in world=cam0 coords).
    Mat4 M = Tr;
    // First pose output: T^W_C[0] = M @ Tr^-1 = Tr @ Tr^-1 = identity.
    write_pose_line(out, mat4_mul(M, Tr_inv));

    // Sliding-window local map: each entry stores a downsampled scan in its
    // own V_j frame, plus M[j] = T^W_V_j, plus a per-point normal so we can
    // transform both into any frame.
    struct MapFrame {
        std::vector<Vec3> pts;  // in V_j coords
        std::vector<Vec3> nrm;  // normals, same length as pts, in V_j coords
        Mat4              M;    // T^W_V_j
    };
    std::deque<MapFrame> map_buf;

    // Load frame 0.
    char fname[32];
    std::snprintf(fname, sizeof(fname), "%06d.bin", 0);
    std::string p0 = sequence_dir + "/velodyne/" + fname;
    std::vector<Vec3> f0_raw  = read_kitti_bin(p0);
    correct_kitti_scan(f0_raw);
    std::vector<Vec3> f0      = voxel_downsample(f0_raw, VOXEL_SIZE);
    std::vector<Vec3> f0_nrm  = estimate_normals(f0, NORMAL_K);
    map_buf.push_back({f0, f0_nrm, M});  // M = Tr at this point

    // Constant-velocity carryover. For i=1 we have no prior motion, so use
    // identity — same as earlier experiments on that single transition.
    Mat4 prev_delta = Mat4::identity();

    for (long i = 1; i < n_frames; ++i) {
        std::snprintf(fname, sizeof(fname), "%06ld.bin", i);
        std::string p = sequence_dir + "/velodyne/" + fname;
        std::vector<Vec3> curr_raw = read_kitti_bin(p);
        correct_kitti_scan(curr_raw);
        std::vector<Vec3> curr_src = voxel_downsample(curr_raw, VOXEL_SIZE_SRC);
        std::vector<Vec3> curr_map = voxel_downsample(curr_raw, VOXEL_SIZE);

        // Build local-map target. Transform every buffered scan and its normals
        // into the V_{i-1} coordinate system. Points use full T^V_{i-1}_V_j;
        // normals use only the rotation part (no translation).
        Mat4 M_target_inv = mat4_inverse_se3(M);
        std::vector<Vec3> target_map;
        std::vector<Vec3> target_nrm;
        size_t total = 0;
        for (const auto &mf : map_buf) total += mf.pts.size();
        target_map.reserve(total);
        target_nrm.reserve(total);
        for (const auto &mf : map_buf) {
            Mat4 T_target_j = mat4_mul(M_target_inv, mf.M);
            for (size_t k = 0; k < mf.pts.size(); ++k) {
                target_map.push_back(mat4_apply(T_target_j, mf.pts[k]));
                // Rotate normal by R part only.
                const Vec3 &n = mf.nrm[k];
                Vec3 nr;
                nr.x = T_target_j.m[0] * n.x + T_target_j.m[1] * n.y + T_target_j.m[2]  * n.z;
                nr.y = T_target_j.m[4] * n.x + T_target_j.m[5] * n.y + T_target_j.m[6]  * n.z;
                nr.z = T_target_j.m[8] * n.x + T_target_j.m[9] * n.y + T_target_j.m[10] * n.z;
                target_nrm.push_back(nr);
            }
        }

        IcpResult r = icp_p2pl(curr_src, target_map, target_nrm,
                               MAX_DIST_LOOSE, MAX_DIST_TIGHT,
                               MAX_ITER, TOL_T, TOL_R, prev_delta);
        Mat4 delta_M = r.T;
        prev_delta = delta_M;

        M = mat4_mul(M, delta_M);  // M[i] = M[i-1] @ delta_M
        write_pose_line(out, mat4_mul(M, Tr_inv));

        if (i % 200 == 0 || i == n_frames - 1) {
            std::fprintf(stderr, "frame %ld/%ld  map_pts=%zu src=%zu map_curr=%zu  iters=%d corrs=%d window=%zu\n",
                         i, n_frames - 1, total, curr_src.size(), curr_map.size(), r.iters, r.n_corrs, map_buf.size());
        }

        // Estimate normals on the new (map-density) scan, push into buffer.
        std::vector<Vec3> curr_map_nrm = estimate_normals(curr_map, NORMAL_K);
        map_buf.push_back({std::move(curr_map), std::move(curr_map_nrm), M});
        while (map_buf.size() > WINDOW_SIZE) map_buf.pop_front();
    }

    out.close();
    return 0;
}
