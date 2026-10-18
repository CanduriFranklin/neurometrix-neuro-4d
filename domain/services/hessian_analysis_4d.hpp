#pragma once
// HessianAnalysis4D - Frangi vesselness on 4D volumes.
// Simplified: uses 3 diagonal Hessian components, no full 3x3 eigenvalues.

#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <vector>

namespace neurometrix::core4d::domain {

class HessianAnalysis4D {
public:
    [[nodiscard]] static Eigen::MatrixXf
    frangi_vesselness(const Eigen::MatrixXf& vol,
                       std::uint32_t h, std::uint32_t w, std::uint32_t d,
                       float sigma_min = 1.0f, float sigma_max = 3.0f,
                       int num_sigmas = 3) {
        const std::uint32_t plane = h * w;
        Eigen::MatrixXf response = Eigen::MatrixXf::Zero(vol.rows(), 1);

        for (int s = 0; s < num_sigmas; ++s) {
            const float sigma = sigma_min +
                (sigma_max - sigma_min) * s / std::max(1, num_sigmas - 1);

            const Eigen::MatrixXf Dxx = second_xx(vol, h, w, d, sigma);
            const Eigen::MatrixXf Dyy = second_yy(vol, h, w, d, sigma);
            const Eigen::MatrixXf Dzz = second_zz(vol, h, w, d, sigma);

            #pragma omp parallel for
            for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
                const float dxx = Dxx(idx);
                const float dyy = Dyy(idx);
                const float dzz = Dzz(idx);
                const float tr  = dxx + dyy + dzz;
                // Simplified magnitude measure: sum of squared Hessian diag
                const float S = std::sqrt(dxx*dxx + dyy*dyy + dzz*dzz);
                // Vesselness approximation (Sato-style)
                const float v = (1.f - std::exp(-(S*S) / (2.f * 0.5f * 0.5f)))
                              * (tr < 0.f ? 1.f : 0.f);   // bright on dark
                if (v > response(idx)) response(idx) = v;
            }
        }
        return response;
    }

private:
    [[nodiscard]] static Eigen::MatrixXf
    gaussian_blur_3d(const Eigen::MatrixXf& vol,
                     std::uint32_t h, std::uint32_t w, std::uint32_t d,
                     float sigma) {
        const std::uint32_t plane = h * w;
        const int r = std::max(1, static_cast<int>(std::ceil(2.0f * sigma)));

        std::vector<float> k(2*r + 1);
        float sum = 0.f;
        for (int i = -r; i <= r; ++i) {
            k[i+r] = std::exp(-(i*i) / (2.f*sigma*sigma));
            sum += k[i+r];
        }
        for (auto& v : k) v /= sum;

        // X pass
        Eigen::MatrixXf a(vol.rows(), 1);
        #pragma omp parallel for
        for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
            const std::uint32_t x = idx % w;
            float acc = 0.f;
            for (int dx = -r; dx <= r; ++dx) {
                const int nx = std::clamp(static_cast<int>(x) + dx, 0, static_cast<int>(w) - 1);
                acc += vol(idx - x + nx) * k[dx + r];
            }
            a(idx) = acc;
        }
        // Y pass
        Eigen::MatrixXf b(vol.rows(), 1);
        #pragma omp parallel for
        for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
            const std::uint32_t y = (idx / w) % h;
            float acc = 0.f;
            for (int dy = -r; dy <= r; ++dy) {
                const int ny = std::clamp(static_cast<int>(y) + dy, 0, static_cast<int>(h) - 1);
                acc += a(idx - (y - ny) * w) * k[dy + r];
            }
            b(idx) = acc;
        }
        // Z pass
        Eigen::MatrixXf c(vol.rows(), 1);
        #pragma omp parallel for
        for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
            const std::uint32_t z = idx / plane;
            float acc = 0.f;
            for (int dz = -r; dz <= r; ++dz) {
                const int nz = std::clamp(static_cast<int>(z) + dz, 0, static_cast<int>(d) - 1);
                acc += b(idx - (z - nz) * plane) * k[dz + r];
            }
            c(idx) = acc;
        }
        return c;
    }

    [[nodiscard]] static Eigen::MatrixXf
    second_xx(const Eigen::MatrixXf& vol,
              std::uint32_t h, std::uint32_t w, std::uint32_t d, float sigma) {
        const auto b = gaussian_blur_3d(vol, h, w, d, sigma);
        Eigen::MatrixXf out = Eigen::MatrixXf::Zero(vol.rows(), 1);
        #pragma omp parallel for
        for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
            const std::uint32_t x = idx % w;
            if (x == 0 || x + 1 >= w) continue;
            out(idx) = (b(idx - 1) - 2.f * b(idx) + b(idx + 1)) * sigma * sigma;
        }
        return out;
    }

    [[nodiscard]] static Eigen::MatrixXf
    second_yy(const Eigen::MatrixXf& vol,
              std::uint32_t h, std::uint32_t w, std::uint32_t d, float sigma) {
        const auto b = gaussian_blur_3d(vol, h, w, d, sigma);
        Eigen::MatrixXf out = Eigen::MatrixXf::Zero(vol.rows(), 1);
        #pragma omp parallel for
        for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
            const std::uint32_t y = (idx / w) % h;
            if (y == 0 || y + 1 >= h) continue;
            out(idx) = (b(idx - w) - 2.f * b(idx) + b(idx + w)) * sigma * sigma;
        }
        return out;
    }

    [[nodiscard]] static Eigen::MatrixXf
    second_zz(const Eigen::MatrixXf& vol,
              std::uint32_t h, std::uint32_t w, std::uint32_t d, float sigma) {
        const std::uint32_t plane = h * w;
        const auto b = gaussian_blur_3d(vol, h, w, d, sigma);
        Eigen::MatrixXf out = Eigen::MatrixXf::Zero(vol.rows(), 1);
        #pragma omp parallel for
        for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
            const std::uint32_t z = idx / plane;
            if (z == 0 || z + 1 >= d) continue;
            out(idx) = (b(idx - plane) - 2.f * b(idx) + b(idx + plane)) * sigma * sigma;
        }
        return out;
    }
};

} // namespace neurometrix::core4d::domain