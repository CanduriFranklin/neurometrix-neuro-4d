#pragma once
// VolumetricMetrics - 4D summary statistics + volume computation.

#include <Eigen/Dense>
#include <cmath>
#include <cstdint>

namespace neurometrix::core4d::domain {

struct VolumeMetrics {
    double mean_hu{0.0};
    double stddev_hu{0.0};
    double min_hu{0.0};
    double max_hu{0.0};
    double mean_entropy{0.0};
    double peak_vesselness{0.0};
    double asymmetry_score{0.0};
    double total_volume_mm3{0.0};
};

class VolumetricMetrics {
public:
    // Compute mean, stddev, min, max over the full volume.
    [[nodiscard]] static VolumeMetrics
    compute(const Eigen::MatrixXf& vol,
            std::uint32_t h, std::uint32_t w, std::uint32_t d,
            float voxel_size_mm = 1.0f) {
        VolumeMetrics m{};
        if (vol.size() == 0) return m;

        m.mean_hu = static_cast<double>(vol.mean());
        m.min_hu  = static_cast<double>(vol.minCoeff());
        m.max_hu  = static_cast<double>(vol.maxCoeff());

        double sq = 0.0;
        for (int i = 0; i < vol.size(); ++i) {
            const double delta = static_cast<double>(vol(i)) - m.mean_hu;
            sq += delta * delta;
        }
        m.stddev_hu = std::sqrt(sq / static_cast<double>(vol.size()));

        const double voxel_mm3 = static_cast<double>(voxel_size_mm) *
                                 static_cast<double>(voxel_size_mm) *
                                 static_cast<double>(voxel_size_mm);
        m.total_volume_mm3 = static_cast<double>(h) *
                             static_cast<double>(w) *
                             static_cast<double>(d) *
                             voxel_mm3;
        return m;
    }

    // Volume of voxels whose HU is within [hu_lo, hu_hi].
    [[nodiscard]] static double
    volume_of_hu_range(const Eigen::MatrixXf& vol,
                       double hu_lo, double hu_hi,
                       float voxel_size_mm = 1.0f) {
        std::uint64_t count = 0;
        #pragma omp parallel for reduction(+:count)
        for (int i = 0; i < static_cast<int>(vol.size()); ++i) {
            const double hu = static_cast<double>(vol(i));
            if (hu >= hu_lo && hu <= hu_hi) ++count;
        }
        const double voxel_mm3 = static_cast<double>(voxel_size_mm) *
                                 static_cast<double>(voxel_size_mm) *
                                 static_cast<double>(voxel_size_mm);
        return static_cast<double>(count) * voxel_mm3;
    }
};

} // namespace neurometrix::core4d::domain