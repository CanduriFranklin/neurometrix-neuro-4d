#pragma once
#include <Eigen/Dense>
#include <cstdint>
#include <stdexcept>
namespace neurometrix::core4d::domain {
class Volume4DTensor {
public:
    Volume4DTensor(std::uint32_t h, std::uint32_t w, std::uint32_t d, std::uint32_t t)
        : h_(h), w_(w), d_(d), t_(t), data_(static_cast<Eigen::Index>(h)*w*d*t) {
        if (h == 0 || w == 0 || d == 0 || t == 0)
            throw std::invalid_argument("Volume4DTensor: zero dimension");
    }
    std::uint32_t height() const noexcept { return h_; }
    std::uint32_t width()  const noexcept { return w_; }
    std::uint32_t depth()  const noexcept { return d_; }
    std::uint32_t timepoints() const noexcept { return t_; }
    Eigen::Index plane_hw()   const noexcept { return static_cast<Eigen::Index>(h_)*w_; }
    Eigen::Index volume_hwd() const noexcept { return plane_hw()*d_; }
    float& at(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t t) noexcept { return data_[idx(x,y,z,t)]; }
    float  at(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t t) const noexcept { return data_[idx(x,y,z,t)]; }
    Eigen::MatrixXf slice_at(std::uint32_t z, std::uint32_t t) const {
        Eigen::MatrixXf out(h_, w_);
        const Eigen::Index base = static_cast<Eigen::Index>(t)*volume_hwd() + static_cast<Eigen::Index>(z)*plane_hw();
        for (std::uint32_t y = 0; y < h_; ++y)
            for (std::uint32_t x = 0; x < w_; ++x)
                out(y,x) = data_[base + static_cast<Eigen::Index>(y)*w_ + x];
        return out;
    }
    Eigen::VectorXf& flat() noexcept { return data_; }
    const Eigen::VectorXf& flat() const noexcept { return data_; }
private:
    Eigen::Index idx(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint32_t t) const noexcept {
        return static_cast<Eigen::Index>(t)*volume_hwd() + static_cast<Eigen::Index>(z)*plane_hw()
             + static_cast<Eigen::Index>(y)*w_ + x;
    }
    std::uint32_t h_{0}, w_{0}, d_{0}, t_{0};
    Eigen::VectorXf data_;
};
}
