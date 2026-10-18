#pragma once
// StrongTime - type-safe temporal quantities.
//
// Motivation: the previous bug where dt_seconds was a raw double and
// the caller passed 1.0 while the data was sampled at 0.5s.
//
//   DurationSeconds  - physical time interval in seconds
//   TimeIndex        - discrete frame index (0..T-1)
//   TimeSeriesLength - number of timepoints
//
// Conversion between TimeIndex and DurationSeconds requires the sampling
// interval explicitly. This is intentional: the compiler forces the caller
// to provide dt.

#include <cstdint>

namespace neurometrix::core4d::domain {

class DurationSeconds {
public:
    constexpr DurationSeconds() noexcept : s_(0.0) {}
    constexpr explicit DurationSeconds(double s) noexcept : s_(s) {}

    [[nodiscard]] constexpr double value() const noexcept { return s_; }
    [[nodiscard]] constexpr double to_millis()  const noexcept { return s_ * 1000.0; }
    [[nodiscard]] constexpr double to_minutes() const noexcept { return s_ / 60.0; }

    [[nodiscard]] static constexpr DurationSeconds from_millis(double ms) noexcept {
        return DurationSeconds{ms / 1000.0};
    }
    [[nodiscard]] static constexpr DurationSeconds from_minutes(double mn) noexcept {
        return DurationSeconds{mn * 60.0};
    }

    [[nodiscard]] constexpr bool operator<(DurationSeconds o) const noexcept { return s_ < o.s_; }
    [[nodiscard]] constexpr bool operator==(DurationSeconds o) const noexcept { return s_ == o.s_; }

    [[nodiscard]] constexpr DurationSeconds operator+(DurationSeconds o) const noexcept { return DurationSeconds{s_ + o.s_}; }
    [[nodiscard]] constexpr DurationSeconds operator-(DurationSeconds o) const noexcept { return DurationSeconds{s_ - o.s_}; }
    [[nodiscard]] constexpr DurationSeconds operator*(double k) const noexcept { return DurationSeconds{s_ * k}; }

private:
    double s_;
};

class TimeIndex {
public:
    constexpr TimeIndex() noexcept : v_(0) {}
    constexpr explicit TimeIndex(std::uint32_t v) noexcept : v_(v) {}

    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return v_; }
    [[nodiscard]] constexpr bool operator<(TimeIndex o) const noexcept { return v_ < o.v_; }
    [[nodiscard]] constexpr bool operator==(TimeIndex o) const noexcept { return v_ == o.v_; }
    [[nodiscard]] constexpr TimeIndex next() const noexcept { return TimeIndex{v_ + 1}; }

    // Explicit conversion: index * dt
    [[nodiscard]] constexpr DurationSeconds to_seconds(DurationSeconds dt) const noexcept {
        return DurationSeconds{static_cast<double>(v_) * dt.value()};
    }

private:
    std::uint32_t v_;
};

class TimeSeriesLength {
public:
    constexpr TimeSeriesLength() noexcept : n_(0) {}
    constexpr explicit TimeSeriesLength(std::uint32_t n) noexcept : n_(n) {}

    [[nodiscard]] constexpr std::uint32_t value() const noexcept { return n_; }
    [[nodiscard]] constexpr bool valid() const noexcept { return n_ > 0; }
    [[nodiscard]] constexpr bool is_4d() const noexcept { return n_ > 1; }
    [[nodiscard]] constexpr bool operator==(TimeSeriesLength o) const noexcept { return n_ == o.n_; }

private:
    std::uint32_t n_;
};

} // namespace neurometrix::core4d::domain
