/**
 * NINA LLM Model Auto-Downloader
 * 
 * Copyright (c) 2026, The NinaCatCoin Project
 * 
 * Automatically downloads the LLaMA 3.2 3B GGUF model on first daemon start.
 * Uses libcurl (already a project dependency) for HTTPS download with progress.
 * 
 * Features:
 *   - Auto-download from HuggingFace on first run
 *   - Resume interrupted downloads (CURLOPT_RESUME_FROM_LARGE)
 *   - SHA256 integrity verification after download
 *   - Automatic retries (up to 3) on transient network failures
 *   - Progress logging with speed (MB/s) every 5%
 *   - Thread-safe curl initialization
 *   - Atomic temp file rename to prevent corrupt model loads
 * 
 * The download happens ONCE. After that, the model is cached locally.
 * If download fails after all retries, NINA degrades gracefully to embedded ML scoring only.
 */

#pragma once

#include <string>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <array>
#include <iomanip>
#include <sstream>

#include <curl/curl.h>
#include "misc_log_ex.h"
#include "cryptonote_config.h"

// For SHA256 verification — OpenSSL is already a project dependency
#include <openssl/sha.h>

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_llm"

namespace ninacatcoin_ai {

class NinaModelDownloader {
public:
    // ========================================================================
    // Model source configuration
    // ========================================================================
    static constexpr const char* MODEL_URL = NINA_MODEL_DOWNLOAD_URL_PRIMARY;
    static constexpr const char* MODEL_FALLBACK_URL = NINA_MODEL_DOWNLOAD_URL_FALLBACK;
    static constexpr const char* MODEL_FILENAME = NINA_MODEL_FILENAME;
    static constexpr uint64_t MODEL_MIN_SIZE = NINA_MODEL_MIN_SIZE_BYTES;
    static constexpr int MAX_RETRIES = 3;
    static constexpr int RETRY_DELAY_SECONDS = 10;

    // SHA256 of the official fine-tuned NINA model (nina-llama3.2-3b-Q4_K_M.gguf)
    // Verified after every download to prevent tampering
    static constexpr const char* MODEL_SHA256 = NINA_MODEL_HASH;

    // ========================================================================
    // Path helpers
    // ========================================================================

    /**
     * Get the default model directory path
     * Windows: C:\Users\Username\.ninacatcoin\models
     * Linux/Mac: ~/.ninacatcoin/models
     */
    static std::string get_model_dir() {
#ifdef _WIN32
        const char* home = std::getenv("USERPROFILE");
        if (!home) home = std::getenv("HOME");
        if (!home) {
            MERROR("[NINA-LLM-DOWNLOAD] Cannot determine USERPROFILE directory");
            return "";
        }
        return std::string(home) + "\\.ninacatcoin\\models";
#else
        const char* home = std::getenv("HOME");
        if (!home) {
            MERROR("[NINA-LLM-DOWNLOAD] Cannot determine HOME directory");
            return "";
        }
        return std::string(home) + "/.ninacatcoin/models";
#endif
    }

    /**
     * Get the full path to the default model file
     */
    static std::string get_default_model_path() {
        std::string dir = get_model_dir();
        if (dir.empty()) return "";
#ifdef _WIN32
        return dir + "\\" + MODEL_FILENAME;
#else
        return dir + "/" + MODEL_FILENAME;
#endif
    }

    // ========================================================================
    // Model validation
    // ========================================================================

    /**
     * Check if the model file exists and passes basic validation:
     * - File exists
     * - File size >= MODEL_MIN_SIZE (protects against truncated downloads)
     * - First 4 bytes are GGUF magic ("GGUF")
     */
    static bool is_model_available(const std::string& path = "") {
        std::string model_path = path.empty() ? get_default_model_path() : path;
        if (model_path.empty()) return false;
        
        try {
            if (!std::filesystem::exists(model_path)) return false;
            auto file_size = std::filesystem::file_size(model_path);
            if (file_size < MODEL_MIN_SIZE) return false;

            // Verify GGUF magic bytes
            std::ifstream f(model_path, std::ios::binary);
            if (!f.is_open()) return false;
            char magic[4] = {};
            f.read(magic, 4);
            if (magic[0] != 'G' || magic[1] != 'G' || magic[2] != 'U' || magic[3] != 'F') {
                MWARNING("[NINA-LLM-DOWNLOAD] File exists but is not a valid GGUF: " << model_path);
                return false;
            }

            return true;
        } catch (...) {
            return false;
        }
    }

