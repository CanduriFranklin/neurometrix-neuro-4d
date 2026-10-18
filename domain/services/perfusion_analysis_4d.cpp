#include "domain/services/perfusion_analysis_4d.hpp"
#include "domain/services/gamma_variate.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace neurometrix::core4d::domain {

Eigen::VectorXf& PerfusionMaps::select(const std::string& name) {
    if (name == "CBF")  return cbf;
    if (name == "CBV")  return cbv;
    if (name == "MTT")  return mtt;
    if (name == "TTP")  return ttp;
    if (name == "TMax") return tmax;
    throw std::invalid_argument("PerfusionMaps::select unknown " + name);
}

namespace {

// Baseline estimation via low percentile (median of the lowest pct samples).
Eigen::VectorXf baseline_subtract(const Eigen::VectorXf& c, double pct) {
    const Eigen::Index n = c.size();
    const Eigen::Index k = std::max<Eigen::Index>(1,
        static_cast<Eigen::Index>(n * pct));
    std::vector<float> head(c.data(), c.data() + k);
    std::nth_element(head.begin(), head.begin() + k / 2, head.end());
    const float base = head[k / 2];
    Eigen::VectorXf out(n);
    for (Eigen::Index i = 0; i < n; ++i)
        out[i] = std::max(0.0f, c[i] - base);
    return out;
}

// Oscillatory index (Wu 2003): oscillations of the residue function R(t)
// after the peak. If the residue oscillates beyond threshold, add more
// truncation to dampen them.
double oscillatory_index(const Eigen::VectorXf& r) {
    if (r.size() < 3) return 0.0;
    const float rmax = r.maxCoeff();
    if (rmax <= 1e-6f) return 0.0;
    double osc = 0.0;
    for (Eigen::Index i = 1; i < r.size(); ++i)
        osc += std::abs(r[i] - r[i - 1]);
    return osc / (2.0 * static_cast<double>(rmax));
}

// Truncated SVD solve with per-voxel adaptive truncation via oscillatory
// index. Preserves the singular value decomposition precomputed on the
// shared Toeplitz matrix A (i.e. does not recompute SVD per voxel).
Eigen::VectorXf svd_solve_adaptive(const Eigen::MatrixXf& V,
                                    const Eigen::MatrixXf& U,
                                    const Eigen::VectorXf& S,
                                    const Eigen::VectorXf& c,
                                    double base_trunc,
                                    double osc_threshold) {
    const Eigen::Index n = S.size();
    const float smax = (n > 0) ? S[0] : 1.0f;
    const float tol = 1e-6f * smax;

    // Start with the base truncation.
    Eigen::Index k = std::max<Eigen::Index>(1,
        static_cast<Eigen::Index>(n * (1.0 - base_trunc)));

    auto build_r = [&](Eigen::Index kk) {
        Eigen::VectorXf s_inv = Eigen::VectorXf::Zero(n);
        for (Eigen::Index i = 0; i < kk && i < n; ++i)
            if (S[i] > tol) s_inv[i] = 1.0f / S[i];
        return Eigen::VectorXf(V * s_inv.asDiagonal() * U.transpose() * c);
    };

    Eigen::VectorXf r = build_r(k);

    // Adaptive: if oscillatory index too high, truncate further.
    const double osc = oscillatory_index(r);
    if (osc > osc_threshold && k > 1) {
        k = std::max<Eigen::Index>(1, k / 2);
        r = build_r(k);
    }
    return r;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Arterial Input Function detection.
// Score = max positive rise rate, biased toward early peaks.
// Candidate voxels must reach [hu_min, hu_max] at peak.
// ---------------------------------------------------------------------------
AIFResult ArterialInput::detect(const Eigen::VectorXf& vol4d,
                                 std::uint32_t h, std::uint32_t w,
                                 std::uint32_t d, std::uint32_t t,
                                 double dt, double hu_min, double hu_max) {
    AIFResult best{};
    const std::uint32_t plane = h * w;
    const std::uint32_t vol = plane * d;
    double best_score = -1.0;

    for (std::uint32_t z = 0; z < d; ++z)
    for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < w; ++x) {
        const std::uint32_t sp = z * plane + y * w + x;
        double peak = -1e9, tp = 0.0;
        for (std::uint32_t ti = 0; ti < t; ++ti) {
            const float v = vol4d[static_cast<Eigen::Index>(ti) * vol + sp];
            if (v > peak) { peak = v; tp = ti * dt; }
        }
        if (peak < hu_min || peak > hu_max) continue;

        double rise = 0.0;
        for (std::uint32_t ti = 1; ti < t; ++ti) {
            const double v1 = vol4d[static_cast<Eigen::Index>(ti) * vol + sp];
            const double v0 = vol4d[static_cast<Eigen::Index>(ti - 1) * vol + sp];
            const double r = (v1 - v0) / std::max(dt, 1e-6);
            if (r > rise) rise = r;
        }
        const double score = rise / std::max(tp, 0.5);
        if (score > best_score) {
            best_score = score;
            best.voxel_x = x; best.voxel_y = y; best.voxel_z = z;
            best.peak_hu = peak; best.time_to_peak_s = tp;
        }
    }

    best.curve = Eigen::VectorXf::Zero(static_cast<Eigen::Index>(t));
    if (best_score > 0.0) {
        const std::uint32_t sp = best.voxel_z * plane + best.voxel_y * w + best.voxel_x;
        for (std::uint32_t ti = 0; ti < t; ++ti)
            best.curve[static_cast<Eigen::Index>(ti)] =
                vol4d[static_cast<Eigen::Index>(ti) * vol + sp];
    }
    return best;
}

// ---------------------------------------------------------------------------
// Perfusion computation (SVD deconvolution + clinical unit conversion).
// ---------------------------------------------------------------------------
PerfusionMaps PerfusionAnalysis4D::compute(const Eigen::VectorXf& vol4d,
                                            std::uint32_t h, std::uint32_t w,
                                            std::uint32_t d, std::uint32_t t,
                                            const AIFResult& aif,
                                            const PerfusionParams& p) {
    if (h == 0 || w == 0 || d == 0 || t == 0)
        throw std::invalid_argument("PerfusionAnalysis4D: zero dims");
    if (static_cast<Eigen::Index>(t) != aif.curve.size())
        throw std::invalid_argument("PerfusionAnalysis4D: AIF size mismatch");

    const std::uint32_t plane = h * w;
    const std::uint32_t vol = plane * d;
    const Eigen::Index n = plane * d;

    PerfusionMaps maps{};
    maps.h = h; maps.w = w; maps.d = d;
    maps.cbf  = Eigen::VectorXf::Zero(n);
    maps.cbv  = Eigen::VectorXf::Zero(n);
    maps.mtt  = Eigen::VectorXf::Zero(n);
    maps.ttp  = Eigen::VectorXf::Zero(n);
    maps.tmax = Eigen::VectorXf::Zero(n);

    // --- Optional: gamma-variate fit of the AIF ---
    Eigen::VectorXf aif_curve = aif.curve;
    if (p.fit_aif_gamma_variate) {
        const auto fit = fit_gamma_variate(aif.curve, DurationSeconds{p.dt_seconds});
        if (fit.A > 0.0 && fit.fitted_curve.size() == aif.curve.size()) {
            aif_curve = fit.fitted_curve;
        }
    }

    // --- AIF integral (denominator of CBV) ---
    const double aif_integral = static_cast<double>(aif_curve.sum()) * p.dt_seconds;
    if (aif_integral <= 0.0) return maps;   // no bolus -> zero maps

    // --- Hematocrit correction (Fahraeus) ---
    const double hct_corr = p.apply_hematocrit_correction
        ? (1.0 - p.hematocrit_large_vessel) / (1.0 - p.hematocrit_small_vessel)
        : 1.0;

    // --- Build the deconvolution operator ---
    // Standard: T x T lower-triangular Toeplitz.
    // Block-circulant: 2T x 2T circulant (Wu 2003), delay-invariant.
    const std::uint32_t Tc = p.use_block_circulant_svd ? 2 * t : t;

    Eigen::MatrixXf A = Eigen::MatrixXf::Zero(Tc, Tc);
    if (p.use_block_circulant_svd) {
        // AIF padded with T zeros
        Eigen::VectorXf aif_pad = Eigen::VectorXf::Zero(Tc);
        for (std::uint32_t i = 0; i < t; ++i) aif_pad[i] = aif_curve[i];
        // Circulant: A(i,j) = aif_pad[(i - j) mod Tc] * dt
        for (std::uint32_t i = 0; i < Tc; ++i) {
            for (std::uint32_t j = 0; j < Tc; ++j) {
                const std::int32_t k =
                    (static_cast<std::int32_t>(i) - static_cast<std::int32_t>(j)) % static_cast<std::int32_t>(Tc);
                const std::uint32_t idx = static_cast<std::uint32_t>((k + static_cast<std::int32_t>(Tc)) % static_cast<std::int32_t>(Tc));
                A(i, j) = aif_pad[idx] * static_cast<float>(p.dt_seconds);
            }
        }
    } else {
        for (std::uint32_t i = 0; i < t; ++i)
            for (std::uint32_t j = 0; j <= i; ++j)
                A(i, j) = aif_curve[j] * static_cast<float>(p.dt_seconds);
    }

    // --- SVD of A, shared across all voxels ---
    Eigen::JacobiSVD<Eigen::MatrixXf> svd(A, Eigen::ComputeThinU | Eigen::ComputeThinV);
    const Eigen::VectorXf S = svd.singularValues();
    const Eigen::MatrixXf U = svd.matrixU();
    const Eigen::MatrixXf V = svd.matrixV();

    const double osc_threshold = 0.10;

    // --- Per-voxel loop ---
    for (std::uint32_t z = 0; z < d; ++z)
    for (std::uint32_t y = 0; y < h; ++y)
    for (std::uint32_t x = 0; x < w; ++x) {
        const std::uint32_t sp = z * plane + y * w + x;

        // 1. Extract and baseline-subtract C(t)
        Eigen::VectorXf c(static_cast<Eigen::Index>(t));
        for (std::uint32_t ti = 0; ti < t; ++ti)
            c[static_cast<Eigen::Index>(ti)] =
                vol4d[static_cast<Eigen::Index>(ti) * vol + sp];
        const Eigen::VectorXf cs = baseline_subtract(c, p.hu_baseline_percentile);

        // 2. SVD deconvolution -> residue function R(t)
        Eigen::VectorXf r;
        if (p.use_block_circulant_svd) {
            Eigen::VectorXf c_pad = Eigen::VectorXf::Zero(Tc);
            for (std::uint32_t ti = 0; ti < t; ++ti) c_pad[ti] = cs[ti];
            const Eigen::VectorXf r_pad = svd_solve_adaptive(
                V, U, S, c_pad, p.svd_truncation, osc_threshold);
            r = r_pad.head(static_cast<Eigen::Index>(t));
        } else {
            r = svd_solve_adaptive(V, U, S, cs, p.svd_truncation, osc_threshold);
        }

        // 3. CBV = ∫C dt / ∫AIF dt   (dimensionless ratio, mL blood / mL tissue)
        const double cbv_ratio =
            static_cast<double>(cs.sum()) * p.dt_seconds / aif_integral;

        // 4. CBF = max(R) (units 1/s) -> mL/100g/min
        //    factor: 60 [s/min] * 100 [mL/100g] / rho [g/mL]
        const double cbf_raw = static_cast<double>(r.maxCoeff());
        const double cbf = cbf_raw * 60.0 * 100.0 / p.tissue_density_g_per_ml * hct_corr;

        // 5. CBV -> mL/100g : ratio * 100 [mL/100g] * rho [g/mL]
        const double cbv = cbv_ratio * 100.0 * p.tissue_density_g_per_ml * hct_corr;

        // 6. MTT = CBV / CBF  (central volume theorem, in seconds)
        //    CBV[mL/100g] / CBF[mL/100g/min] -> minutes -> *60 -> seconds
        const double mtt = (cbf > 1e-6) ? (cbv / cbf) * 60.0 : 0.0;

        // 7. TTP = argmax C(t) * dt
        Eigen::Index ttp_i = 0; cs.maxCoeff(&ttp_i);
        const double ttp = static_cast<double>(ttp_i) * p.dt_seconds;

        // 8. TMax = argmax R(t) * dt
        Eigen::Index tmax_i = 0; r.maxCoeff(&tmax_i);
        const double tmax = static_cast<double>(tmax_i) * p.dt_seconds;

        maps.cbf[sp]  = static_cast<float>(cbf);
        maps.cbv[sp]  = static_cast<float>(cbv);
        maps.mtt[sp]  = static_cast<float>(mtt);
        maps.ttp[sp]  = static_cast<float>(ttp);
        maps.tmax[sp] = static_cast<float>(tmax);
    }

    return maps;
}

} // namespace neurometrix::core4d::domain
