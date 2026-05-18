// Official KITTI odometry evaluation, ported to a self-contained C++17 binary.
//
// Computes translation error % and rotation error deg/m, averaged over
// sub-trajectories of length {100, 200, 300, 400, 500, 600, 700, 800} m,
// across one or more sequences. The aggregation matches the official
// devkit (sum of errors weighted by sub-trajectory count, divided by total
// sub-trajectory count) so numbers are directly comparable to the
// leaderboard at https://www.cvlibs.net/datasets/kitti/eval_odometry.php.
//
// Algorithm reference: Geiger et al., "Are we ready for autonomous driving?
// The KITTI vision benchmark suite", CVPR 2012, devkit_odometry.zip.
//
// Usage:
//   evaluate_odometry <gt_dir> <pred_dir> <seqs...>
//
// Where <seqs...> is a list of two-digit sequence ids (e.g. 00 05 07).
// <gt_dir>/NN.txt and <pred_dir>/NN.txt must both exist for each NN.
//
// Output (stdout, one JSON object per line, deterministic field order):
//   {"seq":"00","trans_pct":0.51,"rot_deg_per_m":0.0021,"n_subtraj":1234,"length_m":3724.187}
//   ...
//   {"seq":"AGG","trans_pct":0.48,"rot_deg_per_m":0.0019,"n_subtraj":4231,"length_m":11487.523}
//
// Exit codes:
//   0 = success
//   1 = arg parse error
//   2 = could not read a required file
//   3 = malformed input (wrong column count, NaN, etc.)
//   4 = pred trajectory line count != gt line count for some sequence

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using Mat34 = std::array<double, 12>; // row-major 3x4: [R(3x3) | t(3x1)]

// 4x4 inverse for SE(3) matrices represented as 3x4 row-major.
// Returns the inverse as a 3x4 row-major matrix.
// inv([R|t]) = [R^T | -R^T * t]
static Mat34 inverseSE3(const Mat34 &m) {
    Mat34 inv{};
    // R^T
    inv[0] = m[0]; inv[1] = m[4]; inv[2] = m[8];
    inv[4] = m[1]; inv[5] = m[5]; inv[6] = m[9];
    inv[8] = m[2]; inv[9] = m[6]; inv[10] = m[10];
    // -R^T * t
    inv[3]  = -(inv[0] * m[3] + inv[1] * m[7] + inv[2]  * m[11]);
    inv[7]  = -(inv[4] * m[3] + inv[5] * m[7] + inv[6]  * m[11]);
    inv[11] = -(inv[8] * m[3] + inv[9] * m[7] + inv[10] * m[11]);
    return inv;
}

// a * b for two 3x4 SE(3) row-major matrices. Treats both as 4x4 with last
// row [0 0 0 1] and returns the upper 3x4 of the product.
static Mat34 multSE3(const Mat34 &a, const Mat34 &b) {
    Mat34 c{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            double s = 0;
            for (int k = 0; k < 3; ++k) s += a[i * 4 + k] * b[k * 4 + j];
            c[i * 4 + j] = s;
        }
        // translation column j=3: a.R * b.t + a.t
        double s = 0;
        for (int k = 0; k < 3; ++k) s += a[i * 4 + k] * b[k * 4 + 3];
        c[i * 4 + 3] = s + a[i * 4 + 3];
    }
    return c;
}

// Translation magnitude of an SE(3) pose error matrix.
static double transError(const Mat34 &errPose) {
    double dx = errPose[3], dy = errPose[7], dz = errPose[11];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// Rotation magnitude (radians) of an SE(3) pose error matrix.
// Uses the trace formula: theta = acos((tr(R) - 1) / 2), clamped.
static double rotError(const Mat34 &errPose) {
    double tr = errPose[0] + errPose[5] + errPose[10];
    double c = 0.5 * (tr - 1.0);
    if (c >  1.0) c =  1.0;
    if (c < -1.0) c = -1.0;
    return std::acos(c);
}

static bool loadPoses(const std::string &path, std::vector<Mat34> &out) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        Mat34 m{};
        for (int i = 0; i < 12; ++i) {
            if (!(iss >> m[i])) return false;
            if (!std::isfinite(m[i])) return false;
        }
        out.push_back(m);
    }
    return true;
}