    // ========================================================================
    // SHA256 verification
    // ========================================================================

    /**
     * Compute SHA256 hex digest of a file
     * @return lowercase hex string, or empty string on error
     */
    static std::string compute_sha256(const std::string& filepath) {
        std::ifstream f(filepath, std::ios::binary);
        if (!f.is_open()) return "";

        SHA256_CTX sha_ctx;
        SHA256_Init(&sha_ctx);

        constexpr size_t BUF_SIZE = 1024 * 1024;  // 1MB chunks
        std::vector<char> buffer(BUF_SIZE);
        while (f.read(buffer.data(), BUF_SIZE) || f.gcount() > 0) {
            SHA256_Update(&sha_ctx, buffer.data(), static_cast<size_t>(f.gcount()));
            if (f.gcount() == 0) break;
        }

        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(hash, &sha_ctx);

        std::ostringstream oss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
        }
        return oss.str();
    }

    /**
     * Verify downloaded file against expected SHA256 hash.
     * If MODEL_SHA256 is empty, verification is skipped (always returns true).
     */
    static bool verify_sha256(const std::string& filepath) {
        if (MODEL_SHA256[0] == '\0') {
            MINFO("[NINA-LLM-DOWNLOAD] SHA256 verification skipped (no expected hash configured)");
            return true;
        }

        MINFO("[NINA-LLM-DOWNLOAD] Computing SHA256 of downloaded model...");
        std::string actual = compute_sha256(filepath);
        if (actual.empty()) {
            MERROR("[NINA-LLM-DOWNLOAD] Failed to compute SHA256");
            return false;
        }

        std::string expected(MODEL_SHA256);
        if (actual == expected) {
            MINFO("[NINA-LLM-DOWNLOAD] ✓ SHA256 verified: " << actual.substr(0, 16) << "...");
            return true;
        } else {
            MERROR("[NINA-LLM-DOWNLOAD] ✗ SHA256 MISMATCH!");
            MERROR("[NINA-LLM-DOWNLOAD]   Expected: " << expected);
            MERROR("[NINA-LLM-DOWNLOAD]   Got:      " << actual);
            return false;
        }
    }

    // ========================================================================
    // Thread-safe curl initialization
    // ========================================================================

    /**
     * Initialize libcurl globally (thread-safe, called once).
     * Must be called before any curl_easy_init() in a multi-threaded program.
     */
    static void init_curl_once() {
        static std::once_flag curl_init_flag;
        std::call_once(curl_init_flag, []() {
            CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
            if (res != CURLE_OK) {
                MERROR("[NINA-LLM-DOWNLOAD] curl_global_init failed: " << curl_easy_strerror(res));
            } else {
                MINFO("[NINA-LLM-DOWNLOAD] libcurl initialized (thread-safe)");
            }
        });
    }

    // ========================================================================
    // Core download logic (single attempt, with resume)
    // ========================================================================

    /**
     * Perform a single download attempt. Supports resuming from a partial .downloading file.
     * 
     * @param model_path Final destination path
     * @param temp_path  Temporary download path (.downloading)
     * @return true if download completed successfully
     */
    static bool download_attempt(const std::string& model_path, const std::string& temp_path) {
        init_curl_once();

        CURL* curl = curl_easy_init();
        if (!curl) {
            MERROR("[NINA-LLM-DOWNLOAD] Failed to initialize libcurl session");
            return false;
        }

        // Check for existing partial download to resume
        uint64_t resume_from = 0;
        FILE* fp = nullptr;

        if (std::filesystem::exists(temp_path)) {
            try {
                resume_from = std::filesystem::file_size(temp_path);
                if (resume_from > 0) {
                    fp = fopen(temp_path.c_str(), "ab");  // Append mode for resume
                    if (fp) {
                        MINFO("[NINA-LLM-DOWNLOAD] Resuming download from " 
                              << (resume_from / (1024 * 1024)) << " MB");
                    } else {
                        resume_from = 0;  // Fall back to fresh download
                    }
                }
            } catch (...) {
                resume_from = 0;
            }
        }

        // If no resume, start fresh
        if (!fp) {
            fp = fopen(temp_path.c_str(), "wb");
            resume_from = 0;
        }

        if (!fp) {
            MERROR("[NINA-LLM-DOWNLOAD] Cannot create/open file: " << temp_path);
            curl_easy_cleanup(curl);
            return false;
        }

        // Progress tracking state
        ProgressState pstate;
        pstate.last_log_percent = 0;
        pstate.resume_offset = resume_from;
        pstate.start_time = std::chrono::steady_clock::now();

        // ---- Configure curl options ----
        curl_easy_setopt(curl, CURLOPT_URL, MODEL_URL);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 7200L);        // 2 hours max (slow connections)
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 512L);  // 512 B/s minimum
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 120L);   // for 2 minutes
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "NinaCatCoin-Daemon/1.0");
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);        // Fail on HTTP >= 400

        // SSL configuration
