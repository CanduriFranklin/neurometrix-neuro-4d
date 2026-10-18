#include "dicom_volume_loader_adapter.hpp"

#include "infrastructure/logging/logger.hpp"

#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dctk.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace neurometrix::core4d::infrastructure::dicom {

namespace fs = std::filesystem;

namespace {

[[nodiscard]] std::string get_string(DcmDataset* ds, const DcmTagKey& tag) {
    OFString v;
    if (ds->findAndGetOFString(tag, v).good()) return std::string{v.c_str()};
    return {};
}

[[nodiscard]] double get_double(DcmDataset* ds, const DcmTagKey& tag, double fb) {
    Float64 v = 0.0;
    if (ds->findAndGetFloat64(tag, v).good()) return static_cast<double>(v);
    OFString s;
    if (ds->findAndGetOFString(tag, s).good() && !s.empty()) {
        try { return std::stod(s.c_str()); } catch (...) {}
    }
    return fb;
}

[[nodiscard]] std::int32_t get_int32(DcmDataset* ds, const DcmTagKey& tag, std::int32_t fb) {
    Sint32 v = 0;
    if (ds->findAndGetSint32(tag, v).good()) return static_cast<std::int32_t>(v);
    OFString s;
    if (ds->findAndGetOFString(tag, s).good() && !s.empty()) {
        try { return static_cast<std::int32_t>(std::stoi(s.c_str())); } catch (...) {}
    }
    return fb;
}

[[nodiscard]] std::string to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

struct LoadedSlice {
    Eigen::MatrixXf hu;
    std::int32_t    instance_number{0};
    std::int32_t    temporal_position{0};   // (0020,9128) - 0 if absent
    double          frame_time_ms{0.0};     // (0018,1063)
    double          slice_position{0.0};
    std::string     study_uid;
    std::string     series_uid;
    std::string     modality;
    std::string     body_part;
};

// Read one DICOM file, apply RescaleSlope/Intercept -> HU.
// Returns the single axial slice.
[[nodiscard]] std::expected<LoadedSlice, application::DicomError>
read_slice(const fs::path& path) {
    DcmFileFormat fmt;
    if (!fmt.loadFile(path.string().c_str()).good()) {
        return std::unexpected(application::DicomError::InvalidHeader);
    }
    DcmDataset* ds = fmt.getDataset();
    if (ds == nullptr) return std::unexpected(application::DicomError::InvalidHeader);

    LoadedSlice s{};
    s.study_uid  = get_string(ds, DCM_StudyInstanceUID);
    s.series_uid = get_string(ds, DCM_SeriesInstanceUID);
    s.modality   = to_upper(get_string(ds, DCM_Modality));
    s.body_part  = to_upper(get_string(ds, DCM_BodyPartExamined));

    if (s.study_uid.empty()) {
        return std::unexpected(application::DicomError::MissingMandatoryTag);
    }

    s.instance_number   = get_int32(ds, DCM_InstanceNumber, 0);
    s.temporal_position = get_int32(ds, DCM_TemporalPositionIdentifier, 0);
    s.frame_time_ms     = get_double(ds, DCM_FrameTime, 0.0);   // ms
    s.slice_position    = get_double(ds, DCM_SliceLocation, 0.0);

    const double slope     = get_double(ds, DCM_RescaleSlope, 1.0);
    const double intercept = get_double(ds, DCM_RescaleIntercept, 0.0);

    Uint16 rows = 0, cols = 0;
    if (ds->findAndGetUint16(DCM_Rows, rows).bad() ||
        ds->findAndGetUint16(DCM_Columns, cols).bad() ||
        rows == 0 || cols == 0) {
        return std::unexpected(application::DicomError::MissingMandatoryTag);
    }

    const Uint16* pixels = nullptr;
    unsigned long count = 0;
    if (ds->findAndGetUint16Array(DCM_PixelData, pixels, &count).bad() ||
        pixels == nullptr || count == 0) {
        return std::unexpected(application::DicomError::CorruptedPixelData);
    }

    const std::size_t frame_pixels =
        static_cast<std::size_t>(rows) * static_cast<std::size_t>(cols);
    if (count < frame_pixels) {
        return std::unexpected(application::DicomError::CorruptedPixelData);
    }

    s.hu = Eigen::MatrixXf(rows, cols);
    for (std::uint32_t r = 0; r < rows; ++r) {
        for (std::uint32_t c = 0; c < cols; ++c) {
            const double raw = static_cast<double>(pixels[r * cols + c]);
            s.hu(r, c) = static_cast<float>(raw * slope + intercept);
        }
    }
    return s;
}

} // anonymous namespace

