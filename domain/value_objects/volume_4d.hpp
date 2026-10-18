#pragma once
// Volume4D - Value Object representing a 4D CT volume in Hounsfield Units.
//
// Internally wraps Volume4DTensor (rank-4: H, W, D, T).
// Exposes legacy 3D API (hu(), slice(), height()/width()/depth())
// for backward compatibility with code that still works on single timepoints.
//
// For T == 1: legacy API returns the only timepoint, no cost.
// For T > 1:  hu() returns timepoint 0; use slice_at_t(z, t) or tensor() for 4D.

#include "domain/models/volume_4d_tensor.hpp"
#include "domain/value_objects/strong_time.hpp"

#include <Eigen/Dense>
#include <cstdint>
#include <string>
#include <utility>

namespace neurometrix::core4d::domain {

class Volume4D {
public:
    // ---- Legacy ctor: 3D only (implicitly T=1) ----
    Volume4D(Eigen::MatrixXf hu_voxels,   // flattened (H*W*D) x 1
             std::uint32_t height,
             std::uint32_t width,
             std::uint32_t depth,
             std::string modality,
             std::string body_part,
             DurationSeconds dt_seconds = DurationSeconds{1.0})
        : tensor_(height, width, depth, 1),
          h_(height), w_(width), d_(depth), t_(1),
          dt_(dt_seconds),
          modality_(std::move(modality)),
          body_part_(std::move(body_part))
    {
        const Eigen::Index n = static_cast<Eigen::Index>(height) * width * depth;
        for (Eigen::Index i = 0; i < n && i < hu_voxels.size(); ++i) {
            tensor_.flat()[i] = hu_voxels(i, 0);
        }
    }

    // ---- New ctor: full 4D tensor ----
    Volume4D(Volume4DTensor tensor,
             std::string modality,
             std::string body_part,
             DurationSeconds dt_seconds = DurationSeconds{1.0})
        : tensor_(std::move(tensor)),
          h_(tensor_.height()), w_(tensor_.width()),
          d_(tensor_.depth()), t_(tensor_.timepoints()),
          dt_(dt_seconds),
          modality_(std::move(modality)),
          body_part_(std::move(body_part))
    {}

    // ---- Legacy 3D API (backward compatible) ----
    // For T == 1: returns the whole volume as (H*W*D) x 1.
    // For T > 1:  returns the FIRST timepoint as (H*W*D) x 1.
    [[nodiscard]] Eigen::MatrixXf hu() const {
        const Eigen::Index n = static_cast<Eigen::Index>(h_) * w_ * d_;
        Eigen::MatrixXf out(n, 1);
        const auto& flat = tensor_.flat();
        for (Eigen::Index i = 0; i < n; ++i) out(i, 0) = flat[i];
        return out;
    }

    [[nodiscard]] Eigen::MatrixXf slice(std::uint32_t z) const {
        return tensor_.slice_at(z, 0);
    }

    [[nodiscard]] std::uint32_t height() const noexcept { return h_; }
    [[nodiscard]] std::uint32_t width()  const noexcept { return w_; }
    [[nodiscard]] std::uint32_t depth()  const noexcept { return d_; }
    [[nodiscard]] bool empty() const noexcept { return tensor_.flat().size() == 0; }

    [[nodiscard]] const std::string& modality()  const noexcept { return modality_; }
    [[nodiscard]] const std::string& body_part() const noexcept { return body_part_; }

    // ---- New 4D API ----
    [[nodiscard]] std::uint32_t timepoints() const noexcept { return t_; }
    [[nodiscard]] bool is_4d() const noexcept { return t_ > 1; }
    [[nodiscard]] DurationSeconds frame_interval() const noexcept { return dt_; }

    [[nodiscard]] const Volume4DTensor& tensor() const noexcept { return tensor_; }
    [[nodiscard]] Volume4DTensor&       tensor()       noexcept { return tensor_; }

    [[nodiscard]] Eigen::MatrixXf slice_at_t(std::uint32_t z,
                                              std::uint32_t t) const {
        return tensor_.slice_at(z, t);
    }

    // Extract full (H*W*D) x 1 timepoint as a MatrixXf.
    [[nodiscard]] Eigen::MatrixXf timepoint(std::uint32_t t) const {
        const Eigen::Index n = static_cast<Eigen::Index>(h_) * w_ * d_;
        Eigen::MatrixXf out(n, 1);
        const auto& flat = tensor_.flat();
        const Eigen::Index base = static_cast<Eigen::Index>(t) * n;
        for (Eigen::Index i = 0; i < n; ++i) out(i, 0) = flat[base + i];
        return out;
    }

private:
    Volume4DTensor tensor_;
    std::uint32_t  h_{0}, w_{0}, d_{0}, t_{0};
    DurationSeconds dt_{1.0};
    std::string    modality_;
    std::string    body_part_;
};

} // namespace neurometrix::core4d::domain
