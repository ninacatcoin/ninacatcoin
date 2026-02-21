// Copyright (c) 2026, The ninacatcoin Project
//
// Auto-updater: downloads verified source from GitHub, compiles, installs.
// Triggered when P2P consensus shows our AI code hash is in the minority.

#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace ninacatcoin_ai {

/**
 * @class AutoUpdater
 * @brief Downloads, verifies, compiles and installs ninacatcoin from GitHub
 *
 * SECURITY MODEL:
 * - Only triggered when >50% of peers have a DIFFERENT hash than ours
 * - Downloads from OFFICIAL GitHub repo only
 * - Verifies compiled hash matches the network consensus hash
 * - If verification fails, update is aborted
 * - Old binary is backed up before replacement
 *
 * FLOW:
 *   1. P2P consensus detects our hash is in the minority
 *   2. Detect source and build directories from the running binary path
 *   3. Download/update source via git in the ORIGINAL source directory
 *   4. Rebuild in the ORIGINAL build directory (cmake + make/MSBuild)
 *   5. Hash the new AI source files, verify they match consensus
 *   6. Install new binary (mv trick to handle "Text file busy" on Linux)
 *   7. Signal daemon for graceful restart
 */
class AutoUpdater {
public:
    static AutoUpdater& getInstance();

    /// Check if an update is needed based on P2P hash consensus
    /// @param our_hash       This node's compiled AI hash
    /// @param consensus_hash The hash that the majority of peers report
    /// @param agreement_pct  Percentage of peers that agree on consensus_hash
    /// @return true if update should proceed
    bool shouldUpdate(const std::string& our_hash,
                      const std::string& consensus_hash,
                      double agreement_pct);

    /// Execute the full update pipeline
    /// @param expected_hash The hash we expect the new source to produce
    /// @return true if update succeeded (daemon should restart)
    bool performUpdate(const std::string& expected_hash);

    /// Check if an update is currently in progress
    bool isUpdating() const { return m_updating.load(); }

    /// Get human-readable status message
    std::string getStatus() const;

    /// Get the path to the current daemon binary
    std::string getDaemonPath() const;

    /// Detect the source root directory from the running binary
    /// e.g. /mnt/i/ninacatcoin/build-linux/bin/ninacatcoind -> /mnt/i/ninacatcoin
    std::string getSourceDir();

    /// Detect the build directory from the running binary
    /// e.g. /mnt/i/ninacatcoin/build-linux/bin/ninacatcoind -> /mnt/i/ninacatcoin/build-linux
    std::string getBuildDir();

    /// Detect the build type name ("build-linux", "build", "build-win", etc.)
    std::string detectBuildType();

    // Configuration
    static constexpr const char* GITHUB_REPO     = "https://github.com/ninacatcoin/ninacatcoin.git";
    static constexpr const char* GITHUB_BRANCH   = "master";
    static constexpr double MIN_CONSENSUS_PCT    = 0.6;   // 60% of peers must agree
    static constexpr int    MIN_PEERS_FOR_UPDATE = 3;      // Need at least 3 peers
    static constexpr int    BUILD_TIMEOUT_SECS   = 1800;   // 30 min max build time

private:
    AutoUpdater();
    ~AutoUpdater() = default;
    AutoUpdater(const AutoUpdater&) = delete;
    AutoUpdater& operator=(const AutoUpdater&) = delete;

    /// Step 1: Clone or pull from GitHub
    bool downloadSource(const std::string& target_dir);

    /// Step 2: Build the project
    bool buildProject(const std::string& source_dir, const std::string& build_dir);

    /// Step 3: Calculate AI source hash from downloaded source
    std::string calculateSourceHash(const std::string& source_dir);

    /// Step 4: Backup current binary
    bool backupCurrentBinary(const std::string& backup_path);

    /// Step 5: Install new binary (handles "Text file busy" on Linux via mv trick)
    bool installNewBinary(const std::string& new_binary, const std::string& target_path);

    /// Step 6: Signal restart (SIGHUP on Linux, graceful exit on Windows)
    void signalRestart();

    /// Run a shell command and return exit code
    int runCommand(const std::string& cmd, int timeout_secs = 0);

    /// Get the update working directory
    std::string getUpdateDir() const;

    std::atomic<bool> m_updating{false};
    mutable std::mutex m_status_mutex;
    std::string m_status;

    void setStatus(const std::string& status);
};

} // namespace ninacatcoin_ai
