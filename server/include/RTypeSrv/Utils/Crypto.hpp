#pragma once

#include <array>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace rtype::srv::utils {

class Crypto
{
    public:
        static std::vector<uint8_t> generateSecureRandom(size_t length)
        {
            std::vector<uint8_t> buffer(length);
            if (RAND_bytes(buffer.data(), static_cast<int>(length)) != 1) {
                throw std::runtime_error("Failed to generate secure random bytes");
            }
            return buffer;
        }

        static std::vector<uint8_t> hmacSHA256(const std::vector<uint8_t> &key, const std::vector<uint8_t> &data)
        {
            unsigned int outlen = static_cast<unsigned int>(EVP_MD_size(EVP_sha256()));
            std::vector<uint8_t> hash(static_cast<size_t>(outlen));
            if (!HMAC(EVP_sha256(), key.data(), static_cast<int>(key.size()), data.data(), static_cast<size_t>(data.size()), hash.data(),
                    &outlen)) {
                throw std::runtime_error("HMAC computation failed");
            }
            hash.resize(static_cast<size_t>(outlen));
            return hash;
        }

        static std::array<uint8_t, 32> deriveKey(const std::vector<uint8_t> &ikm, const std::vector<uint8_t> &salt)
        {
            std::array<uint8_t, 32> okm{};
            size_t s = okm.size();
            EVP_PKEY_CTX *pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
            if (!pctx) {
                unsigned long err = ERR_get_error();
                throw std::runtime_error(
                    std::string("HKDF: EVP_PKEY_CTX_new_id failed: ") + (err ? ERR_reason_error_string(err) : "unknown"));
            }
            int rc = EVP_PKEY_derive_init(pctx);
            if (rc <= 0) {
                unsigned long err = ERR_get_error();
                EVP_PKEY_CTX_free(pctx);
                throw std::runtime_error(
                    std::string("HKDF: EVP_PKEY_derive_init failed: ") + (err ? ERR_reason_error_string(err) : "unknown"));
            }
            rc = EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256());
            if (rc <= 0) {
                unsigned long err = ERR_get_error();
                EVP_PKEY_CTX_free(pctx);
                throw std::runtime_error(
                    std::string("HKDF: EVP_PKEY_CTX_set_hkdf_md failed: ") + (err ? ERR_reason_error_string(err) : "unknown"));
            }
            if (!salt.empty()) {
                rc = EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt.data(), static_cast<int>(salt.size()));
                if (rc <= 0) {
                    unsigned long err = ERR_get_error();
                    EVP_PKEY_CTX_free(pctx);
                    throw std::runtime_error(
                        std::string("HKDF: EVP_PKEY_CTX_set1_hkdf_salt failed: ") + (err ? ERR_reason_error_string(err) : "unknown"));
                }
            }
            if (ikm.empty()) {
                EVP_PKEY_CTX_free(pctx);
                throw std::runtime_error("HKDF: input key material (ikm) is empty");
            }
            rc = EVP_PKEY_CTX_set1_hkdf_key(pctx, ikm.data(), static_cast<int>(ikm.size()));
            if (rc <= 0) {
                unsigned long err = ERR_get_error();
                EVP_PKEY_CTX_free(pctx);
                throw std::runtime_error(
                    std::string("HKDF: EVP_PKEY_CTX_set1_hkdf_key failed: ") + (err ? ERR_reason_error_string(err) : "unknown"));
            }
            rc = EVP_PKEY_derive(pctx, okm.data(), &s);
            if (rc <= 0) {
                unsigned long err = ERR_get_error();
                EVP_PKEY_CTX_free(pctx);
                throw std::runtime_error(std::string("HKDF: EVP_PKEY_derive failed: ") + (err ? ERR_reason_error_string(err) : "unknown"));
            }
            EVP_PKEY_CTX_free(pctx);
            return okm;
        }
};

}// namespace rtype::srv::utils
