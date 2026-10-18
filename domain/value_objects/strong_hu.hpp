#pragma once
// StrongHounsfield - type-safe Hounsfield Units.
//
// Prevents accidental mixing of HU (a physical quantity in CT) with
// probabilities, distances, or raw pixel values.
//
// Arithmetic is restricted to what is physically meaningful:
//   HU + HU -> HU
//   HU - HU -> HU  (a differential HU)
//   HU * scalar -> HU  (e.g. rescale slope)
//   HU * HU -> INVALID (multiplying two physical quantities is usually a bug)

#include <cmath>
#include <cstdint>
#include <limits>

namespace neurometrix::core4d::domain {

class HU {
public:
    constexpr HU() noexcept : v_(0.0) {}
    constexpr explicit HU(double v) noexcept : v_(v) {}

    [[nodiscard]] constexpr double value() const noexcept { return v_; }

    // Physically meaningful operations
    [[nodiscard]] constexpr HU operator+(HU o) const noexcept { return HU{v_ + o.v_}; }
    [[nodiscard]] constexpr HU operator-(HU o) const noexcept { return HU{v_ - o.v_}; }
    [[nodiscard]] constexpr HU operator-() const noexcept { return HU{-v_}; }

    HU& operator+=(HU o) noexcept { v_ += o.v_; return *this; }
    HU& operator-=(HU o) noexcept { v_ -= o.v_; return *this; }

    // Scalar scaling (rescale slope/amplitude adjustment)
    [[nodiscard]] constexpr HU operator*(double k) const noexcept { return HU{v_ * k}; }
    [[nodiscard]] constexpr HU operator/(double k) const noexcept { return HU{v_ / k}; }

    // Comparisons
    [[nodiscard]] constexpr bool operator==(HU o) const noexcept { return v_ == o.v_; }
    [[nodiscard]] constexpr bool operator< (HU o) const noexcept { return v_ <  o.v_; }
    [[nodiscard]] constexpr bool operator<=(HU o) const noexcept { return v_ <= o.v_; }
    [[nodiscard]] constexpr bool operator> (HU o) const noexcept { return v_ >  o.v_; }
    [[nodiscard]] constexpr bool operator>=(HU o) const noexcept { return v_ >= o.v_; }

    // Clinical predicates
    [[nodiscard]] constexpr bool is_air()          const noexcept { return v_ < -900.0; }
    [[nodiscard]] constexpr bool is_fat()          const noexcept { return v_ >= -200.0 && v_ < -50.0; }
    [[nodiscard]] constexpr bool is_csf()          const noexcept { return v_ >= -50.0 && v_ < 20.0; }
    [[nodiscard]] constexpr bool is_white_matter() const noexcept { return v_ >= 20.0 && v_ < 45.0; }
    [[nodiscard]] constexpr bool is_gray_matter()  const noexcept { return v_ >= 45.0 && v_ < 60.0; }
    [[nodiscard]] constexpr bool is_acute_blood()  const noexcept { return v_ >= 60.0 && v_ < 80.0; }
    [[nodiscard]] constexpr bool is_calcification()const noexcept { return v_ >= 100.0 && v_ < 400.0; }
    [[nodiscard]] constexpr bool is_bone()         const noexcept { return v_ >= 400.0; }
    [[nodiscard]] constexpr bool is_tissue()       const noexcept {
        return v_ >= -200.0 && v_ <= 400.0;
    }

    [[nodiscard]] static constexpr HU air()    noexcept { return HU{-1000.0}; }
    [[nodiscard]] static constexpr HU water()  noexcept { return HU{0.0}; }
    [[nodiscard]] static constexpr HU blood()  noexcept { return HU{50.0}; }
    [[nodiscard]] static constexpr HU bone()   noexcept { return HU{800.0}; }

private:
    double v_;
};

// User-defined literal: 65.0_hu
[[nodiscard]] constexpr HU operator""_hu(long double x) noexcept {
    return HU{static_cast<double>(x)};
}
[[nodiscard]] constexpr HU operator""_hu(unsigned long long x) noexcept {
    return HU{static_cast<double>(x)};
}

// Scalar * HU (commutative)
[[nodiscard]] constexpr HU operator*(double k, HU h) noexcept { return h * k; }

} // namespace neurometrix::core4d::domain
