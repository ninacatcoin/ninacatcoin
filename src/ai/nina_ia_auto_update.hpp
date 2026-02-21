// Copyright (c) 2026, The ninacatcoin Project
//
// NINA IA Auto-Update Orchestrator
// Intelligent system that monitors GitHub for new versions and upcoming forks,
// autonomously downloads, compiles, installs, and restarts the daemon.
// NINA IA acts as the decision-maker: she analyzes changes, detects hard forks,
// schedules updates at optimal times, and reports her actions.

#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <cstdint>
#include <map>

namespace ninacatcoin_ai {

// ─────────────────────────────────────────────────────────────────────────────
// Detected change types in new versions
// ─────────────────────────────────────────────────────────────────────────────
enum class ChangeType {
    HARD_FORK,          // New hard fork height or version detected
    SOFT_FORK,          // Consensus parameter change (ring size, fees, etc.)
    SECURITY_FIX,       // Security-related change (critical)
    PERFORMANCE,        // Performance improvement
    FEATURE,            // New feature
    CONFIG_CHANGE,      // Configuration parameter change
    UNKNOWN             // Could not classify
};

struct DetectedChange {
    ChangeType type;
    std::string description;
    std::string file;
    int severity;       // 1-5 (1=low, 5=critical)
    uint64_t fork_height;  // If HARD_FORK, the activation height (0 otherwise)
};

// ─────────────────────────────────────────────────────────────────────────────
// Update decision result
// ─────────────────────────────────────────────────────────────────────────────
enum class UpdateDecision {
    UPDATE_NOW,             // Critical: update immediately
    UPDATE_SCHEDULED,       // Schedule for low-activity window
    UPDATE_BEFORE_FORK,     // Must update before fork height
    UPDATE_OPTIONAL,        // Non-critical, user can decide
    UPDATE_SKIP,            // Skip this version (pre-release, etc.)
    UPDATE_DEFERRED         // Node is busy (syncing), defer
};

struct UpdatePlan {
    UpdateDecision decision;
    std::string reason;
    std::string remote_version;
    std::string remote_tag;
    std::vector<DetectedChange> changes;
    uint64_t scheduled_time;        // epoch seconds (0 = now)
    uint64_t fork_deadline_height;  // If fork detected, must update before this
    int urgency;                    // 1-10 (10 = critical)
};

// ─────────────────────────────────────────────────────────────────────────────
// Update history entry
// ─────────────────────────────────────────────────────────────────────────────
struct UpdateRecord {
    uint64_t timestamp;
    std::string from_version;
    std::string to_version;
    bool success;
    std::string error_msg;
    std::vector<DetectedChange> changes;
};

// ─────────────────────────────────────────────────────────────────────────────
// NINA IA Auto-Update Orchestrator
// ─────────────────────────────────────────────────────────────────────────────
class NinaIAAutoUpdate {
public:
    static NinaIAAutoUpdate& getInstance();

    /// Initialize NINA IA with the current version and blockchain height
    /// @param local_version Current daemon version (e.g., "0.1.0.0")
    /// @param current_height Current blockchain height
    /// @param is_syncing Whether the node is currently syncing
    void initialize(const std::string& local_version,
                    uint64_t current_height,
                    bool is_syncing);

    /// Start the autonomous monitoring loop
    void start();

    /// Stop the monitoring loop
    void stop();

    /// Update the current blockchain state (called by daemon periodically)
    void updateBlockchainState(uint64_t current_height, bool is_syncing);

    /// Force an immediate check (for RPC or manual trigger)
    /// @return UpdatePlan with NINA IA's decision
    UpdatePlan forceCheck();

    /// Get NINA IA's current status report
    std::string getStatusReport() const;

    /// Get the update history
    std::vector<UpdateRecord> getUpdateHistory() const;

    /// Set a callback for Discord/log notifications
    using NotifyCallback = std::function<void(const std::string& title,
                                              const std::string& message,
                                              int severity)>;
    void setNotifyCallback(NotifyCallback cb);

