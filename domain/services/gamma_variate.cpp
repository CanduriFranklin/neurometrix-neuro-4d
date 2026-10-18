#include "domain/services/gamma_variate.hpp"
#include <algorithm>
#include <cmath>

namespace neurometrix::core4d::domain {

namespace {

inline double gamma_basis(double t, double t0, double alpha, double beta) {
    if (t <= t0) return 0.0;
    const double d = t - t0;
    return std::pow(d, alpha) * std::exp(-d / beta);
}

double solve_A(const Eigen::VectorXf& curve,
               const std::vector<double>& t,
               double t0, double alpha, double beta) {
    double num = 0.0, den = 0.0;
    for (std::size_t i = 0; i < t.size(); ++i) {
        const double b = gamma_basis(t[i], t0, alpha, beta);
        num += static_cast<double>(curve(static_cast<Eigen::Index>(i))) * b;
        den += b * b;
    }
    return (den > 1e-12) ? num / den : 0.0;
}

double residual_of(const Eigen::VectorXf& curve,
                   const std::vector<double>& t,
                   double A, double t0, double alpha, double beta) {
    double r = 0.0;
    for (std::size_t i = 0; i < t.size(); ++i) {
        const double b = gamma_basis(t[i], t0, alpha, beta);
        const double d = static_cast<double>(curve(static_cast<Eigen::Index>(i))) - A * b;
        r += d * d;
    }
    return r;
}

} // anonymous namespace

GammaFitResult fit_gamma_variate(const Eigen::VectorXf& curve, DurationSeconds dt) {
    GammaFitResult out{};
    const Eigen::Index n = curve.size();
    if (n < 4 || dt.value() <= 0.0) {
        out.fitted_curve = curve;
        return out;
    }

    Eigen::Index peak_idx = 0;
    const float peak_val = curve.maxCoeff(&peak_idx);
    if (peak_val <= 0.0f) {
        out.fitted_curve = curve;
        return out;
    }

    std::vector<double> t(static_cast<std::size_t>(n));
    for (Eigen::Index i = 0; i < n; ++i)
        t[static_cast<std::size_t>(i)] = static_cast<double>(i) * dt.value();
    const double t_peak = t[static_cast<std::size_t>(peak_idx)];

    // --- Closed-form moment estimate (analytic init guess) ---
    //   gamma(t) = A * (t - t0)^a * exp(-(t - t0)/b),  t > t0
    //   mean  = t0 + b * (a + 1)
    //   var   = b^2 * (a + 1)
    //   t_peak = t0 + a * b
    // =>  b = mean - t_peak
    //     a = var / b^2 - 1
    //     t0 = t_peak - a * b
    double M0 = 0.0, M1 = 0.0, M2 = 0.0;
    for (std::size_t i = 0; i < t.size(); ++i) {
        const double c = static_cast<double>(curve(static_cast<Eigen::Index>(i)));
        M0 += c;
        M1 += c * t[i];
        M2 += c * t[i] * t[i];
    }

    // Defaults (fallback)
    double t0    = std::max(0.0, t_peak * 0.4);
    double alpha = 3.0;
    double beta  = std::max(dt.value(), t_peak * 0.25);
    double A     = solve_A(curve, t, t0, alpha, beta);
    double best_r = residual_of(curve, t, A, t0, alpha, beta);

    // Try closed-form if moments are well-defined
    if (M0 > 1e-9) {
        const double mu  = M1 / M0;
        const double var = M2 / M0 - mu * mu;
        if (var > 1e-9) {
            const double beta_c = mu - t_peak;
            if (beta_c > 1e-3) {
                const double alpha_c = var / (beta_c * beta_c) - 1.0;
                if (alpha_c > 0.3) {
                    const double t0_c = t_peak - alpha_c * beta_c;
                    if (t0_c >= 0.0) {
                        const double A_c = solve_A(curve, t, t0_c, alpha_c, beta_c);
                        const double r_c = residual_of(curve, t, A_c, t0_c, alpha_c, beta_c);
                        if (r_c < best_r) {
                            t0 = t0_c;
                            alpha = alpha_c;
                            beta = beta_c;
                            A = A_c;
                            best_r = r_c;
                        }
                    }
                }
            }
        }
    }

    // --- Coordinate descent refinement with decaying step ---
    for (int iter = 0; iter < 80; ++iter) {
        bool improved = false;
        const double scale = std::max(0.01, std::pow(0.6, iter / 4.0));

        for (double d : {-2.0, -1.0, -0.5, -0.2, -0.1, -0.05,
                          0.05,  0.1,  0.2,  0.5,  1.0,  2.0}) {
            // t0
            {
                const double cand = std::max(0.0, t0 + d * scale * dt.value());
                const double A_new = solve_A(curve, t, cand, alpha, beta);
                const double r = residual_of(curve, t, A_new, cand, alpha, beta);
                if (r < best_r - 1e-9) { t0 = cand; A = A_new; best_r = r; improved = true; }
            }
            // alpha
            {
                const double cand = std::max(0.3, alpha + d * scale);
                const double A_new = solve_A(curve, t, t0, cand, beta);
                const double r = residual_of(curve, t, A_new, t0, cand, beta);
                if (r < best_r - 1e-9) { alpha = cand; A = A_new; best_r = r; improved = true; }
            }
            // beta
            {
                const double cand = std::max(0.05, beta + d * scale * dt.value());
                const double A_new = solve_A(curve, t, t0, alpha, cand);
                const double r = residual_of(curve, t, A_new, t0, alpha, cand);
                if (r < best_r - 1e-9) { beta = cand; A = A_new; best_r = r; improved = true; }
            }
        }

        if (!improved && iter > 30) break;
    }

    out.fitted_curve = Eigen::VectorXf::Zero(n);
    for (Eigen::Index i = 0; i < n; ++i)
        out.fitted_curve[i] = static_cast<float>(
            A * gamma_basis(t[static_cast<std::size_t>(i)], t0, alpha, beta));

    out.A = A;
    out.t0_seconds = t0;
    out.alpha = alpha;
    out.beta_seconds = beta;
    out.residual = best_r;
    return out;
}

} // namespace neurometrix::core4d::domain
