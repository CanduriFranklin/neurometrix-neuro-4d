#pragma once
// MorphologicalOps4D - Dilate / erode / top-hat on 4D volumes.

#include <Eigen/Dense>
#include <algorithm>

namespace neurometrix::core4d::domain {

class MorphologicalOps4D {
public:
    [[nodiscard]] static Eigen::MatrixXf
    dilate(const Eigen::MatrixXf& vol,
           std::uint32_t h, std::uint32_t w, std::uint32_t d,
           int radius = 1) {
        const std::uint32_t plane = h * w;
        Eigen::MatrixXf out(vol.rows(), 1);
        #pragma omp parallel for
        for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
            const std::uint32_t x = idx % w;
            const std::uint32_t y = (idx / w) % h;
            const std::uint32_t z = idx / plane;
            float m = vol(idx);
            for (int dz = -radius; dz <= radius; ++dz) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        const int nx = static_cast<int>(x) + dx;
                        const int ny = static_cast<int>(y) + dy;
                        const int nz = static_cast<int>(z) + dz;
                        if (nx < 0 || nx >= static_cast<int>(w)) continue;
                        if (ny < 0 || ny >= static_cast<int>(h)) continue;
                        if (nz < 0 || nz >= static_cast<int>(d)) continue;
                        const std::uint32_t nidx = nz * plane + ny * w + nx;
                        m = std::max(m, vol(nidx));
                    }
                }
            }
            out(idx) = m;
        }
        return out;
    }

    [[nodiscard]] static Eigen::MatrixXf
    erode(const Eigen::MatrixXf& vol,
          std::uint32_t h, std::uint32_t w, std::uint32_t d,
          int radius = 1) {
        const std::uint32_t plane = h * w;
        Eigen::MatrixXf out(vol.rows(), 1);
        #pragma omp parallel for
        for (int idx = 0; idx < static_cast<int>(vol.rows()); ++idx) {
            const std::uint32_t x = idx % w;
            const std::uint32_t y = (idx / w) % h;
            const std::uint32_t z = idx / plane;
            float m = vol(idx);
            for (int dz = -radius; dz <= radius; ++dz) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        const int nx = static_cast<int>(x) + dx;
                        const int ny = static_cast<int>(y) + dy;
                        const int nz = static_cast<int>(z) + dz;
                        if (nx < 0 || nx >= static_cast<int>(w)) continue;
                        if (ny < 0 || ny >= static_cast<int>(h)) continue;
                        if (nz < 0 || nz >= static_cast<int>(d)) continue;
                        const std::uint32_t nidx = nz * plane + ny * w + nx;
                        m = std::min(m, vol(nidx));
                    }
                }
            }
            out(idx) = m;
        }
        return out;
    }

    [[nodiscard]] static Eigen::MatrixXf
    opening(const Eigen::MatrixXf& vol,
            std::uint32_t h, std::uint32_t w, std::uint32_t d, int r = 1) {
        return dilate(erode(vol, h, w, d, r), h, w, d, r);
    }

    [[nodiscard]] static Eigen::MatrixXf
    white_tophat(const Eigen::MatrixXf& vol,
                 std::uint32_t h, std::uint32_t w, std::uint32_t d, int r = 3) {
        return vol - opening(vol, h, w, d, r);
    }
};

} // namespace neurometrix::core4d::domain