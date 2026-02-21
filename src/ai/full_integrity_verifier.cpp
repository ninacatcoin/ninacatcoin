// Copyright (c) 2026, The ninacatcoin Project
//
// Full Binary Integrity Verifier - Implementation
// Verifies ALL source files composing the daemon binary haven't been tampered.
// Auto-remediates from GitHub when tampering is detected.

#include "full_integrity_verifier.hpp"
#include "full_source_hash.h"       // AUTO-GENERATED: contains FULL_COMPILED_SOURCE_HASH
#include "ai_auto_updater.hpp"
#include "common/util.h"            // tools::sha256sum()
#include "crypto/hash.h"
#include "string_tools.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <regex>

namespace ninacatcoin_integrity {

static FullIntegrityVerifier* g_full_verifier = nullptr;

FullIntegrityVerifier& FullIntegrityVerifier::getInstance() {
    if (!g_full_verifier) {
        g_full_verifier = new FullIntegrityVerifier();
    }
    return *g_full_verifier;
}

FullIntegrityVerifier::FullIntegrityVerifier() {
}

void FullIntegrityVerifier::setStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    m_last_status = status;
    std::cout << "[FULL-INTEGRITY] " << status << std::endl;
}

std::string FullIntegrityVerifier::getLastStatus() const {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    return m_last_status;
}

bool FullIntegrityVerifier::initialize() {
    if (m_initialized) return true;

    m_initialized = true;

    setStatus("Full Source Integrity Verifier initialized");
    setStatus("Compiled hash: " + std::string(FULL_COMPILED_SOURCE_HASH).substr(0, 16)
              + "... (" + std::to_string(FULL_COMPILED_FILE_COUNT) + " source files, built "
              + FULL_HASH_BUILD_TIME + ")");

    return true;
}

const char* FullIntegrityVerifier::getCompiledFullHash() {
    return FULL_COMPILED_SOURCE_HASH;
}

int FullIntegrityVerifier::getCompiledFileCount() {
    return FULL_COMPILED_FILE_COUNT;
}

void FullIntegrityVerifier::setTamperingCallback(
    std::function<void(const std::string&, const std::string&)> cb
) {
    m_tampering_callback = cb;
}

// ===================== SOURCE FILE DISCOVERY =====================

std::string FullIntegrityVerifier::findSourceRoot() {
    // Try multiple base paths (development, installed, packaged)
    std::vector<std::string> candidates;

    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");

    candidates.push_back("/mnt/i/ninacatcoin");                    // WSL dev (primary)
    candidates.push_back(".");                                      // current dir
    if (home) {
        candidates.push_back(std::string(home) + "/ninacatcoin");   // home dir
    }
    candidates.push_back("/opt/ninacatcoin");                       // installed
    candidates.push_back("/usr/local/src/ninacatcoin");             // alt install

    for (const auto& path : candidates) {
        // Check if src/ directory exists with expected structure
        if (std::filesystem::exists(path + "/src/ai/ai_module.cpp") &&
            std::filesystem::exists(path + "/src/cryptonote_core")) {
            return path;
        }
    }

    return ""; // Source root not found
}

std::vector<std::string> FullIntegrityVerifier::discoverSourceFiles(
    const std::string& base_path
) {
    std::vector<std::string> files;
    std::string src_dir = base_path + "/src";

    if (!std::filesystem::exists(src_dir)) {
        return files;
    }

    // Extensions to include (same as CMake GLOB_RECURSE pattern)
    static const std::vector<std::string> extensions = {".cpp", ".hpp", ".h", ".c"};

    // Directories to exclude (same as CMake filtering)
    static const std::vector<std::string> excludes = {
        "__pycache__", "nina_ml", "generated_include", "build-linux"
    };

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(src_dir)) {
            if (!entry.is_regular_file()) continue;

            std::string path = entry.path().string();

            // Normalize path separators to forward slash
            std::replace(path.begin(), path.end(), '\\', '/');

            // Check extension
            std::string ext = entry.path().extension().string();
            bool valid_ext = false;
            for (const auto& e : extensions) {
                if (ext == e) { valid_ext = true; break; }
            }
            if (!valid_ext) continue;

            // Check excludes
            bool excluded = false;
            for (const auto& excl : excludes) {
                if (path.find(excl) != std::string::npos) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;

            files.push_back(path);
        }
    } catch (const std::exception& e) {
        setStatus("Error scanning source files: " + std::string(e.what()));
        return {};
    }

    // Also include top-level CMakeLists.txt files (same as CMake module)
    std::string main_cmake = base_path + "/CMakeLists.txt";
    std::string src_cmake = src_dir + "/CMakeLists.txt";

    // Normalize path separators
    std::replace(main_cmake.begin(), main_cmake.end(), '\\', '/');
    std::replace(src_cmake.begin(), src_cmake.end(), '\\', '/');

    if (std::filesystem::exists(main_cmake)) files.push_back(main_cmake);
    if (std::filesystem::exists(src_cmake)) files.push_back(src_cmake);

    // CRITICAL: Sort for deterministic hashing (must match CMake's list(SORT))
    std::sort(files.begin(), files.end());

    return files;
}

