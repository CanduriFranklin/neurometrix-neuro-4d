#pragma once
// BilateralSymmetry4D - Hemispheric asymmetry analysis across the 4D volume.
// Assumes the sagittal midline is at x = width/2.

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace neurometrix::core4d::domain {

struct AsymmetryResult4D {
    double  asymmetry_score;
    int     peak_x;
    int     peak_y;
    int     peak_z;
    double  peak_value;
};

class BilateralSymmetry4D {
public:
    [[nodiscard]] static AsymmetryResult4D
    analyze(const Eigen::MatrixXf& vol,
            std::uint32_t h, std::uint32_t w, std::uint32_t d) {
        const std::uint32_t plane = h * w;
        const std::uint32_t mid = w / 2;

        AsymmetryResult4D r{};
        r.asymmetry_score = 0.0;
        r.peak_x = 0; r.peak_y = 0; r.peak_z = 0; r.peak_value = 0.0;

        if (w < 4 || h < 4 || d < 1) return r;

        double total = 0.0;
        double peak = 0.0;
        int px = 0, py = 0, pz = 0;

        #pragma omp parallel for reduction(+:total)
        for (int z = 0; z < static_cast<int>(d); ++z) {
            double local_peak = 0.0;
            int lx = 0, ly = 0, lz = z;
            for (std::uint32_t y = 0; y < h; ++y) {
                for (std::uint32_t x = 0; x < mid; ++x) {
                    const std::uint32_t mx = w - 1 - x;
                    const std::uint32_t i1 = z * plane + y * w + x;
                    const std::uint32_t i2 = z * plane + y * w + mx;
                    const double v1 = static_cast<double>(vol(i1));
                    const double v2 = static_cast<double>(vol(i2));
                    if (v1 < -200.0 || v1 > 400.0) continue;
                    if (v2 < -200.0 || v2 > 400.0) continue;
                    const double diff = std::abs(v1 - v2);
                    total += diff;
                    if (diff > local_peak) {
                        local_peak = diff;
                        lx = static_cast<int>(x);
                        ly = static_cast<int>(y);
                        lz = z;
                    }
                }
            }
            #pragma omp critical
            {
                if (local_peak > peak) {
                    peak = local_peak;
                    px = lx; py = ly; pz = lz;
                }
            }
        }

        const double denom = static_cast<double>(h) *
                             static_cast<double>(mid) *
                             static_cast<double>(d);
        r.asymmetry_score = denom > 0 ? total / denom : 0.0;
        r.peak_x = px; r.peak_y = py; r.peak_z = pz;
        r.peak_value = peak;
        return r;
    }
};

} // namespace neurometrix::core4d::domain