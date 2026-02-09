#include "service/CryptoService.hpp"

#include <openssl/sha.h>
#include <sstream>
#include <iomanip>

std::string CryptoService::sha256(const std::string& input)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];

    SHA256_CTX ctx{};
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, input.data(), input.size());
    SHA256_Final(hash, &ctx);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char c : hash) {
        oss << std::setw(2) << static_cast<int>(c);
    }

    return oss.str();
}
