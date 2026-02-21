// Copyright (c) 2026, The ninacatcoin Project
//
// Version Checker: queries GitHub API for new releases every 12 hours.
// When a newer version is found, triggers the existing AutoUpdater pipeline
// to download, compile, install, and restart the daemon automatically.

#pragma once

#include <string>
#include <atomic>
#include <mutex>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

namespace ninacatcoin_ai {

/**
 * @struct VersionInfo
 * @brief Parsed semantic version (major.minor.patch.build)
 */
struct VersionInfo {
    int major = 0;
    int minor = 0;
    int patch = 0;
    int build = 0;
    std::string tag_name;       // e.g. "v0.2.0.0"
    std::string commit_sha;     // SHA of the tagged commit
    std::string download_url;   // tarball_url or clone URL
    std::string release_notes;  // body text from GitHub release
    bool is_prerelease = false;

    /// Compare: returns true if 'other' is newer than 'this'
    bool isOlderThan(const VersionInfo& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        if (patch != other.patch) return patch < other.patch;
        return build < other.build;
    }

    /// Format as string "major.minor.patch.build"
    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "."
             + std::to_string(patch) + "." + std::to_string(build);
    }

    bool isValid() const { return major > 0 || minor > 0 || patch > 0 || build > 0; }
};

/**
 * @class VersionChecker
 * @brief Periodically checks GitHub for new releases and triggers auto-update
 *
 * SECURITY MODEL:
 * - Only official GitHub releases (not arbitrary commits)
 * - Pre-releases are skipped by default
 * - Uses the existing AutoUpdater pipeline for safe update
 * - Logs all version check attempts and results
 * - Discord notification on new version found
 *
 * FLOW:
 *   1. Every 12 hours (configurable): query GitHub API for latest release
 *   2. Parse remote version tag (e.g., "v0.2.0.0")
 *   3. Compare against local version (ninacatcoin_VERSION)
 *   4. If newer → log + Discord notify + AutoUpdater::performUpdate()
 *   5. AutoUpdater handles: git clone → cmake → make → backup → install → restart
 */
class VersionChecker {
public:
    static VersionChecker& getInstance();

    /// Initialize the version checker with the current local version
    /// @param local_version The current daemon version string (e.g., "0.1.0.0")
    void initialize(const std::string& local_version);

    /// Start the background checker thread (checks every CHECK_INTERVAL_SECS)
    void startPeriodicCheck();

    /// Stop the background checker thread
    void stop();

    /// Perform a single version check right now
    /// @return true if a new version was found (update initiated)
    bool checkForUpdate();

    /// Set callback for when a new version is detected
    /// @param cb Callback(local_version, remote_version, release_notes)
    using UpdateCallback = std::function<void(const std::string&, const std::string&, const std::string&)>;
    void setUpdateCallback(UpdateCallback cb);

    /// Get the current status
    std::string getStatus() const;

    /// Get last check time (epoch seconds)
    uint64_t getLastCheckTime() const { return m_last_check_time.load(); }

    /// Get the parsed local version
    const VersionInfo& getLocalVersion() const { return m_local_version; }

    /// Check if we're currently checking for updates
    bool isChecking() const { return m_checking.load(); }

    // Configuration
    static constexpr const char* GITHUB_API_RELEASES =
        "https://api.github.com/repos/ninacatcoin/ninacatcoin/releases/latest";
    static constexpr const char* GITHUB_API_TAGS =
        "https://api.github.com/repos/ninacatcoin/ninacatcoin/tags";
    static constexpr int CHECK_INTERVAL_SECS = 43200;  // 12 hours
    static constexpr int API_TIMEOUT_SECS    = 30;      // HTTP timeout
    static constexpr int RETRY_DELAY_SECS    = 300;     // 5 min retry on failure
    static constexpr int MAX_RETRIES         = 3;

private:
    VersionChecker();
    ~VersionChecker();
    VersionChecker(const VersionChecker&) = delete;
    VersionChecker& operator=(const VersionChecker&) = delete;

    /// Parse a version string "0.1.0.0" or "v0.1.0.0" into VersionInfo
    static VersionInfo parseVersion(const std::string& version_str);

    /// Query GitHub API and parse the response
    /// @return VersionInfo of the latest release (isValid() false on failure)
    VersionInfo fetchLatestRelease();

    /// Query GitHub tags API as fallback
    VersionInfo fetchLatestTag();

    /// Execute an HTTP GET request and return the response body
    /// Uses curl under the hood
    std::string httpGet(const std::string& url, int timeout_secs = API_TIMEOUT_SECS);

    /// Parse JSON response from GitHub releases API
    VersionInfo parseReleaseJson(const std::string& json_body);

    /// Parse JSON response from GitHub tags API
    VersionInfo parseTagJson(const std::string& json_body);

    /// The periodic check loop (runs in background thread)
    void periodicCheckLoop();

    /// Log and set status
    void setStatus(const std::string& status);

    // State
    VersionInfo m_local_version;
    std::atomic<uint64_t> m_last_check_time{0};
    std::atomic<bool> m_checking{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_status_mutex;
    std::string m_status;
    UpdateCallback m_update_callback;
    std::thread m_check_thread;
};

} // namespace ninacatcoin_ai
