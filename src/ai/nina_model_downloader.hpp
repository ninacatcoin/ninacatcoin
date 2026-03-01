// Copyright (c) 2026, The ninacatcoin Project
//
// NINA Model Auto-Downloader — fetches the GGUF model on first run.
//
// When a new node starts and nina_model.gguf is missing from data_dir,
// this module downloads it from the compiled-in URLs (HuggingFace primary,
// GitHub Releases fallback).  After download it verifies the SHA-256
// hash matches NINA_MODEL_HASH before the daemon proceeds.
//
// Design:
//   - Uses system curl/wget (available on every Linux VPS and modern Windows)
//   - Progress output to stdout so operators see download status
//   - Downloads to a .tmp file, verifies, then renames — no partial files
//   - Retry logic: primary URL first, then fallback
//   - Called once from nina_advanced_inline.hpp during daemon init

#pragma once

#include <string>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include "cryptonote_config.h"
#include "ai/nina_model_validator.hpp"

namespace ninacatcoin_ai {

/**
 * Check if a command is available on the system.
 */
inline bool command_exists(const std::string& cmd) {
#ifdef _WIN32
    std::string check = "where " + cmd + " >nul 2>&1";
#else
    std::string check = "which " + cmd + " > /dev/null 2>&1";
#endif
    return (std::system(check.c_str()) == 0);
}

/**
 * Download a file from url to dest using curl or wget.
 * Returns true if the download command exited successfully.
 */
inline bool download_file(const std::string& url, const std::string& dest) {
    std::string cmd;

    if (command_exists("curl")) {
        // -L follows redirects, -f fails on HTTP errors, --progress-bar for visual feedback
        cmd = "curl -L -f --progress-bar -o \"" + dest + "\" \"" + url + "\"";
    } else if (command_exists("wget")) {
        cmd = "wget --no-check-certificate -q --show-progress -O \"" + dest + "\" \"" + url + "\"";
    } else {
        std::cerr << "\n[NINA] ERROR: Neither curl nor wget found on this system.\n"
                  << "[NINA] Please install curl or wget, or manually download the model:\n"
                  << "[NINA]   " << url << "\n"
                  << "[NINA] and place it as nina_model.gguf in the data directory.\n";
        return false;
    }

    std::cout << "\n[NINA] Downloading model with: " << (cmd.substr(0, 4) == "curl" ? "curl" : "wget") << "\n";
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

/**
 * Attempt to download and validate the NINA model.
 *
 * @param data_dir   NinaCatCoin data directory (e.g. ~/.ninacatcoin)
 * @return true if model is now present and valid
 */
inline bool ensure_nina_model(const std::string& data_dir) {
    // Build target path
    std::string model_path = get_nina_model_path(data_dir);

    // If model already exists, just validate (don't re-download)
    {
        std::ifstream test(model_path, std::ios::binary);
        if (test.is_open()) {
            test.close();
            auto result = validate_nina_model(data_dir);
            if (result.valid) {
                return true;  // Already have valid model
            }
            // Model exists but invalid — remove and re-download
            std::cerr << "[NINA] Existing model failed validation: " << result.error << "\n";
            std::cerr << "[NINA] Removing corrupt model and re-downloading...\n";
            std::remove(model_path.c_str());
        }
    }

    // Check if hash is placeholder (pre-deployment)
    const std::string zero_hash(64, '0');
    if (std::string(NINA_MODEL_HASH) == zero_hash) {
        std::cout << "[NINA] Model hash is placeholder — skipping download (pre-deployment mode)\n";
        return false;
    }

    // Temp file for atomic download
    std::string tmp_path = model_path + ".downloading";

    // URLs to try in order
    const std::string urls[] = {
        NINA_MODEL_DOWNLOAD_URL_PRIMARY,
        NINA_MODEL_DOWNLOAD_URL_FALLBACK
    };
    const char* url_names[] = { "HuggingFace", "GitHub Releases" };

    std::cout << "\n"
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║         NINA — Downloading AI Model (first run)             ║\n"
              << "║         Size: ~1.9 GB — please wait...                      ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n\n";

    bool downloaded = false;
    for (int i = 0; i < 2; i++) {
        std::cout << "[NINA] Trying " << url_names[i] << "...\n";
        std::cout << "[NINA] URL: " << urls[i] << "\n";

        // Remove any partial tmp file
        std::remove(tmp_path.c_str());

        if (download_file(urls[i], tmp_path)) {
            downloaded = true;
            std::cout << "[NINA] Download complete from " << url_names[i] << "\n";
            break;
        } else {
            std::cerr << "[NINA] Download failed from " << url_names[i] << "\n";
        }
    }

    if (!downloaded) {
        std::cerr << "\n[NINA] ERROR: Could not download model from any source.\n"
                  << "[NINA] Please download manually:\n"
                  << "[NINA]   " << NINA_MODEL_DOWNLOAD_URL_PRIMARY << "\n"
                  << "[NINA] and save as: " << model_path << "\n\n";
        std::remove(tmp_path.c_str());
        return false;
    }

    // Verify hash of downloaded file
    std::cout << "[NINA] Verifying SHA-256 hash...\n";
    uint64_t file_size = 0;
    std::string computed = sha256_file(tmp_path, file_size);

    if (computed.empty()) {
        std::cerr << "[NINA] ERROR: Failed to compute hash of downloaded file.\n";
        std::remove(tmp_path.c_str());
        return false;
    }

    if (computed != NINA_MODEL_HASH) {
        std::cerr << "[NINA] ERROR: Hash mismatch after download!\n"
                  << "[NINA]   Expected: " << NINA_MODEL_HASH << "\n"
                  << "[NINA]   Got:      " << computed << "\n"
                  << "[NINA] The download may be corrupted. Please try again.\n";
        std::remove(tmp_path.c_str());
        return false;
    }

    // Size sanity check
    if (file_size < NINA_MODEL_MIN_SIZE_BYTES || file_size > NINA_MODEL_MAX_SIZE_BYTES) {
        std::cerr << "[NINA] ERROR: Downloaded file size out of bounds ("
                  << (file_size / (1024 * 1024)) << " MB).\n";
        std::remove(tmp_path.c_str());
        return false;
    }

    // Atomic rename: tmp -> final
    std::remove(model_path.c_str());  // Remove any leftover
    if (std::rename(tmp_path.c_str(), model_path.c_str()) != 0) {
        std::cerr << "[NINA] ERROR: Failed to rename downloaded file to " << model_path << "\n";
        return false;
    }

    std::cout << "\n[NINA] ✓ Model downloaded and verified successfully!\n"
              << "[NINA]   File: " << model_path << "\n"
              << "[NINA]   Size: " << (file_size / (1024 * 1024)) << " MB\n"
              << "[NINA]   SHA-256: " << computed << "\n\n";

    return true;
}

} // namespace ninacatcoin_ai