// Cumulative arc-length along a trajectory of poses.
static std::vector<double> trajectoryLengths(const std::vector<Mat34> &poses) {
    std::vector<double> L(poses.size(), 0.0);
    for (size_t i = 1; i < poses.size(); ++i) {
        double dx = poses[i][3]  - poses[i - 1][3];
        double dy = poses[i][7]  - poses[i - 1][7];
        double dz = poses[i][11] - poses[i - 1][11];
        L[i] = L[i - 1] + std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return L;
}

// For each starting frame i, find the smallest j > i such that L[j] - L[i] >= target_len.
// Returns -1 if no such j exists.
static long lastFrameFromSegmentLength(const std::vector<double> &L, size_t i,
                                       double target_len) {
    for (size_t j = i; j < L.size(); ++j) {
        if (L[j] - L[i] >= target_len) return static_cast<long>(j);
    }
    return -1;
}

struct Errors {
    double trans_err_frac; // (trans error in m) / segment length (so it's a fraction)
    double rot_err_rad_per_m;
    double segment_len_m;
};

// Compute per-sub-trajectory errors for a single sequence.
// Mirrors the official devkit: step every 10 frames, for each length in
// {100..800}m, accumulate one error sample per (start_frame, length) pair.
static std::vector<Errors> calcSequenceErrors(const std::vector<Mat34> &gt,
                                              const std::vector<Mat34> &pred) {
    static const std::array<double, 8> kLengths = {100, 200, 300, 400, 500, 600, 700, 800};
    constexpr int kStep = 10;

    std::vector<Errors> errs;
    if (gt.size() < 2) return errs;

    auto L = trajectoryLengths(gt);
    errs.reserve(gt.size() / kStep * kLengths.size());

    for (size_t i = 0; i < gt.size(); i += kStep) {
        for (double seg : kLengths) {
            long j = lastFrameFromSegmentLength(L, i, seg);
            if (j < 0) continue;
            // pose error = inv(pred_delta) * gt_delta, where
            // gt_delta = inv(gt[i]) * gt[j], pred_delta = inv(pred[i]) * pred[j]
            Mat34 gt_delta   = multSE3(inverseSE3(gt[i]),   gt[static_cast<size_t>(j)]);
            Mat34 pred_delta = multSE3(inverseSE3(pred[i]), pred[static_cast<size_t>(j)]);
            Mat34 err = multSE3(inverseSE3(pred_delta), gt_delta);

            Errors e;
            e.trans_err_frac    = transError(err) / seg;
            e.rot_err_rad_per_m = rotError(err)   / seg;
            e.segment_len_m     = seg;
            errs.push_back(e);
        }
    }
    return errs;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <gt_dir> <pred_dir> <seqs...>\n", argv[0]);
        return 1;
    }
    std::string gt_dir   = argv[1];
    std::string pred_dir = argv[2];

    // Aggregate errors across all sequences.
    std::vector<Errors> agg_errs;
    double agg_length = 0.0;

    for (int ai = 3; ai < argc; ++ai) {
        std::string seq = argv[ai];
        std::string gt_path   = gt_dir   + "/" + seq + ".txt";
        std::string pred_path = pred_dir + "/" + seq + ".txt";

        std::vector<Mat34> gt, pred;
        if (!loadPoses(gt_path, gt)) {
            std::fprintf(stderr, "could not read or parse %s\n", gt_path.c_str());
            return 2;
        }
        if (!loadPoses(pred_path, pred)) {
            std::fprintf(stderr, "could not read or parse %s\n", pred_path.c_str());
            return 2;
        }
        if (gt.size() != pred.size()) {
            std::fprintf(stderr, "line count mismatch on seq %s: gt=%zu pred=%zu\n",
                         seq.c_str(), gt.size(), pred.size());
            return 4;
        }

        auto errs = calcSequenceErrors(gt, pred);
        double sum_t = 0.0, sum_r = 0.0;
        for (const auto &e : errs) { sum_t += e.trans_err_frac; sum_r += e.rot_err_rad_per_m; }
        double n = static_cast<double>(errs.size());
        double trans_pct = (n > 0) ? (sum_t / n) * 100.0 : 0.0;
        double rot_dpm   = (n > 0) ? (sum_r / n) * 180.0 / M_PI : 0.0;
        auto L = trajectoryLengths(gt);
        double length_m = L.empty() ? 0.0 : L.back();

        std::printf("{\"seq\":\"%s\",\"trans_pct\":%.6f,\"rot_deg_per_m\":%.6f,"
                    "\"n_subtraj\":%zu,\"length_m\":%.3f}\n",
                    seq.c_str(), trans_pct, rot_dpm, errs.size(), length_m);

        agg_errs.insert(agg_errs.end(), errs.begin(), errs.end());
        agg_length += length_m;
    }

    double sum_t = 0.0, sum_r = 0.0;
    for (const auto &e : agg_errs) { sum_t += e.trans_err_frac; sum_r += e.rot_err_rad_per_m; }
    double n = static_cast<double>(agg_errs.size());
    double trans_pct = (n > 0) ? (sum_t / n) * 100.0 : 0.0;
    double rot_dpm   = (n > 0) ? (sum_r / n) * 180.0 / M_PI : 0.0;
    std::printf("{\"seq\":\"AGG\",\"trans_pct\":%.6f,\"rot_deg_per_m\":%.6f,"
                "\"n_subtraj\":%zu,\"length_m\":%.3f}\n",
                trans_pct, rot_dpm, agg_errs.size(), agg_length);

    return 0;
}
