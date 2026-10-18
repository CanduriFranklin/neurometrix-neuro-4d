#pragma once
// PerfusionAnalysis4D - Deconvolution-based CT/MR perfusion.
//
// Method: SVD deconvolution of the tissue concentration curve C(t) by the
// arterial input function AIF(t), per Ostergaard 1996 and Konstas 2009.
//
//   C(t) = CBF * (AIF ⊗ R)(t)
//
// Outputs (per voxel):
//   CBF  = Cerebral Blood Flow   [mL/100g/min]
//   CBV  = Cerebral Blood Volume [mL/100g]
//   MTT  = Mean Transit Time     [s]        (central volume theorem)
//   TTP  = Time To Peak of C(t)  [s]
//   TMax = Time to max of R(t)   [s]
//
// Physics constants (brain tissue):
//   rho    = 1.04 g/mL   tissue density
//   Hct_LV = 0.45        large-vessel hematocrit
//   Hct_SV = 0.25        small-vessel hematocrit (Fahraeus effect)
//   correction = (1 - Hct_LV) / (1 - Hct_SV) ≈ 0.733

#include <Eigen/Dense>
#include <cstdint>
#include <string>

namespace neurometrix::core4d::domain {

struct AIFResult {
    std::uint32_t voxel_x{0}, voxel_y{0}, voxel_z{0};
    double peak_hu{0.0}, time_to_peak_s{0.0};
    Eigen::VectorXf curve;
};

struct PerfusionMaps {
    std::uint32_t h{0}, w{0}, d{0};
    Eigen::VectorXf cbf, cbv, mtt, ttp, tmax;
    Eigen::VectorXf& select(const std::string& name);
};

struct PerfusionParams {
    double dt_seconds{1.0};
    double tissue_density_g_per_ml{1.04};
    double hematocrit_large_vessel{0.45};
    double hematocrit_small_vessel{0.25};
    double svd_truncation{0.15};
    double hu_baseline_percentile{0.20};
    bool   apply_hematocrit_correction{true};

    // --- Advanced options (Wu 2003) ---
    // Block-circulant SVD removes the time-delay artifact that
    // truncated SVD introduces when the bolus arrives at different
    // times in different voxels (e.g. left vs right hemisphere).
    bool   use_block_circulant_svd{false};

    // Fit a gamma-variate to the AIF before deconvolution. Removes
    // double-peak artifacts from raw AIF detection.
    bool   fit_aif_gamma_variate{false};
};

class ArterialInput {
public:
    static AIFResult detect(const Eigen::VectorXf& vol4d,
                            std::uint32_t h, std::uint32_t w,
                            std::uint32_t d, std::uint32_t t,
                            double dt_seconds,
                            double hu_min = 80.0, double hu_max = 500.0);
};

class PerfusionAnalysis4D {
public:
    static PerfusionMaps compute(const Eigen::VectorXf& vol4d,
                                 std::uint32_t h, std::uint32_t w,
                                 std::uint32_t d, std::uint32_t t,
                                 const AIFResult& aif,
                                 const PerfusionParams& params);
};

} // namespace neurometrix::core4d::domain
