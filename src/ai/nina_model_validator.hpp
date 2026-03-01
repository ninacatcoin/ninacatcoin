// Copyright (c) 2026, The ninacatcoin Project
//
// NINA Model Validator — verifies GGUF model integrity at daemon startup.
// Computes SHA-256 of the model file and compares against the hash 
// compiled into the binary via NINA_MODEL_HASH in cryptonote_config.h.
//
// Design decisions:
// - Chunked hashing (4 MB) — no need to load entire model into RAM
// - Size sanity check catches corrupted/truncated files early
// - Graceful degradation: node runs without NINA pre-v18, but cannot mine post-v18
// - CPU-only inference for consensus determinism (no GPU non-determinism)
// - Hash stored globally for coinbase tag 0xCA injection

#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include "cryptonote_config.h"

// Forward declarations — we use Keccak-256 from crypto/ for hashing
// but SHA-256 from OpenSSL for model verification (industry standard for GGUF)
#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#else
#include <openssl/sha.h>
#endif

namespace ninacatcoin_ai {

// Chunk size for streaming hash computation
static constexpr size_t MODEL_HASH_CHUNK_SIZE = 4 * 1024 * 1024; // 4 MB

struct ModelValidationResult {
    bool valid = false;
    std::string computed_hash;   // hex string, 64 chars
    std::string expected_hash;   // from NINA_MODEL_HASH
    uint64_t file_size = 0;
    std::string error;
};

/**
 * Compute SHA-256 hash of a file using chunked reads.
 * Returns hex string (64 chars lowercase).
 */
inline std::string sha256_file(const std::string& filepath, uint64_t& file_size_out) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    // Get file size
    file.seekg(0, std::ios::end);
    file_size_out = static_cast<uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

#ifdef _WIN32
    // Windows CryptoAPI SHA-256
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        return "";
    }
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        CryptReleaseContext(hProv, 0);
        return "";
    }

    std::vector<char> buffer(MODEL_HASH_CHUNK_SIZE);
    while (file.good()) {
        file.read(buffer.data(), MODEL_HASH_CHUNK_SIZE);
        auto bytes_read = file.gcount();
        if (bytes_read > 0) {
            CryptHashData(hHash, reinterpret_cast<const BYTE*>(buffer.data()), 
                         static_cast<DWORD>(bytes_read), 0);
        }
    }

    BYTE hash[32];
    DWORD hash_len = 32;
    CryptGetHashParam(hHash, HP_HASHVAL, hash, &hash_len, 0);
    CryptDestroyHash(hHash);
    CryptReleaseContext(hProv, 0);

    std::ostringstream oss;
    for (int i = 0; i < 32; i++) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();

#else
    // Linux/macOS OpenSSL SHA-256
    SHA256_CTX ctx;
    SHA256_Init(&ctx);

    std::vector<char> buffer(MODEL_HASH_CHUNK_SIZE);
    while (file.good()) {
        file.read(buffer.data(), MODEL_HASH_CHUNK_SIZE);
        auto bytes_read = file.gcount();
        if (bytes_read > 0) {
            SHA256_Update(&ctx, buffer.data(), static_cast<size_t>(bytes_read));
        }
    }

    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_Final(hash, &ctx);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return oss.str();
#endif
}

/**
 * Validate NINA model file against compiled-in hash.
 * Called once at daemon startup.
 * 
 * @param data_dir  Path to the ninacatcoin data directory
 * @return ModelValidationResult with validation status
 */
inline ModelValidationResult validate_nina_model(const std::string& data_dir) {
    ModelValidationResult result;
    result.expected_hash = NINA_MODEL_HASH;

    // Build model path
    std::string model_path = data_dir;
    if (!model_path.empty() && model_path.back() != '/' && model_path.back() != '\\') {
        model_path += '/';
    }
    model_path += NINA_MODEL_FILENAME;

    // Check if placeholder hash (model not yet deployed)
    const std::string zero_hash(64, '0');
    if (result.expected_hash == zero_hash) {
        result.error = "NINA model hash is placeholder (all zeros). "
                       "Model validation skipped — NINA running in stub mode.";
        // Not an error per se, just not deployed yet
        return result;
    }

    // Check file exists
    std::ifstream test(model_path, std::ios::binary);
    if (!test.is_open()) {
        result.error = "NINA model file not found: " + model_path;
        return result;
    }
    test.close();

    // Compute SHA-256
    result.computed_hash = sha256_file(model_path, result.file_size);
    if (result.computed_hash.empty()) {
        result.error = "Failed to compute SHA-256 of model file: " + model_path;
        return result;
    }

    // Size sanity check
    if (result.file_size < NINA_MODEL_MIN_SIZE_BYTES) {
        result.error = "NINA model file too small (" 
                       + std::to_string(result.file_size / (1024*1024)) + " MB). "
                       "Minimum: " + std::to_string(NINA_MODEL_MIN_SIZE_BYTES / (1024*1024)) + " MB. "
                       "File may be corrupted or truncated.";
        return result;
    }

    if (result.file_size > NINA_MODEL_MAX_SIZE_BYTES) {
        result.error = "NINA model file too large ("
                       + std::to_string(result.file_size / (1024*1024)) + " MB). "
                       "Maximum: " + std::to_string(NINA_MODEL_MAX_SIZE_BYTES / (1024*1024)) + " MB.";
        return result;
    }

    // Compare hashes
    if (result.computed_hash != result.expected_hash) {
        result.error = "NINA model hash MISMATCH!\n"
                       "  Expected: " + result.expected_hash + "\n"
                       "  Computed: " + result.computed_hash + "\n"
                       "  File:     " + model_path + " (" 
                       + std::to_string(result.file_size / (1024*1024)) + " MB)";
        return result;
    }

    // Success
    result.valid = true;
    return result;
}

/**
 * Get the full path to the NINA model file.
 */
inline std::string get_nina_model_path(const std::string& data_dir) {
    std::string path = data_dir;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') {
        path += '/';
    }
    path += NINA_MODEL_FILENAME;
    return path;
}

} // namespace ninacatcoin_ai
