#pragma once
// Gamma-variate fit for arterial input function / tissue curves.
//
//   C(t) = A * (t - t0)^alpha * exp(-(t - t0)/beta)   for t > t0
//   C(t) = 0                                          otherwise
//
// Fitted via coordinate descent on (t0, alpha, beta) with A solved
// linearly. Robust to noisy bolus; removes double-peak artifacts that
// plague raw AIF detection.

#include "domain/value_objects/strong_time.hpp"
#include <Eigen/Dense>

namespace neurometrix::core4d::domain {

struct GammaFitResult {
    double A{0.0};
    double t0_seconds{0.0};
    double alpha{0.0};
    double beta_seconds{0.0};
    Eigen::VectorXf fitted_curve;
    double residual{0.0};
};

[[nodiscard]] GammaFitResult fit_gamma_variate(const Eigen::VectorXf& curve,
                                                DurationSeconds dt);

} // namespace neurometrix::core4d::domain
