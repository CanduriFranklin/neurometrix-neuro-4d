#include "domain/services/hemodynamic_maps.hpp"
#include <algorithm>
namespace neurometrix::core4d::domain {
MismatchResult HemodynamicMaps::classify(const PerfusionMaps& m, const MismatchThresholds& thr) {
    const Eigen::Index n = m.cbf.size();
    MismatchResult out{};
    out.tissue_class = Eigen::VectorXf::Zero(n);
    std::vector<float> s(m.cbv.data(), m.cbv.data() + n);
    std::nth_element(s.begin(), s.begin() + n/2, s.end());
    const float cref = s[n/2];
    const float ccore = cref * static_cast<float>(thr.cbv_core_ratio);
    for (Eigen::Index i = 0; i < n; ++i) {
        if (m.tmax[i] > thr.tmax_threshold_s) {
            if (m.cbv[i] < ccore) { out.tissue_class[i] = static_cast<float>(TissueClass::Core); ++out.core_voxels; }
            else { out.tissue_class[i] = static_cast<float>(TissueClass::Penumbra); ++out.penumbra_voxels; }
        }
    }
    out.mismatch_ratio = out.core_voxels > 0
        ? static_cast<double>(out.penumbra_voxels) / static_cast<double>(out.core_voxels) : 0.0;
    return out;
}
}