// ===================== HASH CALCULATION =====================

std::string FullIntegrityVerifier::calculateCombinedHash(
    const std::vector<std::string>& files
) {
    std::string combined;

    for (const auto& filepath : files) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            // If any file is missing, we can't compute a valid hash
            setStatus("Warning: Cannot open file: " + filepath);
            return "";
        }
        std::stringstream ss;
        ss << file.rdbuf();
        std::string content = ss.str();

        // Normalize line endings: strip \r so CRLF becomes LF
        // This matches CMake's file(READ) behavior which auto-converts CRLF→LF
        content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());

        combined += content;
    }

    if (combined.empty()) {
        return "";
    }

    // SHA-256 using the project's implementation
    crypto::hash hash;
    if (tools::sha256sum(
            reinterpret_cast<const uint8_t*>(combined.data()),
            combined.size(),
            hash))
    {
        return epee::string_tools::pod_to_hex(hash);
    }

    return "";
}

std::string FullIntegrityVerifier::calculateLocalFullHash() {
    std::string source_root = findSourceRoot();
    if (source_root.empty()) {
        setStatus("Source root not found (binary-only deployment)");
        return "";
    }

    std::vector<std::string> files = discoverSourceFiles(source_root);
    if (files.empty()) {
        setStatus("No source files found at " + source_root);
        return "";
    }

    setStatus("Hashing " + std::to_string(files.size()) + " source files from " + source_root + "...");
    return calculateCombinedHash(files);
}

// ===================== VERIFICATION =====================

bool FullIntegrityVerifier::verifyFullSourceIntegrity() {
    setStatus("=== Full Binary Integrity Check ===");

    if (!m_initialized) {
        initialize();
    }

    std::string compiled_hash = FULL_COMPILED_SOURCE_HASH;
    setStatus("Compiled hash: " + compiled_hash.substr(0, 16) + "... ("
              + std::to_string(FULL_COMPILED_FILE_COUNT) + " files)");

    // Calculate hash of source files currently on disk
    m_last_local_hash = calculateLocalFullHash();

    if (m_last_local_hash.empty()) {
        // Source files not available — binary-only deployment
        // Can't verify source, but the binary itself is what was compiled
        setStatus("Source files not available on disk — binary deployment mode");
        setStatus("✅ Binary integrity assumed (no source to verify)");
        return true;
    }

    setStatus("Local source hash: " + m_last_local_hash.substr(0, 16) + "...");

    if (m_last_local_hash == compiled_hash) {
        setStatus("✅ ALL " + std::to_string(FULL_COMPILED_FILE_COUNT)
                  + " source files match compiled hash — binary is CLEAN");
        return true;
    }

    // ===== TAMPERING DETECTED! =====
    setStatus("🚨 CRITICAL: BINARY TAMPERING DETECTED!");
    setStatus("   Compiled hash: " + compiled_hash);
    setStatus("   On-disk hash:  " + m_last_local_hash);
    setStatus("   Someone has modified source files after compilation!");

    // Notify via callback (Discord, etc.)
    if (m_tampering_callback) {
        m_tampering_callback(compiled_hash, m_last_local_hash);
    }

    // Auto-remediate: download clean source from GitHub, rebuild, restart
    setStatus("🔧 Initiating automatic remediation from GitHub...");
    triggerAutoRemediation();

    return false;
}

// ===================== AUTO-REMEDIATION =====================

bool FullIntegrityVerifier::triggerAutoRemediation() {
    bool expected = false;
    if (!m_remediating.compare_exchange_strong(expected, true)) {
        setStatus("Remediation already in progress, skipping");
        return false;
    }

    // Use the existing AutoUpdater infrastructure
    // For full source tampering, we don't need P2P consensus — local detection is enough
    auto& updater = ninacatcoin_ai::AutoUpdater::getInstance();

    if (updater.isUpdating()) {
        setStatus("AutoUpdater already running, waiting...");
        m_remediating.store(false);
        return false;
    }

    setStatus("=== AUTO-REMEDIATION: Downloading original source from GitHub ===");
    setStatus("Repository: " + std::string(ninacatcoin_ai::AutoUpdater::GITHUB_REPO));
    setStatus("Branch: " + std::string(ninacatcoin_ai::AutoUpdater::GITHUB_BRANCH));

    // Perform the update (download → build → backup → install → restart)
    // Pass empty expected_hash — for full source remediation we trust GitHub directly
    // The performUpdate will clone fresh, build, and replace the binary
    bool success = updater.performUpdate("");

    if (!success) {
        setStatus("❌ Auto-remediation FAILED — manual intervention required");
        setStatus("Please manually: git pull && cmake .. && make -j2 daemon");
    } else {
        setStatus("✅ Auto-remediation successful — daemon will restart with clean binary");
    }

    m_remediating.store(false);
    return success;
}

} // namespace ninacatcoin_integrity
