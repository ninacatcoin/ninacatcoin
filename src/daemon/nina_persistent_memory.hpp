// NINA Persistent Memory Layer
// Uses the blockchain's LMDB for crash-safe, ACID-compliant state persistence
//
// ON-CHAIN ARCHITECTURE (v18 active from genesis):
//   All NINA data writes to the blockchain's data.mdb via
//   BlockchainDB::nina_*_put/get methods (5 on-chain tables).
//   No separate database — everything goes through the blockchain.

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ctime>
#include <sstream>
#include <cstring>
#include <mutex>

// Full include needed — methods like nina_state_put/get are called in this header
#include "blockchain_db/blockchain_db.h"

// v18 hard fork — NINA on-chain persistence active from height 3
#define NINA_V18_ONCHAIN_HEIGHT 3

namespace daemonize {

// Estructura para persistir históricos de bloques
struct PersistedBlockRecord {
    uint64_t height;
    uint64_t timestamp;
    uint64_t solve_time;
    uint64_t difficulty;
    uint64_t previous_difficulty;
    double lwma_prediction_error;
    int anomaly_flags;
    
    std::string serialize() const {
        std::stringstream ss;
        ss << height << "|" << timestamp << "|" << solve_time << "|" 
           << difficulty << "|" << previous_difficulty << "|"
           << lwma_prediction_error << "|" << anomaly_flags;
        return ss.str();
    }
    
    static PersistedBlockRecord deserialize(const std::string& data) {
        PersistedBlockRecord rec{};
        std::istringstream ss(data);
        std::string token;
        int field = 0;
        
        while (std::getline(ss, token, '|')) {
            try {
                switch (field++) {
                    case 0: rec.height = std::stoull(token); break;
                    case 1: rec.timestamp = std::stoull(token); break;
                    case 2: rec.solve_time = std::stoull(token); break;
                    case 3: rec.difficulty = std::stoull(token); break;
                    case 4: rec.previous_difficulty = std::stoull(token); break;
                    case 5: rec.lwma_prediction_error = std::stod(token); break;
                    case 6: rec.anomaly_flags = std::stoi(token); break;
                }
            } catch (...) { }
        }
        return rec;
    }
};

// Estructura para estadísticas generales
struct PersistedStatistics {
    uint64_t total_blocks_processed;
    uint64_t total_anomalies_detected;
    uint64_t total_attacks_predicted;
    uint64_t session_count;
    double average_prediction_accuracy;
    double peer_reputation_average;
    double network_health_average;
    uint64_t last_persistence_time;
    
    std::string serialize() const {
        std::stringstream ss;
        ss << total_blocks_processed << "|" << total_anomalies_detected << "|"
           << total_attacks_predicted << "|" << session_count << "|"
           << average_prediction_accuracy << "|" << peer_reputation_average << "|"
           << network_health_average << "|" << last_persistence_time;
        return ss.str();
    }
    
    static PersistedStatistics deserialize(const std::string& data) {
        PersistedStatistics stats{};
        std::istringstream ss(data);
        std::string token;
        int field = 0;
        
        while (std::getline(ss, token, '|')) {
            try {
                switch (field++) {
                    case 0: stats.total_blocks_processed = std::stoull(token); break;
                    case 1: stats.total_anomalies_detected = std::stoull(token); break;
                    case 2: stats.total_attacks_predicted = std::stoull(token); break;
                    case 3: stats.session_count = std::stoull(token); break;
                    case 4: stats.average_prediction_accuracy = std::stod(token); break;
                    case 5: stats.peer_reputation_average = std::stod(token); break;
                    case 6: stats.network_health_average = std::stod(token); break;
                    case 7: stats.last_persistence_time = std::stoull(token); break;
                }
            } catch (...) { }
        }
        return stats;
    }
};

// =============================================================================
// On-chain Persistence Manager
// All NINA data lives in the blockchain's data.mdb via 5 on-chain tables:
//   nina_state, nina_blocks, nina_checkpoints, nina_decisions, nina_audit
// =============================================================================
class NINAPersistenceManager {
public:
    static NINAPersistenceManager& instance() {
        static NINAPersistenceManager inst;
        return inst;
    }

    ~NINAPersistenceManager() = default;

    // =========================================================================
    // Register the BlockchainDB pointer for on-chain persistence.
    // Called once after the daemon initializes the blockchain.
    // =========================================================================
    void set_blockchain_db(cryptonote::BlockchainDB* db) {
        m_blockchain_db = db;
        if (db) {
            MINFO("[NINA-LMDB] ✓ Blockchain DB registered for on-chain persistence");
        }
    }

