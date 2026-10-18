#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace neurometrix::core4d::infrastructure::security {

enum class TlsError : std::uint8_t {
    CaFileMissing,
    CertFileMissing,
    KeyFileMissing,
    CertKeyMismatch,
    InvalidCertFormat,
    IoError,
    UnknownError,
};

class TLSContext {
public:
    [[nodiscard]] static std::expected<TLSContext, TlsError>
    load(std::string_view ca_path,
         std::string_view cert_path,
         std::string_view key_path) noexcept;

    [[nodiscard]] const std::string& ca_path() const noexcept   { return ca_path_; }
    [[nodiscard]] const std::string& cert_path() const noexcept { return cert_path_; }
    [[nodiscard]] const std::string& key_path() const noexcept  { return key_path_; }

private:
    TLSContext(std::string ca, std::string cert, std::string key) noexcept
        : ca_path_(std::move(ca)),
          cert_path_(std::move(cert)),
          key_path_(std::move(key)) {}

    std::string ca_path_;
    std::string cert_path_;
    std::string key_path_;
};

} // namespace neurometrix::core4d::infrastructure::security