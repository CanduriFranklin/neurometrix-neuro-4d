#pragma once
#include <Eigen/Dense>
#include <cstdint>
#include "domain/services/perfusion_analysis_4d.hpp"
namespace neurometrix::core4d::domain {
enum class TissueClass : std::uint8_t { Normal = 0, Penumbra = 1, Core = 2 };
struct MismatchResult {
    Eigen::VectorXf tissue_class;
    std::uint64_t core_voxels{0}, penumbra_voxels{0};
    double mismatch_ratio{0.0};
};
struct MismatchThresholds { double tmax_threshold_s{6.0}; double cbv_core_ratio{0.30}; };
class HemodynamicMaps {
public:
    static MismatchResult classify(const PerfusionMaps& maps, const MismatchThresholds& thr = {});
};
}
