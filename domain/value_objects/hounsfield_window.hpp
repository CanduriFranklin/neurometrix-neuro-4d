#pragma once
// HounsfieldWindow - Physics-based conditioning for CT.

#include <algorithm>
#include <cstdint>
#include <string_view>

namespace neurometrix::core4d::domain {

enum class WindowPreset : std::uint8_t {
    Brain, Blood, Bone, Stroke, Subdural,
};

struct WindowParams { double level; double width; };

[[nodiscard]] constexpr WindowParams window_of(WindowPreset p) noexcept {
    switch (p) {
        case WindowPreset::Brain:    return {40.0,   80.0};
        case WindowPreset::Blood:    return {50.0,  100.0};
        case WindowPreset::Bone:     return {500.0, 2000.0};
        case WindowPreset::Stroke:   return {32.0,    8.0};
        case WindowPreset::Subdural: return {80.0,  200.0};
    }
    return {40.0, 80.0};
}

class HounsfieldWindow {
public:
    [[nodiscard]] static double to_hu(std::int32_t raw,
                                       double slope, double intercept) noexcept {
        return static_cast<double>(raw) * slope + intercept;
    }

    [[nodiscard]] static double apply_window(double hu,
                                              const WindowParams& w) noexcept {
        const double lo = w.level - w.width * 0.5;
        const double hi = w.level + w.width * 0.5;
        if (hu <= lo) return 0.0;
        if (hu >= hi) return 1.0;
        return (hu - lo) / (hi - lo);
    }

    [[nodiscard]] static std::string_view classify_hu(double hu) noexcept {
        if (hu < -1000.0) return "air";
        if (hu < -100.0)  return "fat";
        if (hu < 20.0)    return "csf";
        if (hu < 45.0)    return "white_matter";
        if (hu < 60.0)    return "gray_matter";
        if (hu < 80.0)    return "acute_blood";
        if (hu < 100.0)   return "contrast_or_blood";
        if (hu < 300.0)   return "calcification_soft";
        return "bone";
    }

    [[nodiscard]] static bool is_acute_blood(double hu) noexcept {
        return hu >= 45.0 && hu <= 80.0;
    }
    [[nodiscard]] static bool is_ischemic_risk(double hu) noexcept {
        return hu >= 20.0 && hu <= 35.0;
    }
};

} // namespace neurometrix::core4d::domain