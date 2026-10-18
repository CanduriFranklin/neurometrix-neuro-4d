#pragma once
// EntropyMap4D - Local Shannon entropy on 4D volumes.

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>

namespace neurometrix::core4d::domain {

class EntropyMap4D {
public:
    [[nodiscard]] static Eigen::MatrixXf
    compute(const Eigen::MatrixXf& vol,
            std::uint32_t h, std::uint32_t w, std::uint32_t d,
            int window = 5) {
        const std::uint32_t plane = h * w;
        const int r = window / 2;
        Eigen::MatrixXf out = Eigen::MatrixXf::Zero(vol.rows(), 1);

        #pragma omp parallel for
        for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
            const std::uint32_t x = idx % w;
            const std::uint32_t y = (idx / w) % h;
            const std::uint32_t z = idx / plane;

            float lo = 1e30f, hi = -1e30f;
            for (int dz = -r; dz <= r; ++dz) {
                for (int dy = -r; dy <= r; ++dy) {
                    for (int dx = -r; dx <= r; ++dx) {
                        const int nx = static_cast<int>(x) + dx;
                        const int ny = static_cast<int>(y) + dy;
                        const int nz = static_cast<int>(z) + dz;
                        if (nx < 0 || nx >= static_cast<int>(w)) continue;
                        if (ny < 0 || ny >= static_cast<int>(h)) continue;
                        if (nz < 0 || nz >= static_cast<int>(d)) continue;
                        const float v = vol(nz * plane + ny * w + nx);
                        lo = std::min(lo, v);
                        hi = std::max(hi, v);
                    }
                }
            }
            const float range = std::max(1e-6f, hi - lo);
            constexpr int kBins = 16;
            int hist[kBins] = {0};
            int n = 0;
            for (int dz = -r; dz <= r; ++dz) {
                for (int dy = -r; dy <= r; ++dy) {
                    for (int dx = -r; dx <= r; ++dx) {
                        const int nx = static_cast<int>(x) + dx;
                        const int ny = static_cast<int>(y) + dy;
                        const int nz = static_cast<int>(z) + dz;
                        if (nx < 0 || nx >= static_cast<int>(w)) continue;
                        if (ny < 0 || ny >= static_cast<int>(h)) continue;
                        if (nz < 0 || nz >= static_cast<int>(d)) continue;
                        const float v = vol(nz * plane + ny * w + nx);
                        int b = static_cast<int>((v - lo) / range * (kBins - 1));
                        b = std::clamp(b, 0, kBins - 1);
                        hist[b]++;
                        n++;
                    }
                }
            }
            if (n == 0) continue;
            double H = 0.0;
            for (int b = 0; b < kBins; ++b) {
                if (hist[b] == 0) continue;
                const double p = static_cast<double>(hist[b]) / n;
                H -= p * std::log2(p);
            }
            out(idx) = static_cast<float>(H);
        }
        return out;
    }
};

} // namespace neurometrix::core4d::domain