#ifdef _WIN32
        // On Windows, use the native Windows certificate store (Schannel/WinSSL)
        // This avoids the "SSL certificate problem: unable to get local issuer certificate" 
        // error that occurs when OpenSSL can't find a CA bundle file
        curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, CURLSSLOPT_NATIVE_CA);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

        // Also try to find a CA bundle next to the executable as fallback
        {
            std::filesystem::path exe_dir = std::filesystem::current_path();
            std::filesystem::path ca_bundle = exe_dir / "curl-ca-bundle.crt";
            if (std::filesystem::exists(ca_bundle)) {
                curl_easy_setopt(curl, CURLOPT_CAINFO, ca_bundle.string().c_str());
                MINFO("[NINA-LLM-DOWNLOAD] Using CA bundle: " << ca_bundle.string());
            }
        }
#else
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
#endif

        // Resume from offset if applicable
        if (resume_from > 0) {
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(resume_from));
        }

        // Progress callback
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pstate);

        MINFO("[NINA-LLM-DOWNLOAD] Connecting to HuggingFace...");

        // ---- Execute download ----
        CURLcode res = curl_easy_perform(curl);
        fclose(fp);

        if (res != CURLE_OK) {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            curl_easy_cleanup(curl);

            // If HTTP 416 (Range Not Satisfiable), delete temp and retry fresh
            if (http_code == 416) {
                MWARNING("[NINA-LLM-DOWNLOAD] Server rejected resume range, restarting download");
                try { std::filesystem::remove(temp_path); } catch (...) {}
            } else {
                MERROR("[NINA-LLM-DOWNLOAD] Download failed: " << curl_easy_strerror(res)
                       << " (HTTP " << http_code << ")");
                // Keep temp file for potential resume on next attempt
            }
            return false;
        }

        // Get final HTTP code
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        double total_time = 0;
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);

        curl_easy_cleanup(curl);

        // HTTP 200 = full download, HTTP 206 = partial (resume) — both are OK
        if (http_code != 200 && http_code != 206) {
            MERROR("[NINA-LLM-DOWNLOAD] Unexpected HTTP response: " << http_code);
            return false;
        }

        // ---- Verify downloaded file ----
        try {
            auto file_size = std::filesystem::file_size(temp_path);
            if (file_size < MODEL_MIN_SIZE) {
                MERROR("[NINA-LLM-DOWNLOAD] Downloaded file too small: " 
                       << (file_size / (1024 * 1024)) << " MB (expected >= " 
                       << (MODEL_MIN_SIZE / (1024 * 1024)) << " MB)");
                std::filesystem::remove(temp_path);
                return false;
            }

            double size_gb = static_cast<double>(file_size) / (1024.0 * 1024.0 * 1024.0);
            double speed_mbps = (total_time > 0) 
                ? (static_cast<double>(file_size - resume_from) / (1024.0 * 1024.0)) / total_time 
                : 0;

            MINFO("[NINA-LLM-DOWNLOAD] Download complete: " 
                  << std::fixed << std::setprecision(2) << size_gb << " GB in " 
                  << static_cast<int>(total_time) << "s (avg " 
                  << std::fixed << std::setprecision(1) << speed_mbps << " MB/s)");

        } catch (const std::exception& e) {
            MERROR("[NINA-LLM-DOWNLOAD] Error checking downloaded file: " << e.what());
            return false;
        }

        // ---- SHA256 verification ----
        if (!verify_sha256(temp_path)) {
            MERROR("[NINA-LLM-DOWNLOAD] Integrity check failed — deleting corrupt file");
            try { std::filesystem::remove(temp_path); } catch (...) {}
            return false;
        }

        // ---- Atomic rename to final path ----
        try {
            std::filesystem::rename(temp_path, model_path);
        } catch (const std::exception& e) {
            MERROR("[NINA-LLM-DOWNLOAD] Failed to rename temp file: " << e.what());
            // Try copy + delete as fallback (cross-device moves)
            try {
                std::filesystem::copy_file(temp_path, model_path, 
                    std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(temp_path);
                MINFO("[NINA-LLM-DOWNLOAD] Used copy+delete fallback for finalization");
            } catch (const std::exception& e2) {
                MERROR("[NINA-LLM-DOWNLOAD] Fatal: cannot finalize model file: " << e2.what());
                return false;
            }
        }

        return true;
    }

    // ========================================================================
    // Download with retries
    // ========================================================================

    /**
     * Download the model file with automatic retries.
     * Supports resuming interrupted downloads across retries.
     * 
     * @param dest_path Where to save the model (default: ~/.ninacatcoin/models/MODEL_FILENAME)
     * @return true if download successful and file is valid
     */
    static bool download_model(const std::string& dest_path = "") {
        std::string model_path = dest_path.empty() ? get_default_model_path() : dest_path;
        if (model_path.empty()) {
            MERROR("[NINA-LLM-DOWNLOAD] Cannot determine model path (HOME not set)");
            return false;
        }

        // Already have it?
        if (is_model_available(model_path)) {
            MINFO("[NINA-LLM-DOWNLOAD] Model already available and valid: " << model_path);
            return true;
        }

        // Create directory
        try {
            std::string dir = std::filesystem::path(model_path).parent_path().string();
            std::filesystem::create_directories(dir);
        } catch (const std::exception& e) {
            MERROR("[NINA-LLM-DOWNLOAD] Cannot create model directory: " << e.what());
            return false;
        }

        MINFO("╔══════════════════════════════════════════════════════════════╗");
        MINFO("║  NINA AI — Auto-downloading LLM model (first run only)     ║");
        MINFO("║  Model: NINA fine-tuned Llama 3.2 3B (Q4_K_M, ~1.9GB)     ║");
        MINFO("║  Source: HuggingFace (ninacatcoin/nina-model)              ║");
        MINFO("║  Resume: YES — interrupted downloads will continue         ║");
        MINFO("║  Retries: up to " << MAX_RETRIES << " attempts on failure                      ║");
        MINFO("╚══════════════════════════════════════════════════════════════╝");

        std::string temp_path = model_path + ".downloading";

        for (int attempt = 1; attempt <= MAX_RETRIES; ++attempt) {
            if (attempt > 1) {
                MINFO("[NINA-LLM-DOWNLOAD] Retry " << attempt << "/" << MAX_RETRIES 
                      << " — waiting " << RETRY_DELAY_SECONDS << "s before retrying...");
                std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
            }

            MINFO("[NINA-LLM-DOWNLOAD] Download attempt " << attempt << "/" << MAX_RETRIES);

            if (download_attempt(model_path, temp_path)) {
                MINFO("╔══════════════════════════════════════════════════════════════╗");
                MINFO("║  ✅ NINA AI model downloaded and verified!                  ║");
                MINFO("║  Location: " << model_path);
                MINFO("║  NINA AI intelligence is now fully operational              ║");
                MINFO("╚══════════════════════════════════════════════════════════════╝");
                return true;
            }

            MWARNING("[NINA-LLM-DOWNLOAD] Attempt " << attempt << " failed");
        }

        // All retries exhausted
        MERROR("[NINA-LLM-DOWNLOAD] All " << MAX_RETRIES << " download attempts failed");
        MERROR("[NINA-LLM-DOWNLOAD] You can manually download the model:");
        MERROR("[NINA-LLM-DOWNLOAD]   wget -O " << model_path << " " << MODEL_URL);
        MERROR("[NINA-LLM-DOWNLOAD]   Or run: utils/nina-download-model.sh");

        // Clean up temp file if it's too small (likely corrupt)
        try {
            if (std::filesystem::exists(temp_path)) {
                auto sz = std::filesystem::file_size(temp_path);
                if (sz < MODEL_MIN_SIZE) {
                    MINFO("[NINA-LLM-DOWNLOAD] Keeping partial download ("
                          << (sz / (1024 * 1024)) << " MB) for resume on next start");
                }
            }
        } catch (...) {}

        return false;
    }

    // ========================================================================
    // Main entry point
    // ========================================================================

    /**
     * Ensure model is available, downloading if necessary.
     * This is the main entry point — called from NinaLLMEngine::initialize().
     * 
     * @param model_path Path to check/download to
     * @return true if model is available (was present or successfully downloaded)
     */
    static bool ensure_model_available(const std::string& model_path = "") {
        std::string path = model_path.empty() ? get_default_model_path() : model_path;
        
        if (is_model_available(path)) {
            try {
                auto size_mb = std::filesystem::file_size(path) / (1024 * 1024);
                MINFO("[NINA-LLM] Model found and valid: " << path << " (" << size_mb << " MB)");
            } catch (...) {
                MINFO("[NINA-LLM] Model found: " << path);
            }
            return true;
        }

        MINFO("[NINA-LLM] Model not found or invalid, starting auto-download...");
        return download_model(path);
    }

