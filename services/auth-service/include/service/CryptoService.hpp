#pragma once

#include <string>

// Petit service de hash utilisé par CredentialVerifier.
class CryptoService {
public:
    // SHA-256 hexadécimal, utilisé quand password_algo == "sha256"
    static std::string sha256(const std::string& input);
};