    cryptonote::BlockchainDB* get_blockchain_db() const { return m_blockchain_db; }

    // Check if we should use the blockchain's LMDB for persistence
    bool use_onchain_db(uint64_t current_height) const {
        return m_blockchain_db != nullptr && current_height >= NINA_V18_ONCHAIN_HEIGHT;
    }

    // =========================================================================
    // Save current NINA state to blockchain's data.mdb
    // =========================================================================
    bool persist_nina_state(
        uint64_t current_height,
        const std::map<uint64_t, PersistedBlockRecord>& block_history,
        const PersistedStatistics& stats
    ) {
        if (!m_blockchain_db) return false;
        return persist_nina_state_onchain(current_height, block_history, stats);
    }
    
    // =========================================================================
    // Load previous state from blockchain's data.mdb
    // =========================================================================
    bool load_nina_state(
        uint64_t& out_last_height,
        std::map<uint64_t, PersistedBlockRecord>& out_block_history,
        PersistedStatistics& out_stats
    ) {
        if (!m_blockchain_db) return false;
        return load_nina_state_onchain(out_last_height, out_block_history, out_stats);
    }
    
    // =========================================================================
    // Append to audit log (on-chain, key = height)
    // =========================================================================
    void log_Nina_audit_trail(
        uint64_t height,
        const std::string& event_type,
        const std::string& details
    ) {
        // Rate-limit audit writes: only persist important events
        if (event_type != "FRAMEWORK_INIT" && event_type != "STATE_PERSISTED"
            && event_type != "SHUTDOWN" && event_type != "MEMORY_PERSIST") {
            return;
        }

        if (!m_blockchain_db) return;

        uint64_t routing_height = height;
        if (routing_height == 0) {
            try { routing_height = m_blockchain_db->height(); } catch (...) {}
        }
        try {
            std::stringstream val;
            val << height << "|" << event_type << "|" << details;
            m_blockchain_db->nina_audit_put(routing_height, val.str());
        } catch (...) { /* non-fatal */ }
    }

private:
    time_t last_persist_time = 0;
    uint64_t records_saved = 0;
    uint64_t records_loaded = 0;
    cryptonote::BlockchainDB* m_blockchain_db = nullptr;
    mutable std::mutex m_db_mutex;

    NINAPersistenceManager() {
        MINFO("[NINA-LMDB] Manager initialized (on-chain persistence)");
    }

    // =========================================================================
    // V18 ON-CHAIN PERSISTENCE — write to blockchain's data.mdb
    // =========================================================================
    bool persist_nina_state_onchain(
        uint64_t current_height,
        const std::map<uint64_t, PersistedBlockRecord>& block_history,
        const PersistedStatistics& stats)
    {
        try {
            // 1. Write statistics (single key, overwritten each time)
            m_blockchain_db->nina_state_put("current", stats.serialize());

            // 2. Write metadata (single key, overwritten)
            {
                std::stringstream meta;
                meta << current_height << "|" << static_cast<uint64_t>(std::time(nullptr));
                m_blockchain_db->nina_state_put("meta_current", meta.str());
            }

            // 3. Write only NEW block records (LMDB put = upsert)
            for (const auto& [h, rec] : block_history) {
                m_blockchain_db->nina_block_put(h, rec.serialize());
            }

            last_persist_time = std::time(nullptr);
            records_saved += block_history.size();
            return true;
        } catch (const std::exception& e) {
            MERROR("[NINA-WRITER] ERROR persisting at height " << current_height << ": " << e.what());
            return false;
        }
    }

