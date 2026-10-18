#pragma once
// Wire contracts between Router and Core 4D.
// Input: binary. Output: JSON.

#include "domain/models/finding_4d.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace neurometrix::core4d::contracts {

struct VolumeRequest {
    std::uint64_t message_id{0};
    std::uint64_t timestamp_utc{0};
    std::string   study_uid;
    std::string   dicom_path;
};

struct PerfusionSummary {
    double mean_cbf{0.0};
    double mean_cbv{0.0};
    double mean_mtt{0.0};
    double mean_ttp{0.0};
    double mean_tmax{0.0};
};

struct MismatchSummary {
    std::uint64_t core_voxels{0};
    std::uint64_t penumbra_voxels{0};
    double        mismatch_ratio{0.0};
};

struct VolumeResult {
    std::uint64_t message_id{0};
    std::string   study_uid;
    std::string   status;

    // Dimensions of the processed volume.
    std::uint32_t height{0};
    std::uint32_t width{0};
    std::uint32_t depth{0};
    std::uint32_t timepoints{0};      // 4D: number of temporal frames
    bool          is_4d{false};

    // HU statistics across the full volume.
    double mean_hu{0.0};
    double stddev_hu{0.0};
    double min_hu{0.0};
    double max_hu{0.0};

    // Volumetric advanced metrics.
    double mean_entropy{0.0};
    double peak_vesselness{0.0};
    double asymmetry_score{0.0};
    double total_volume_mm3{0.0};

    // ---- 4D / perfusion (populated only when is_4d) ----
    PerfusionSummary perfusion{};
    MismatchSummary  mismatch{};

    // Clinical findings extracted from the volume.
    std::vector<domain::Finding4D> findings;

    std::uint64_t processing_ms{0};
    bool          gpu_used{false};
    std::string   error_detail;
};

} // namespace neurometrix::core4d::contracts