struct DicomVolumeLoaderAdapter::Impl {};

DicomVolumeLoaderAdapter::DicomVolumeLoaderAdapter() noexcept
    : pimpl_(std::make_unique<Impl>()) {}

DicomVolumeLoaderAdapter::~DicomVolumeLoaderAdapter() = default;

bool DicomVolumeLoaderAdapter::can_read(std::string_view path) const noexcept {
    try {
        const fs::path p{std::string{path}};
        std::error_code ec;
        return fs::exists(p, ec) && !ec;
    } catch (...) { return false; }
}

std::expected<domain::Volume4D, application::DicomError>
DicomVolumeLoaderAdapter::load(std::string_view path) const noexcept {
    try {
        const fs::path p{std::string{path}};
        std::error_code ec;

        if (!fs::exists(p, ec) || ec) {
            return std::unexpected(application::DicomError::FileNotFound);
        }

        // Collect candidate DICOM files
        std::vector<fs::path> files;
        if (fs::is_directory(p, ec)) {
            for (const auto& entry : fs::directory_iterator(p, ec)) {
                if (ec) return std::unexpected(application::DicomError::IOError);
                if (!entry.is_regular_file(ec) || ec) continue;
                const auto& ep = entry.path();
                if (ep.extension() != ".dcm" && ep.extension() != ".DCM") continue;
                files.push_back(ep);
            }
        } else if (fs::is_regular_file(p, ec) && !ec) {
            files.push_back(p);
        } else {
            return std::unexpected(application::DicomError::IOError);
        }

        if (files.empty()) {
            return std::unexpected(application::DicomError::FileNotFound);
        }

        // For directories: need at least 4 slices to be a volume.
        // For single files: we accept only if the file is multiframe (see below).
        std::vector<LoadedSlice> slices;
        slices.reserve(files.size());

        for (const auto& f : files) {
            auto slice = read_slice(f);
            if (!slice) return std::unexpected(slice.error());
            slices.push_back(std::move(*slice));
        }

        if (slices.size() < 4) {
            logging::get()->warn("[dicom-4d] only {} timepoints; need >= 2 for 4D",
                                 slices.size());
            return std::unexpected(application::DicomError::InsufficientSlices);
        }

        // Validate modality and body part from first slice
        const auto& first = slices.front();
        if (first.modality != "CT") {
            logging::get()->warn("[dicom-4d] rejecting non-CT modality: '{}'", first.modality);
            return std::unexpected(application::DicomError::UnsupportedModality);
        }
        if (!first.body_part.empty() &&
            first.body_part != "HEAD" && first.body_part != "BRAIN" &&
            first.body_part != "SKULL" && first.body_part != "NEURO") {
            logging::get()->warn("[dicom-4d] rejecting non-head body part: '{}'", first.body_part);
            return std::unexpected(application::DicomError::UnsupportedBodyPart);
        }

        // ---- Group slices by TemporalPositionIdentifier (0020,9128) ----
        // Map: T -> vector<slice index>. If all slices have T=0, treat as 3D (T=1).
        std::map<std::int32_t, std::vector<LoadedSlice*>> by_time;
        for (auto& s : slices) {
            by_time[s.temporal_position].push_back(&s);
        }

        const std::uint32_t t_count = static_cast<std::uint32_t>(by_time.size());

        // Sort each timepoint's slices by InstanceNumber; fallback SliceLocation
        for (auto& [t_id, group] : by_time) {
            const bool any_instance = std::any_of(
                group.begin(), group.end(),
                [](const LoadedSlice* s) { return s->instance_number != 0; });
            if (any_instance) {
                std::sort(group.begin(), group.end(),
                          [](const LoadedSlice* a, const LoadedSlice* b) {
                              return a->instance_number < b->instance_number;
                          });
            } else {
                std::sort(group.begin(), group.end(),
                          [](const LoadedSlice* a, const LoadedSlice* b) {
                              return a->slice_position < b->slice_position;
                          });
            }
        }

        // Validate dimensions consistent across timepoints
        const std::uint32_t h = static_cast<std::uint32_t>(slices.front().hu.rows());
        const std::uint32_t w = static_cast<std::uint32_t>(slices.front().hu.cols());
        const std::uint32_t d = static_cast<std::uint32_t>(by_time.begin()->second.size());

        if (t_count < 1 || d < 2) {
            logging::get()->warn("[dicom-4d] insufficient slices per timepoint");
            return std::unexpected(application::DicomError::InsufficientSlices);
        }

        for (auto& [t_id, group] : by_time) {
            if (group.size() != d) {
                logging::get()->warn("[dicom-4d] inconsistent slice count across timepoints");
                return std::unexpected(application::DicomError::CorruptedPixelData);
            }
        }

        // Build the 4D tensor
        domain::Volume4DTensor tensor(h, w, d, t_count);

        std::uint32_t ti = 0;
        for (auto& [t_id, group] : by_time) {
            for (std::uint32_t z = 0; z < d; ++z) {
                const auto& slice = *group[z];
                if (static_cast<std::uint32_t>(slice.hu.rows()) != h ||
                    static_cast<std::uint32_t>(slice.hu.cols()) != w) {
                    logging::get()->warn("[dicom-4d] inconsistent slice dimensions");
                    return std::unexpected(application::DicomError::CorruptedPixelData);
                }
                for (std::uint32_t y = 0; y < h; ++y) {
                    for (std::uint32_t x = 0; x < w; ++x) {
                        tensor.at(x, y, z, ti) = slice.hu(y, x);
                    }
                }
            }
            ++ti;
        }

        // ---- Temporal resolution ----
        // 1. Prefer FrameTime (0018,1063) from the first slice of any
        //    timepoint; convert ms -> s.
        // 2. Fallback: 1.0 s with explicit warning (this was the silent
        //    bug that produced dt=1.0 with 0.5s data).
        double dt_seconds = 1.0;
        bool dt_explicit = false;
        for (const auto& s : slices) {
            if (s.frame_time_ms > 0.0) {
                dt_seconds = s.frame_time_ms / 1000.0;
                dt_explicit = true;
                break;
            }
        }
        if (!dt_explicit && t_count > 1) {
            logging::get()->warn(
                "[dicom-4d] FrameTime (0018,1063) missing; "
                "defaulting dt=1.0s. Pass explicit FrameTime in DICOM.");
        }

        logging::get()->info("[dicom-4d] loaded {}D volume {}x{}x{}x{} HU[{}..{}] dt={:.3f}s",
                             t_count > 1 ? "4" : "3",
                             h, w, d, t_count,
                             static_cast<double>(tensor.flat().minCoeff()),
                             static_cast<double>(tensor.flat().maxCoeff()),
                             dt_seconds);

        return domain::Volume4D(std::move(tensor), first.modality, first.body_part,
                                domain::DurationSeconds{dt_seconds});
    } catch (const std::bad_alloc&) {
        return std::unexpected(application::DicomError::CorruptedPixelData);
    } catch (...) {
        return std::unexpected(application::DicomError::UnknownError);
    }
}

} // namespace neurometrix::core4d::infrastructure::dicom