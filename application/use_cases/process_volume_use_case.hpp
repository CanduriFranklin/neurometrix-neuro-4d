#pragma once

#include "contracts/messages.hpp"
#include "application/ports/inbound/i_process_volume_use_case.hpp"
#include "application/ports/outbound/i_dicom_volume_loader.hpp"
#include "application/ports/outbound/i_job_repository.hpp"
#include "application/ports/outbound/i_message_publisher.hpp"

#include <cstdint>

namespace neurometrix::core4d::application {

class ProcessVolumeUseCase final : public IProcessVolumeUseCase {
public:
    ProcessVolumeUseCase(
        const IDicomVolumeLoader& dicom_loader,
        IJobRepository&           repository,
        IMessagePublisher&        publisher
    ) noexcept;

    [[nodiscard]] std::expected<contracts::VolumeResult, UseCaseError>
    execute(const contracts::VolumeRequest& request) noexcept override;

private:
    [[nodiscard]] std::expected<void, UseCaseError>
    validate(const contracts::VolumeRequest& request) const noexcept;

    const IDicomVolumeLoader& dicom_loader_;
    IJobRepository&           repository_;
    IMessagePublisher&        publisher_;
};

} // namespace neurometrix::core4d::application