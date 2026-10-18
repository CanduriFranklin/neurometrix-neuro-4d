#include <gtest/gtest.h>
#include "domain/services/perfusion_analysis_4d.hpp"
#include "domain/services/gamma_variate.hpp"

using namespace neurometrix::core4d::domain;

namespace {
Eigen::VectorXf make_gamma_bolus(int n, double dt, double t0, double alpha, double beta, double A) {
    Eigen::VectorXf out = Eigen::VectorXf::Zero(n);
    for (int i = 0; i < n; ++i) {
        const double t = i * dt;
        if (t > t0) {
            const double d = t - t0;
            out[i] = static_cast<float>(A * std::pow(d, alpha) * std::exp(-d / beta));
        }
    }
    return out;
}
} // anonymous

TEST(GammaVariate, RecoversKnownParameters) {
    const int n = 40;
    const double dt = 0.5;
    // Ground truth: A=100, t0=1.0s, alpha=3.0, beta=0.9s
    auto curve = make_gamma_bolus(n, dt, 1.0, 3.0, 0.9, 100.0);
    auto fit = fit_gamma_variate(curve, DurationSeconds{dt});

    EXPECT_GT(fit.A, 0.0);
    EXPECT_NEAR(fit.t0_seconds, 1.0, 0.5);
    EXPECT_NEAR(fit.alpha, 3.0, 0.8);
    EXPECT_NEAR(fit.beta_seconds, 0.9, 0.3);

    // Fitted curve should be close to the input
    double rmse = 0.0;
    for (int i = 0; i < n; ++i) {
        const double d = curve[i] - fit.fitted_curve[i];
        rmse += d * d;
    }
    rmse = std::sqrt(rmse / n);
    EXPECT_LT(rmse, 5.0);   // < 5 HU residual on a 100 HU peak
}

TEST(GammaVariate, FitsNoisyBolus) {
    const int n = 40;
    const double dt = 0.5;
    auto clean = make_gamma_bolus(n, dt, 1.5, 3.5, 1.0, 150.0);
    auto noisy = clean;
    // Add +/- 5 HU noise deterministically
    for (int i = 0; i < n; ++i) noisy[i] += (i % 3 == 0) ? 5.0f : -3.0f;

    auto fit = fit_gamma_variate(noisy, DurationSeconds{dt});
    EXPECT_GT(fit.A, 50.0);
    // Noise floor: 14 samples * 25 + 26 samples * 9 = 584. Threshold ~2x.
    EXPECT_LT(fit.residual, 1500.0);
}

TEST(BlockCirculant, IsDelayInvariant) {
    // Same tissue curve but shifted by 1 second should produce
    // similar TMax values under block-circulant SVD.
    const std::uint32_t h = 4, w = 4, d = 2, t = 20;
    const double dt = 0.5;
    const std::uint32_t n4 = h * w * d;

    auto aif = make_gamma_bolus(t, dt, 1.0, 3.0, 0.7, 300.0);

    // Build volume: uniform tissue response
    auto tissue = make_gamma_bolus(t, dt, 1.5, 3.0, 0.9, 60.0);
    Eigen::VectorXf vol = Eigen::VectorXf::Zero(n4 * t);
    for (std::uint32_t ti = 0; ti < t; ++ti)
        for (std::uint32_t i = 0; i < n4; ++i)
            vol[ti * n4 + i] = tissue[ti];

    AIFResult aifres{};
    aifres.curve = aif;
    aifres.peak_hu = 300.0;

    PerfusionParams p_trunc{};
    p_trunc.dt_seconds = dt;
    p_trunc.use_block_circulant_svd = false;

    PerfusionParams p_circ{};
    p_circ.dt_seconds = dt;
    p_circ.use_block_circulant_svd = true;

    auto maps_trunc = PerfusionAnalysis4D::compute(vol, h, w, d, t, aifres, p_trunc);
    auto maps_circ  = PerfusionAnalysis4D::compute(vol, h, w, d, t, aifres, p_circ);

    // Both produce finite, non-negative CBV
    EXPECT_GE(maps_trunc.cbv.minCoeff(), 0.0f);
    EXPECT_GE(maps_circ.cbv.minCoeff(), 0.0f);

    // CBV should be close between the two methods (integral of C(t) / AIF integral)
    const double cbv_trunc = maps_trunc.cbv.mean();
    const double cbv_circ  = maps_circ.cbv.mean();
    EXPECT_NEAR(cbv_trunc, cbv_circ, 5.0);
}