    // Configuration
    static constexpr int CHECK_INTERVAL_SECS      = 21600;   // 6 hours (more frequent than basic checker)
    static constexpr int URGENT_CHECK_INTERVAL     = 1800;    // 30 min when fork approaching
    static constexpr int STARTUP_DELAY_SECS        = 180;     // 3 min after daemon start
    static constexpr int MAX_SYNC_WAIT_SECS        = 3600;    // Max 1 hour wait for sync
    static constexpr int PRE_FORK_BLOCKS_MARGIN    = 500;     // Update at least 500 blocks before fork
    static constexpr const char* GITHUB_API_COMPARE =
        "https://api.github.com/repos/ninacatcoin/ninacatcoin/compare/";
    static constexpr const char* GITHUB_RAW_CONFIG =
        "https://raw.githubusercontent.com/ninacatcoin/ninacatcoin/master/"
        "src/cryptonote_config.h";
    static constexpr const char* GITHUB_RAW_HF =
        "https://raw.githubusercontent.com/ninacatcoin/ninacatcoin/master/"
        "src/hardforks/hardforks.cpp";

private:
    NinaIAAutoUpdate();
    ~NinaIAAutoUpdate();
    NinaIAAutoUpdate(const NinaIAAutoUpdate&) = delete;
    NinaIAAutoUpdate& operator=(const NinaIAAutoUpdate&) = delete;

    // ── Core Intelligence ──

    /// Fetch and analyze the remote cryptonote_config.h for changes
    std::vector<DetectedChange> analyzeRemoteConfig();

    /// Fetch and analyze remote hardforks.cpp for new fork heights
    std::vector<DetectedChange> analyzeRemoteHardforks();

    /// Compare a raw remote file against local file content
    std::vector<DetectedChange> diffConfigFiles(const std::string& local_content,
                                                 const std::string& remote_content);

    /// Classify a config diff line into a ChangeType
    DetectedChange classifyChange(const std::string& define_name,
                                  const std::string& old_value,
                                  const std::string& new_value);

    /// Make an intelligent update decision based on detected changes
    UpdatePlan makeDecision(const std::string& remote_version,
                            const std::string& remote_tag,
                            const std::vector<DetectedChange>& changes);

    /// Execute the update (delegates to AutoUpdater)
    bool executeUpdate(const UpdatePlan& plan);

    // ── Utilities ──

    /// HTTP GET via curl
    std::string httpGet(const std::string& url, int timeout_secs = 30);

    /// Read a local file into a string
    std::string readLocalFile(const std::string& path);

    /// Extract #define values from config content: name → value
    std::map<std::string, std::string> parseDefines(const std::string& content);

    /// Parse hard fork table entries: returns vector of {version, height}
    std::vector<std::pair<int, uint64_t>> parseHardForkTable(const std::string& content);

    /// Get the path to local cryptonote_config.h (relative to binary)
    std::string getLocalConfigPath() const;

    /// Get the path to local hardforks.cpp
    std::string getLocalHardforksPath() const;

    /// Calculate blocks remaining until a fork height
    int64_t blocksUntilFork(uint64_t fork_height) const;

    /// Calculate estimated time until a fork height (in seconds)
    int64_t secondsUntilFork(uint64_t fork_height) const;

    /// Log a NINA IA message
    void ninaLog(const std::string& message, int severity = 1);

    /// Save update record to history
    void recordUpdate(const UpdateRecord& record);

    /// The autonomous monitoring loop
    void monitorLoop();

    // ── State ──
    std::string m_local_version;
    std::atomic<uint64_t> m_current_height{0};
    std::atomic<bool> m_is_syncing{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_urgent_mode{false};     // When fork is approaching
    std::atomic<uint64_t> m_next_fork_height{0}; // Detected upcoming fork

    mutable std::mutex m_status_mutex;
    std::string m_status;
    
    mutable std::mutex m_history_mutex;
    std::vector<UpdateRecord> m_update_history;

    NotifyCallback m_notify_callback;
    std::thread m_monitor_thread;

    // Cached analysis
    mutable std::mutex m_cache_mutex;
    uint64_t m_last_analysis_time{0};
    std::vector<DetectedChange> m_cached_changes;
    std::string m_cached_remote_version;
};

} // namespace ninacatcoin_ai
