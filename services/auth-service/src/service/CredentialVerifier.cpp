#include "service/CredentialVerifier.hpp"
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <stdexcept>

std::string CredentialVerifier::sha256(const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) throw std::runtime_error("EVP_MD_CTX_new failed");

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(ctx, data.data(), data.size()) != 1 ||
        EVP_DigestFinal_ex(ctx, hash, &len) != 1) {
        EVP_MD_CTX_free(ctx);
        throw std::runtime_error("EVP sha256 failed");
    }
    EVP_MD_CTX_free(ctx);

    std::ostringstream oss;
    for (unsigned int i = 0; i < len; ++i)
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(hash[i]);
    return oss.str();
}

bool CredentialVerifier::verifyPassword(const User& user,
                                        const std::string& password) const {
    if (user.pass_algo == "plain") {
        return user.pass_hash == password;
    } else if (user.pass_algo == "sha256") {
        return user.pass_hash == sha256(password);
    }

    // algorithme non supporté
    return false;
}
