#pragma once
#include <cstdint>
namespace neurometrix::core4d::domain {
struct TimeIdx   { std::int32_t value{0}; constexpr explicit TimeIdx(std::int32_t v = 0) noexcept : value(v) {} };
struct SliceIdx  { std::int32_t value{0}; constexpr explicit SliceIdx(std::int32_t v = 0) noexcept : value(v) {} };
struct VolumeIdx { std::int32_t value{0}; constexpr explicit VolumeIdx(std::int32_t v = 0) noexcept : value(v) {} };
}
