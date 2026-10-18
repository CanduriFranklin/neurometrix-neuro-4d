#pragma once
// StrongSpatial - type-safe spatial indices and coordinates.
//
// Prevents: mixing (x,y,z) order, using slice index where voxel index
// is expected, confusing physical mm with voxel counts.

#include <cstdint>
#include <string>

namespace neurometrix::core4d::domain {

class VoxelCoord {
public:
    constexpr VoxelCoord() noexcept : x_(0), y_(0), z_(0) {}
    constexpr VoxelCoord(std::uint32_t x, std::uint32_t y, std::uint32_t z) noexcept
        : x_(x), y_(y), z_(z) {}

    [[nodiscard]] constexpr std::uint32_t x() const noexcept { return x_; }
    [[nodiscard]] constexpr std::uint32_t y() const noexcept { return y_; }
    [[nodiscard]] constexpr std::uint32_t z() const noexcept { return z_; }

    [[nodiscard]] constexpr bool operator==(VoxelCoord o) const noexcept {
        return x_ == o.x_ && y_ == o.y_ && z_ == o.z_;
    }

    [[nodiscard]] std::string to_string() const {
        return "(" + std::to_string(x_) + "," + std::to_string(y_) + "," + std::to_string(z_) + ")";
    }

private:
    std::uint32_t x_, y_, z_;
};

class SliceIndex {
public:
    constexpr SliceIndex() noexcept : v_(0) {}
    constexpr explicit SliceIndex(std::uint32_t v) noexcept : v_(v) {}
    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return v_; }
    [[nodiscard]] constexpr bool operator<(SliceIndex o) const noexcept { return v_ < o.v_; }
    [[nodiscard]] constexpr bool operator==(SliceIndex o) const noexcept { return v_ == o.v_; }
private:
    std::uint32_t v_;
};

class Millimetres {
public:
    constexpr Millimetres() noexcept : mm_(0.0) {}
    constexpr explicit Millimetres(double mm) noexcept : mm_(mm) {}
    [[nodiscard]] constexpr double value() const noexcept { return mm_; }
    [[nodiscard]] constexpr double to_cm() const noexcept { return mm_ / 10.0; }
private:
    double mm_;
};

class VoxelVolumeMM3 {
public:
    constexpr VoxelVolumeMM3() noexcept : v_(0.0) {}
    constexpr explicit VoxelVolumeMM3(double v) noexcept : v_(v) {}
    [[nodiscard]] constexpr double value() const noexcept { return v_; }
    [[nodiscard]] constexpr double to_cm3() const noexcept { return v_ / 1000.0; }
    [[nodiscard]] constexpr double to_ml()  const noexcept { return v_ / 1000.0; }

    [[nodiscard]] static constexpr VoxelVolumeMM3
    from_spacing(Millimetres sx, Millimetres sy, Millimetres sz) noexcept {
        return VoxelVolumeMM3{sx.value() * sy.value() * sz.value()};
    }

private:
    double v_;
};

} // namespace neurometrix::core4d::domain
