#pragma once
// StrongPhysiology - type-safe physiological quantities for perfusion.
//
// These are the OUTPUTS of perfusion analysis, each with explicit units:
//   CBF  - Cerebral Blood Flow   [mL / 100g / min]
//   CBV  - Cerebral Blood Volume [mL / 100g]
//   MTT  - Mean Transit Time     [s]
//   TTP  - Time To Peak          [s]
//   TMax - Time to max residue   [s]
//
// Making these distinct types prevents swapping them in call sites.

#include "domain/value_objects/strong_time.hpp"

namespace neurometrix::core4d::domain {

class CBF_mL_per_100g_per_min {
public:
    constexpr CBF_mL_per_100g_per_min() noexcept : v_(0.0) {}
    constexpr explicit CBF_mL_per_100g_per_min(double v) noexcept : v_(v) {}
    [[nodiscard]] constexpr double value() const noexcept { return v_; }
    [[nodiscard]] constexpr bool is_normal() const noexcept { return v_ >= 40.0 && v_ <= 60.0; }
    [[nodiscard]] constexpr bool is_low()    const noexcept { return v_ < 20.0; }
private:
    double v_;
};

class CBV_mL_per_100g {
public:
    constexpr CBV_mL_per_100g() noexcept : v_(0.0) {}
    constexpr explicit CBV_mL_per_100g(double v) noexcept : v_(v) {}
    [[nodiscard]] constexpr double value() const noexcept { return v_; }
    [[nodiscard]] constexpr bool is_normal() const noexcept { return v_ >= 3.0 && v_ <= 5.0; }
    [[nodiscard]] constexpr bool is_low()    const noexcept { return v_ < 2.0; }
private:
    double v_;
};

class MTT_seconds {
public:
    constexpr MTT_seconds() noexcept : s_(0.0) {}
    constexpr explicit MTT_seconds(double s) noexcept : s_(s) {}
    [[nodiscard]] constexpr double value() const noexcept { return s_; }
    [[nodiscard]] constexpr bool is_normal() const noexcept { return s_ >= 4.0 && s_ <= 6.0; }
    [[nodiscard]] constexpr bool is_prolonged() const noexcept { return s_ > 6.5; }
private:
    double s_;
};

class TMax_seconds {
public:
    constexpr TMax_seconds() noexcept : s_(0.0) {}
    constexpr explicit TMax_seconds(double s) noexcept : s_(s) {}
    [[nodiscard]] constexpr double value() const noexcept { return s_; }
    // Clinical thresholds (DEFUSE 2/3, EXTEND-IA):
    //   TMax > 6s  -> tissue at risk (penumbra)
    //   TMax > 10s -> severe hypoperfusion
    [[nodiscard]] constexpr bool is_penumbra_threshold() const noexcept { return s_ > 6.0; }
    [[nodiscard]] constexpr bool is_severe()             const noexcept { return s_ > 10.0; }
private:
    double s_;
};

// Central volume theorem: MTT = CBV / CBF (with unit conversion).
[[nodiscard]] inline MTT_seconds
central_volume_theorem(CBV_mL_per_100g cbv, CBF_mL_per_100g_per_min cbf) noexcept {
    if (cbf.value() < 1e-6) return MTT_seconds{0.0};
    // CBV[mL/100g] / CBF[mL/100g/min] -> minutes; * 60 -> seconds
    return MTT_seconds{(cbv.value() / cbf.value()) * 60.0};
}

} // namespace neurometrix::core4d::domain
