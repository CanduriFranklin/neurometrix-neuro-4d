#include "tls_context.hpp"

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <filesystem>
#include <memory>

namespace neurometrix::core4d::infrastructure::security {

namespace {

struct BioDeleter { void operator()(BIO* p) const noexcept { if (p) BIO_free(p); } };
using BioPtr = std::unique_ptr<BIO, BioDeleter>;

struct X509Deleter { void operator()(X509* p) const noexcept { if (p) X509_free(p); } };
using X509Ptr = std::unique_ptr<X509, X509Deleter>;

struct PkeyDeleter { void operator()(EVP_PKEY* p) const noexcept { if (p) EVP_PKEY_free(p); } };
using PkeyPtr = std::unique_ptr<EVP_PKEY, PkeyDeleter>;

[[nodiscard]] bool file_exists(const std::string& p) noexcept {
    try { std::error_code ec; return std::filesystem::exists(p, ec) && !ec; }
    catch (...) { return false; }
}

} // anonymous namespace

std::expected<TLSContext, TlsError>
TLSContext::load(std::string_view ca_path,
                 std::string_view cert_path,
                 std::string_view key_path) noexcept {
    try {
        const std::string ca_s{ca_path};
        const std::string cert_s{cert_path};
        const std::string key_s{key_path};

        if (!file_exists(ca_s))   return std::unexpected(TlsError::CaFileMissing);
        if (!file_exists(cert_s)) return std::unexpected(TlsError::CertFileMissing);
        if (!file_exists(key_s))  return std::unexpected(TlsError::KeyFileMissing);

        BioPtr cert_bio{BIO_new_file(cert_s.c_str(), "r")};
        if (!cert_bio) return std::unexpected(TlsError::IoError);
        X509Ptr cert{PEM_read_bio_X509(cert_bio.get(), nullptr, nullptr, nullptr)};
        if (!cert) return std::unexpected(TlsError::InvalidCertFormat);

        BioPtr key_bio{BIO_new_file(key_s.c_str(), "r")};
        if (!key_bio) return std::unexpected(TlsError::IoError);
        PkeyPtr key{PEM_read_bio_PrivateKey(key_bio.get(), nullptr, nullptr, nullptr)};
        if (!key) return std::unexpected(TlsError::InvalidCertFormat);

        if (X509_check_private_key(cert.get(), key.get()) != 1) {
            return std::unexpected(TlsError::CertKeyMismatch);
        }

        BioPtr ca_bio{BIO_new_file(ca_s.c_str(), "r")};
        if (!ca_bio) return std::unexpected(TlsError::IoError);
        X509Ptr ca_cert{PEM_read_bio_X509(ca_bio.get(), nullptr, nullptr, nullptr)};
        if (!ca_cert) return std::unexpected(TlsError::InvalidCertFormat);

        return TLSContext(ca_s, cert_s, key_s);
    } catch (...) {
        return std::unexpected(TlsError::UnknownError);
    }
}

} // namespace neurometrix::core4d::infrastructure::security