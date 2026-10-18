#pragma once
// PhysicsFilters4D - Total Variation + Perona-Malik on 4D volumes.
// Layout: flattened (H*W*D) x 1, Z-major index: z*H*W + y*W + x

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>

namespace neurometrix::core4d::domain {

class PhysicsFilters4D {
public:
    // Chambolle-Pock TV denoising (4D).
    [[nodiscard]] static Eigen::MatrixXf
    total_variation_denoise(const Eigen::MatrixXf& vol,
                             std::uint32_t h, std::uint32_t w, std::uint32_t d,
                             float lambda = 0.1f, int iters = 30) {
        const std::uint32_t plane = h * w;
        Eigen::MatrixXf u = vol;
        Eigen::MatrixXf ubar = vol;
        Eigen::MatrixXf px = Eigen::MatrixXf::Zero(vol.rows(), 1);
        Eigen::MatrixXf py = Eigen::MatrixXf::Zero(vol.rows(), 1);
        Eigen::MatrixXf pz = Eigen::MatrixXf::Zero(vol.rows(), 1);
        const float tau = 0.2f, sigma = 0.2f;

        for (int it = 0; it < iters; ++it) {
            #pragma omp parallel for
            for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
                const std::uint32_t x = idx % w;
                const std::uint32_t y = (idx / w) % h;
                const std::uint32_t z = idx / plane;

                const float gx = (x + 1 < w) ? ubar(idx + 1) - ubar(idx) : 0.f;
                const float gy = (y + 1 < h) ? ubar(idx + w) - ubar(idx) : 0.f;
                const float gz = (z + 1 < d) ? ubar(idx + plane) - ubar(idx) : 0.f;

                float nx = px(idx) + sigma * gx;
                float ny = py(idx) + sigma * gy;
                float nz = pz(idx) + sigma * gz;
                const float nrm = std::max(1.0f,
                    std::sqrt(nx*nx + ny*ny + nz*nz) / std::max(lambda, 1e-6f));
                px(idx) = nx / nrm;
                py(idx) = ny / nrm;
                pz(idx) = nz / nrm;
            }

            Eigen::MatrixXf u_new = u;
            #pragma omp parallel for
            for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
                const std::uint32_t x = idx % w;
                const std::uint32_t y = (idx / w) % h;
                const std::uint32_t z = idx / plane;

                const float dx = px(idx) - (x > 0 ? px(idx - 1) : 0.f);
                const float dy = py(idx) - (y > 0 ? py(idx - w) : 0.f);
                const float dz = pz(idx) - (z > 0 ? pz(idx - plane) : 0.f);
                u_new(idx) = u(idx) + tau * (dx + dy + dz);
            }
            ubar = 2.f * u_new - u;
            u = u_new;
        }
        return u;
    }

    // Perona-Malik 4D.
    [[nodiscard]] static Eigen::MatrixXf
    perona_malik(const Eigen::MatrixXf& vol,
                  std::uint32_t h, std::uint32_t w, std::uint32_t d,
                  float kappa = 25.0f, int iters = 15, float dt = 0.15f) {
        const std::uint32_t plane = h * w;
        Eigen::MatrixXf u = vol;
        auto g = [kappa](float s) { return 1.f / (1.f + (s*s)/(kappa*kappa)); };

        for (int it = 0; it < iters; ++it) {
            Eigen::MatrixXf u_new = u;
            #pragma omp parallel for
            for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
                const std::uint32_t x = idx % w;
                const std::uint32_t y = (idx / w) % h;
                const std::uint32_t z = idx / plane;

                if (x == 0 || y == 0 || z == 0 ||
                    x + 1 >= w || y + 1 >= h || z + 1 >= d) continue;

                const float dxm = u(idx - 1)      - u(idx);
                const float dxp = u(idx + 1)      - u(idx);
                const float dym = u(idx - w)      - u(idx);
                const float dyp = u(idx + w)      - u(idx);
                const float dzm = u(idx - plane)  - u(idx);
                const float dzp = u(idx + plane)  - u(idx);

                u_new(idx) = u(idx) + dt * (
                    g(std::abs(dxm)) * dxm + g(std::abs(dxp)) * dxp +
                    g(std::abs(dym)) * dym + g(std::abs(dyp)) * dyp +
                    g(std::abs(dzm)) * dzm + g(std::abs(dzp)) * dzp);
            }
            u = u_new;
        }
        return u;
    }
};

} // namespace neurometrix::core4d::domain