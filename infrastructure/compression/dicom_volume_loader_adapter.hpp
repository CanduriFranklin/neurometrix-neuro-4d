#pragma once

#include "application/ports/outbound/i_dicom_volume_loader.hpp"

#include <memory>

namespace neurometrix::core4d::infrastructure::dicom {

class DicomVolumeLoaderAdapter final : public application::IDicomVolumeLoader {
public:
    DicomVolumeLoaderAdapter() noexcept;
    ~DicomVolumeLoaderAdapter() override;

    DicomVolumeLoaderAdapter(const DicomVolumeLoaderAdapter&) = delete;
    DicomVolumeLoaderAdapter& operator=(const DicomVolumeLoaderAdapter&) = delete;

    [[nodiscard]] bool can_read(std::string_view path) const noexcept override;

    [[nodiscard]] std::expected<domain::Volume4D, application::DicomError>
    load(std::string_view path) const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace neurometrix::core4d::infrastructure::dicom