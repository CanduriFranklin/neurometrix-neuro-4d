#include "domain/services/physics_filters_4d.hpp"
#include "domain/services/morphological_ops_4d.hpp"
#include "domain/services/bilateral_symmetry_4d.hpp"
#include "domain/services/entropy_map_4d.hpp"
#include "domain/services/volumetric_metrics.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <Eigen/Dense>
#include <vector>

using namespace neurometrix::core4d::domain;
using namespace std::chrono;

static Eigen::MatrixXf make_volume(int h, int w, int d) {
    Eigen::MatrixXf m(h * w * d, 1);
    for (int i = 0; i < m.rows(); ++i) {
        m(i) = 40.0f + 20.0f * std::sin(0.05f * i);
    }
    return m;
}

static double pct(const std::vector<double>& v, double p) {
    if (v.empty()) return 0.0;
    const std::size_t i = static_cast<std::size_t>((v.size() - 1) * p / 100.0);
    return v[i];
}

int main() {
    constexpr int kIter = 5;
    struct Shape { int h, w, d; const char* label; };
    const Shape shapes[] = {
        {128, 128, 64,  "128x128x64"},
        {256, 256, 128, "256x256x128"},
    };

    for (const auto& shape : shapes) {
        const auto vol = make_volume(shape.h, shape.w, shape.d);
        std::vector<double> times;
        times.reserve(kIter);

        for (int it = 0; it < kIter; ++it) {
            const auto t0 = steady_clock::now();
            const auto tv = PhysicsFilters4D::total_variation_denoise(
                vol, shape.h, shape.w, shape.d, 0.1f, 5);
            const auto th = MorphologicalOps4D::white_tophat(
                tv, shape.h, shape.w, shape.d, 2);
            const auto sy = BilateralSymmetry4D::analyze(
                tv, shape.h, shape.w, shape.d);
            const auto en = EntropyMap4D::compute(
                tv, shape.h, shape.w, shape.d, 5);
            volatile float sink = th(0) + (float)sy.asymmetry_score + en(0);
            (void)sink;
            const auto t1 = steady_clock::now();
            times.push_back(duration_cast<microseconds>(t1 - t0).count() / 1000.0);
        }
        std::sort(times.begin(), times.end());

        std::printf("=== %s (voxels=%d) ===\n",
                    shape.label, shape.h * shape.w * shape.d);
        std::printf("  p50 = %8.2f ms\n", pct(times, 50));
        std::printf("  p95 = %8.2f ms\n", pct(times, 95));
        std::printf("  max = %8.2f ms\n", times.back());
    }
    return 0;
}