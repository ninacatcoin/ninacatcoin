// Copyright (c) 2026, The ninacatcoin Project
//
// Full Binary Integrity Verifier
// Verifies that ALL source files composing the daemon binary haven't been
// tampered with. Uses compile-time SHA-256 hash of every .cpp/.hpp/.h/.c
// file in src/ (excluding python ML service and generated files).
//
// When tampering is detected:
//   1. Logs the violation with details
//   2. Sends Discord alert
//   3. Automatically downloads original source from GitHub
//   4. Recompiles and installs the clean binary
//   5. Restarts the daemon
//
// This is the ULTIMATE defense against binary modification attacks.

#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>

namespace ninacatcoin_integrity {

/**
 * @class FullIntegrityVerifier
 * @brief Verifies ALL source files of the daemon against compile-time hash
 *
 * SECURITY MODEL:
 * - At compile time, CMake hashes ALL source files → embeds in binary
 * - At runtime, recalculates hash of source files on disk
 * - If mismatch → source was tampered → auto-download from GitHub + rebuild
 * - No P2P consensus required for local source tampering detection
 * - This protects the ENTIRE binary, not just the AI subsystem
 */
class FullIntegrityVerifier {
public:
    static FullIntegrityVerifier& getInstance();

    /// Initialize the verifier (logs compiled hash info)
    bool initialize();

    /**
     * Verify full source integrity
     * @return true if source files match compiled hash (or not available)
     *         false if tampering detected
     */
    bool verifyFullSourceIntegrity();

    /**
     * Get the compile-time full source hash
     */
    static const char* getCompiledFullHash();

    /**
     * Get the compile-time file count  
     */
    static int getCompiledFileCount();

    /**
     * Calculate hash of all source files currently on disk
     * Uses same file discovery + sort as CMake for deterministic results
     * @return SHA-256 hex string, or empty if source files not found
     */
    std::string calculateLocalFullHash();

    /**
     * Trigger auto-remediation: download from GitHub, rebuild, restart
     * Called automatically when tampering is detected.
     * @return true if remediation was initiated
     */
    bool triggerAutoRemediation();

    /**
     * Set callback for tampering notification (e.g., Discord alert)
     */
    void setTamperingCallback(std::function<void(const std::string&, const std::string&)> cb);

    /// Get last verification status message
    std::string getLastStatus() const;

    /// Check if remediation is in progress
    bool isRemediating() const { return m_remediating.load(); }

private:
    FullIntegrityVerifier();
    ~FullIntegrityVerifier() = default;
    FullIntegrityVerifier(const FullIntegrityVerifier&) = delete;
    FullIntegrityVerifier& operator=(const FullIntegrityVerifier&) = delete;

    /**
     * Discover all source files in the source tree
     * Mirrors the CMake GLOB_RECURSE pattern for deterministic matching
     * @param base_path Root of the source tree (e.g., /mnt/i/ninacatcoin)
     * @return Sorted list of absolute file paths
     */
    std::vector<std::string> discoverSourceFiles(const std::string& base_path);

    /**
     * Calculate SHA-256 hash of concatenated file contents
     * Strips \r for CRLF compatibility (matches CMake's file(READ) behavior)
     */
    std::string calculateCombinedHash(const std::vector<std::string>& files);

    /**
     * Find the project source root directory
     * Tries multiple paths (WSL dev, installed, etc.)
     */
    std::string findSourceRoot();

    bool m_initialized = false;
    std::string m_last_status;
    std::string m_last_local_hash;
    mutable std::mutex m_status_mutex;
    std::atomic<bool> m_remediating{false};
    std::function<void(const std::string&, const std::string&)> m_tampering_callback;

    void setStatus(const std::string& status);
};

} // namespace ninacatcoin_integrity