private:
    // ========================================================================
    // Progress tracking state
    // ========================================================================

    struct ProgressState {
        uint64_t last_log_percent = 0;
        uint64_t resume_offset = 0;
        std::chrono::steady_clock::time_point start_time;
    };

    // ========================================================================
    // libcurl callbacks
    // ========================================================================

    /// Write callback — writes received data to file
    static size_t write_callback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
        return fwrite(ptr, size, nmemb, stream);
    }

    /// Progress callback — logs every 5% with download speed
    static int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                  curl_off_t /*ultotal*/, curl_off_t /*ulnow*/) {
        if (dltotal <= 0) return 0;
        
        auto* ps = static_cast<ProgressState*>(clientp);

        // Calculate percentage including resume offset
        uint64_t total = static_cast<uint64_t>(dltotal) + ps->resume_offset;
        uint64_t now = static_cast<uint64_t>(dlnow) + ps->resume_offset;
        uint64_t percent = (total > 0) ? (now * 100 / total) : 0;
        uint64_t threshold = (percent / 5) * 5;  // Round down to nearest 5%

        if (threshold > ps->last_log_percent && threshold > 0) {
            ps->last_log_percent = threshold;

            double dl_mb = static_cast<double>(now) / (1024.0 * 1024.0);
            double total_mb = static_cast<double>(total) / (1024.0 * 1024.0);

            // Calculate speed
            auto elapsed = std::chrono::steady_clock::now() - ps->start_time;
            double elapsed_sec = std::chrono::duration<double>(elapsed).count();
            double speed_mbps = (elapsed_sec > 0) 
                ? (static_cast<double>(dlnow) / (1024.0 * 1024.0)) / elapsed_sec 
                : 0;

            // ETA calculation
            double remaining_mb = total_mb - dl_mb;
            int eta_sec = (speed_mbps > 0.1) ? static_cast<int>(remaining_mb / speed_mbps) : -1;

            if (eta_sec >= 0) {
                int eta_min = eta_sec / 60;
                int eta_s = eta_sec % 60;
                MINFO("[NINA-LLM-DOWNLOAD] " << threshold << "% — " 
                      << static_cast<int>(dl_mb) << "/" << static_cast<int>(total_mb) 
                      << " MB | " << std::fixed << std::setprecision(1) << speed_mbps 
                      << " MB/s | ETA " << eta_min << "m" << eta_s << "s");
            } else {
                MINFO("[NINA-LLM-DOWNLOAD] " << threshold << "% — " 
                      << static_cast<int>(dl_mb) << "/" << static_cast<int>(total_mb) 
                      << " MB | " << std::fixed << std::setprecision(1) << speed_mbps << " MB/s");
            }
        }
        return 0;  // 0 = continue, non-zero = abort
    }
};

} // namespace ninacatcoin_ai