    bool load_nina_state_onchain(
        uint64_t& out_last_height,
        std::map<uint64_t, PersistedBlockRecord>& out_block_history,
        PersistedStatistics& out_stats)
    {
        try {
            // 1. Read metadata
            std::string meta_str;
            if (!m_blockchain_db->nina_state_get("meta_current", meta_str)) {
                return false;  // No on-chain data yet
            }
            {
                std::istringstream ss(meta_str);
                std::string token;
                if (std::getline(ss, token, '|')) out_last_height = std::stoull(token);
            }

            // 2. Read statistics
            std::string stats_str;
            if (m_blockchain_db->nina_state_get("current", stats_str)) {
                out_stats = PersistedStatistics::deserialize(stats_str);
                MINFO("[NINA-ONCHAIN] ✓ Statistics loaded from blockchain (blocks="
                      << out_stats.total_blocks_processed
                      << ", sessions=" << out_stats.session_count << ")");
            }

            // 3. Read all block records via iterator
            m_blockchain_db->nina_block_for_all(
                [&out_block_history](uint64_t h, const std::string& data) -> bool {
                    out_block_history[h] = PersistedBlockRecord::deserialize(data);
                    return true;
                });

            records_loaded += out_block_history.size();
            MINFO("[NINA-ONCHAIN] ✓ Recovery complete from blockchain data.mdb"
                  << " (last height: " << out_last_height 
                  << ", " << out_block_history.size() << " block records)");
            return true;
        } catch (const std::exception& e) {
            MWARNING("[NINA-ONCHAIN] Recovery warning: " << e.what());
            return false;
        }
    }
};

// ============================================================================
// Global convenience functions
// ============================================================================

inline bool nina_load_persistent_state() {
    auto& mgr = NINAPersistenceManager::instance();
    
    uint64_t last_height = 0;
    std::map<uint64_t, PersistedBlockRecord> history;
    PersistedStatistics stats{};
    
    if (mgr.load_nina_state(last_height, history, stats)) {
        MINFO("[NINA-LMDB] ✓ Memory restored from blockchain LMDB");
        MINFO("[NINA-LMDB]   Last height: " << last_height);
        MINFO("[NINA-LMDB]   Total sessions: " << stats.session_count);
        return true;
    }
    
    return false;
}

// NINA-WRITER: Streamlined save — NO load+save cycle.
// Simply writes the new data — LMDB put = upsert.
inline void nina_save_persistent_state(
    uint64_t current_height,
    uint64_t anomalies = 0,
    uint64_t attacks = 0,
    double accuracy = 0.0,
    double peer_rep = 0.0,
    double health = 0.0,
    const std::map<uint64_t, PersistedBlockRecord>& new_block_records = {}
) {
    auto& mgr = NINAPersistenceManager::instance();

    PersistedStatistics stats{
        current_height,
        anomalies,
        attacks,
        0,  // session_count is tracked in-memory, not reloaded per block
        accuracy,
        peer_rep,
        health,
        static_cast<uint64_t>(std::time(nullptr))
    };

    // Write only new block records + current stats snapshot (no full reload)
    mgr.persist_nina_state(current_height, new_block_records, stats);
}

inline void nina_audit_log(
    uint64_t height,
    const std::string& event,
    const std::string& details = ""
) {
    NINAPersistenceManager::instance().log_Nina_audit_trail(height, event, details);
}

/**
 * Check if NINA should use on-chain DB for standalone persistence functions.
 * Returns true only when BlockchainDB is registered AND current chain height >= 3.
 */
inline bool nina_should_use_onchain() {
    auto* bdb = NINAPersistenceManager::instance().get_blockchain_db();
    if (!bdb) return false;
    try {
        return bdb->height() >= NINA_V18_ONCHAIN_HEIGHT;
    } catch (...) {
        return false;
    }
}

// ============ MODULE PERSISTENCE FUNCTIONS ============

inline bool persist_memory_system_data(
    const std::string& patterns_data,
    const std::string& peers_data,
    size_t num_patterns = 0,
    size_t num_peers = 0
) {
    try {
        if (!nina_should_use_onchain()) return true;  // no-op before blockchain ready
        auto* bdb = NINAPersistenceManager::instance().get_blockchain_db();
        try {
            bdb->nina_state_put("memory_attack_patterns", patterns_data);
            bdb->nina_state_put("memory_peer_behaviors", peers_data);
            return true;
        } catch (const std::exception& e) {
            MWARNING("[NINA-MEMORY] On-chain persist failed: " << e.what());
            return false;
        }
    } catch (...) {
        return false;
    }
}

inline bool load_memory_system_data(
    std::string& out_patterns,
    std::string& out_peers
) {
    try {
        if (!nina_should_use_onchain()) return false;
        auto* bdb = NINAPersistenceManager::instance().get_blockchain_db();
        try {
            bool got_patterns = bdb->nina_state_get("memory_attack_patterns", out_patterns);
            bool got_peers = bdb->nina_state_get("memory_peer_behaviors", out_peers);
            if (got_patterns || got_peers) {
                MINFO("[NINA-MEMORY] ✓ Memory data loaded from blockchain LMDB");
                return true;
            }
        } catch (...) {}
        return false;
    } catch (...) {
        return false;
    }
}

inline bool persist_learning_module_data(
    const void* metrics_ptr
) {
    try {
        if (!metrics_ptr) return false;

        const auto* metrics = static_cast<const std::map<std::string, std::string>*>(metrics_ptr);

        // Serialize all metrics into a single blob
        std::stringstream ss;
        for (const auto& pair : *metrics) {
            ss << pair.second << "\n";
        }
        std::string value = ss.str();

        if (!nina_should_use_onchain()) return true;  // no-op before blockchain ready
        auto* bdb = NINAPersistenceManager::instance().get_blockchain_db();
        try {
            bdb->nina_state_put("learning_metrics", value);
            return true;
        } catch (const std::exception& e) {
            MWARNING("[NINA-WRITER] Learning persist failed: " << e.what());
            return false;
        }
    } catch (const std::exception& e) {
        MERROR("[NINA-WRITER] ERROR persisting learning data: " << e.what());
        return false;
    }
}

inline bool load_learning_module_data(std::string& out_data) {
    try {
        if (!nina_should_use_onchain()) return false;
        auto* bdb = NINAPersistenceManager::instance().get_blockchain_db();
        try {
            std::string data;
            if (bdb->nina_state_get("learning_metrics", data) && !data.empty()) {
                out_data = std::move(data);
                MINFO("[NINA-LMDB] ✓ Learning metrics loaded from blockchain LMDB ("
                      << out_data.size() << " bytes)");
                return true;
            }
        } catch (...) {}
        return false;
    } catch (...) {
        return false;
    }
}

inline bool persist_suggestion_engine_data(
    const std::string& pending_data,
    const std::string& history_data,
    size_t num_pending = 0,
    size_t num_history = 0
) {
    try {
        if (!nina_should_use_onchain()) return true;  // no-op before blockchain ready
        auto* bdb = NINAPersistenceManager::instance().get_blockchain_db();
        try {
            bdb->nina_state_put("suggestion_pending", pending_data);
            bdb->nina_state_put("suggestion_history", history_data);
            return true;
        } catch (const std::exception& e) {
            MWARNING("[NINA-WRITER] Suggestion persist failed: " << e.what());
            return false;
        }
    } catch (...) {
        return false;
    }
}

inline bool load_suggestion_engine_data(
    std::string& out_pending,
    std::string& out_history
) {
    try {
        if (!nina_should_use_onchain()) return false;
        auto* bdb = NINAPersistenceManager::instance().get_blockchain_db();
        try {
            bool got_pending = bdb->nina_state_get("suggestion_pending", out_pending);
            bool got_history = bdb->nina_state_get("suggestion_history", out_history);
            if (got_pending || got_history) {
                MINFO("[NINA-SUGGEST] ✓ Suggestions loaded from blockchain LMDB");
                return true;
            }
        } catch (...) {}
        return false;
    } catch (...) {
        return false;
    }
}

// ============ NINA STATE EXPORT FOR P2P SHARING ============
// Exports the full NINA state as a serialized string for P2P sync
// Format: HEADER\nSTATS_LINE\nBLOCK_RECORD_1\nBLOCK_RECORD_2\n...
inline std::string export_nina_state_for_p2p() {
    try {
        auto& mgr = NINAPersistenceManager::instance();
        uint64_t last_height = 0;
        std::map<uint64_t, PersistedBlockRecord> history;
        PersistedStatistics stats{};

        if (!mgr.load_nina_state(last_height, history, stats))
            return "";

        std::stringstream ss;
        // Header line: version|last_height|timestamp
        ss << "NINA_STATE_V1|" << last_height << "|" << static_cast<uint64_t>(std::time(nullptr)) << "\n";
        // Stats line
        ss << "STATS|" << stats.serialize() << "\n";
        // Block records (limit to last 500 to keep message reasonable)
        size_t count = 0;
        for (auto it = history.rbegin(); it != history.rend() && count < 500; ++it, ++count) {
            ss << "BLOCK|" << it->second.serialize() << "\n";
        }
        return ss.str();
    } catch (...) {
        return "";
    }
}

// ============ INCREMENTAL NINA STATE EXPORT ============
// Only exports block records AFTER since_height — much smaller payload for periodic sync
inline std::string export_nina_state_incremental(uint64_t since_height) {
    try {
        auto& mgr = NINAPersistenceManager::instance();
        uint64_t last_height = 0;
        std::map<uint64_t, PersistedBlockRecord> history;
        PersistedStatistics stats{};

        if (!mgr.load_nina_state(last_height, history, stats))
            return "";

        std::stringstream ss;
        ss << "NINA_STATE_V1|" << last_height << "|" << static_cast<uint64_t>(std::time(nullptr)) << "\n";
        ss << "STATS|" << stats.serialize() << "\n";

        size_t count = 0;
        for (auto it = history.upper_bound(since_height); it != history.end() && count < 200; ++it, ++count) {
            ss << "BLOCK|" << it->second.serialize() << "\n";
        }
        return ss.str();
    } catch (...) {
        return "";
    }
}

// ============ NINA STATE SUMMARY (lightweight) ============
// Returns just height + record count — no heavy serialization
struct NinaStateSummary {
    uint64_t last_height = 0;
    uint64_t block_records = 0;
    uint64_t session_count = 0;
};

inline NinaStateSummary get_nina_state_summary() {
    NinaStateSummary summary;
    try {
        auto& mgr = NINAPersistenceManager::instance();
        std::map<uint64_t, PersistedBlockRecord> history;
        PersistedStatistics stats{};

        if (mgr.load_nina_state(summary.last_height, history, stats)) {
            summary.block_records = history.size();
            summary.session_count = stats.session_count;
        }
    } catch (...) {}
    return summary;
}

// Import NINA state received from a peer via P2P
// Returns true if the imported state was useful (newer/more data)
inline bool import_nina_state_from_p2p(const std::string& data, uint64_t peer_height) {
    try {
        if (data.empty() || data.substr(0, 14) != "NINA_STATE_V1|")
            return false;

        auto& mgr = NINAPersistenceManager::instance();

        // Load our current state to compare
        uint64_t our_height = 0;
        std::map<uint64_t, PersistedBlockRecord> our_history;
        PersistedStatistics our_stats{};
        mgr.load_nina_state(our_height, our_history, our_stats);

        // Parse peer state
        std::istringstream stream(data);
        std::string line;
        uint64_t peer_last_height = 0;
        PersistedStatistics peer_stats{};
        std::map<uint64_t, PersistedBlockRecord> peer_blocks;

        while (std::getline(stream, line)) {
            if (line.empty()) continue;

            if (line.substr(0, 14) == "NINA_STATE_V1|") {
                // Parse header
                auto sep = line.find('|', 14);
                if (sep != std::string::npos) {
                    try { peer_last_height = std::stoull(line.substr(14, sep - 14)); } catch (...) {}
                }
            } else if (line.substr(0, 6) == "STATS|") {
                peer_stats = PersistedStatistics::deserialize(line.substr(6));
            } else if (line.substr(0, 6) == "BLOCK|") {
                auto rec = PersistedBlockRecord::deserialize(line.substr(6));
                if (rec.height > 0) {
                    peer_blocks[rec.height] = rec;
                }
            }
        }

        // Only import if peer has more data than us
        if (peer_last_height <= our_height && peer_blocks.size() <= our_history.size())
            return false;

        // Merge: add block records we don't have
        size_t imported = 0;
        for (const auto& [h, rec] : peer_blocks) {
            if (our_history.find(h) == our_history.end()) {
                our_history[h] = rec;
                imported++;
            }
        }

        if (imported == 0)
            return false;

        // Update stats with best values
        PersistedStatistics merged_stats = our_stats;
        if (peer_stats.total_blocks_processed > our_stats.total_blocks_processed)
            merged_stats.total_blocks_processed = peer_stats.total_blocks_processed;
        if (peer_stats.total_anomalies_detected > our_stats.total_anomalies_detected)
            merged_stats.total_anomalies_detected = peer_stats.total_anomalies_detected;
        if (peer_stats.average_prediction_accuracy > our_stats.average_prediction_accuracy)
            merged_stats.average_prediction_accuracy = peer_stats.average_prediction_accuracy;
        merged_stats.last_persistence_time = static_cast<uint64_t>(std::time(nullptr));

        // Save merged state
        uint64_t new_height = std::max(our_height, peer_last_height);
        mgr.persist_nina_state(new_height, our_history, merged_stats);

        MINFO("[NINA-P2P-SYNC] ✓ Imported " << imported << " block records from peer"
              << " (our height: " << our_height << " -> " << new_height << ")");
        nina_audit_log(new_height, "P2P_STATE_IMPORT",
            "Imported " + std::to_string(imported) + " records from peer at height " + std::to_string(peer_height));
        return true;
    } catch (const std::exception& e) {
        MERROR("[NINA-P2P-SYNC] ERROR importing state: " << e.what());
        return false;
    }
}

inline bool persist_constitution_data(
    uint64_t height,
    bool consensus_compliant,
    bool censorship_free,
    bool transparent,
    bool respects_autonomy,
    bool maintains_immutability,
    double overall_score
) {
    try {
        std::stringstream details;
        details << "consensus=" << consensus_compliant
                << " censorship_free=" << censorship_free
                << " transparent=" << transparent
                << " autonomy=" << respects_autonomy
                << " immutability=" << maintains_immutability
                << " score=" << overall_score;
        nina_audit_log(height, "CONSTITUTION_EVAL", details.str());
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace daemonize
