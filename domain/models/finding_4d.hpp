#pragma once
// Finding4D - Volumetric clinical finding.

#include <cstdint>
#include <string>

namespace neurometrix::core4d::domain {

enum class FindingType : std::uint8_t {
    MicroHemorrhage,
    AcuteHemorrhage,
    IschemicChange,
    MidlineShift,
    VentricularDilation,
    MassEffect,
    Calcification,
    Edema,
    Unknown,
};

[[nodiscard]] inline const char* to_string(FindingType t) noexcept {
    switch (t) {
        case FindingType::MicroHemorrhage:     return "micro_hemorrhage";
        case FindingType::AcuteHemorrhage:     return "acute_hemorrhage";
        case FindingType::IschemicChange:      return "ischemic_change";
        case FindingType::MidlineShift:        return "midline_shift";
        case FindingType::VentricularDilation: return "ventricular_dilation";
        case FindingType::MassEffect:          return "mass_effect";
        case FindingType::Calcification:       return "calcification";
        case FindingType::Edema:               return "edema";
        case FindingType::Unknown:             return "unknown";
    }
    return "unknown";
}

struct Finding4D {
    FindingType  type{FindingType::Unknown};
    double       confidence{0.0};       // 0.0 - 1.0
    int          centroid_x{0};         // voxel coordinates
    int          centroid_y{0};
    int          centroid_z{0};
    double       volume_mm3{0.0};       // physical volume
    double       mean_hu{0.0};
    std::string  note;
};

} // namespace neurometrix::core4d::domain