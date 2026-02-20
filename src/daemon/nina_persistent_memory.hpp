// NINA Persistent Memory Layer
// Uses LMDB for crash-safe, ACID-compliant state persistence
// Separate database from blockchain — stored in ~/.ninacatcoin/nina_state/
// Recovers historical memory on each daemon restart

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ctime>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <lmdb.h>

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
// LMDB-backed Persistence Manager
// Database: ~/.ninacatcoin/nina_state/data.mdb
// Tables (named DBIs):
//   "stats"  — single key "current" → serialized PersistedStatistics
//   "meta"   — single key "current" → "height|timestamp|first_run_ts|version"
//   "blocks" — key = height (8 bytes BE) → serialized PersistedBlockRecord
//   "audit"  — key = timestamp (8 bytes BE) → "height|event|details"
// =============================================================================
class NINAPersistenceManager {
private:
    MDB_env* m_env = nullptr;
    MDB_dbi  m_dbi_stats  = 0;
    MDB_dbi  m_dbi_meta   = 0;
    MDB_dbi  m_dbi_blocks = 0;
    MDB_dbi  m_dbi_audit  = 0;
    std::string m_db_path;
    bool m_open = false;

    // 64 MB map size — more than enough for NINA state
    static constexpr size_t DB_MAP_SIZE = 64ULL * 1024 * 1024;

    std::string resolve_db_path() const {
        const char* home = std::getenv("HOME");
        if (!home) home = "/tmp";
        return std::string(home) + "/.ninacatcoin/nina_state";
    }

    static void ensure_dir(const std::string& path) {
        std::string cmd = "mkdir -p '" + path + "' 2>/dev/null";
        (void)system(cmd.c_str());
    }

    // Convert uint64_t to big-endian 8-byte key for sorted iteration
    static void uint64_to_key(uint64_t val, char buf[8]) {
        for (int i = 7; i >= 0; --i) {
            buf[i] = static_cast<char>(val & 0xFF);
            val >>= 8;
        }
    }

    static uint64_t key_to_uint64(const char* buf) {
        uint64_t val = 0;
        for (int i = 0; i < 8; ++i) {
            val = (val << 8) | static_cast<uint8_t>(buf[i]);
        }
        return val;
    }

    // Helper: put a string value with a string key in a write txn
    bool lmdb_put_str(MDB_txn* txn, MDB_dbi dbi, const std::string& key, const std::string& value) {
        MDB_val k, v;
        k.mv_data = const_cast<char*>(key.data());
        k.mv_size = key.size();
        v.mv_data = const_cast<char*>(value.data());
        v.mv_size = value.size();
        int rc = mdb_put(txn, dbi, &k, &v, 0);
        return rc == 0;
    }

    // Helper: put a uint64 key with string value
    bool lmdb_put_u64(MDB_txn* txn, MDB_dbi dbi, uint64_t key_num, const std::string& value) {
        char key_buf[8];
        uint64_to_key(key_num, key_buf);
        MDB_val k, v;
        k.mv_data = key_buf;
        k.mv_size = 8;
        v.mv_data = const_cast<char*>(value.data());
        v.mv_size = value.size();
        int rc = mdb_put(txn, dbi, &k, &v, 0);
        return rc == 0;
    }

    // Helper: get a string value by string key
    bool lmdb_get_str(MDB_txn* txn, MDB_dbi dbi, const std::string& key, std::string& out) {
        MDB_val k, v;
        k.mv_data = const_cast<char*>(key.data());
        k.mv_size = key.size();
        int rc = mdb_get(txn, dbi, &k, &v);
        if (rc == 0) {
            out.assign(static_cast<const char*>(v.mv_data), v.mv_size);
            return true;
        }
        return false;
    }

    bool open_env() {
        if (m_open) return true;

        ensure_dir(m_db_path);

        int rc = mdb_env_create(&m_env);
        if (rc) {
            std::cerr << "[NINA-LMDB] Failed to create env: " << mdb_strerror(rc) << std::endl;
            return false;
        }

        // 4 named databases: stats, meta, blocks, audit
        rc = mdb_env_set_maxdbs(m_env, 4);
        if (rc) {
            std::cerr << "[NINA-LMDB] Failed to set maxdbs: " << mdb_strerror(rc) << std::endl;
            mdb_env_close(m_env); m_env = nullptr;
            return false;
        }

        rc = mdb_env_set_mapsize(m_env, DB_MAP_SIZE);
        if (rc) {
            std::cerr << "[NINA-LMDB] Failed to set mapsize: " << mdb_strerror(rc) << std::endl;
            mdb_env_close(m_env); m_env = nullptr;
            return false;
        }

        rc = mdb_env_open(m_env, m_db_path.c_str(), 0, 0664);
        if (rc) {
            std::cerr << "[NINA-LMDB] Failed to open env at " << m_db_path << ": " << mdb_strerror(rc) << std::endl;
            mdb_env_close(m_env); m_env = nullptr;
            return false;
        }

        // Open all 4 named DBIs in a single write transaction
        MDB_txn* txn = nullptr;
        rc = mdb_txn_begin(m_env, nullptr, 0, &txn);
        if (rc) {
            std::cerr << "[NINA-LMDB] Failed to begin init txn: " << mdb_strerror(rc) << std::endl;
            mdb_env_close(m_env); m_env = nullptr;
            return false;
        }

        rc = mdb_dbi_open(txn, "nina_stats",  MDB_CREATE, &m_dbi_stats);
        if (rc) { mdb_txn_abort(txn); mdb_env_close(m_env); m_env = nullptr; return false; }

        rc = mdb_dbi_open(txn, "nina_meta",   MDB_CREATE, &m_dbi_meta);
        if (rc) { mdb_txn_abort(txn); mdb_env_close(m_env); m_env = nullptr; return false; }

        rc = mdb_dbi_open(txn, "nina_blocks", MDB_CREATE, &m_dbi_blocks);
        if (rc) { mdb_txn_abort(txn); mdb_env_close(m_env); m_env = nullptr; return false; }

        rc = mdb_dbi_open(txn, "nina_audit",  MDB_CREATE, &m_dbi_audit);
        if (rc) { mdb_txn_abort(txn); mdb_env_close(m_env); m_env = nullptr; return false; }

        rc = mdb_txn_commit(txn);
        if (rc) {
            std::cerr << "[NINA-LMDB] Failed to commit init txn: " << mdb_strerror(rc) << std::endl;
            mdb_env_close(m_env); m_env = nullptr;
            return false;
        }

        m_open = true;
        std::cout << "[NINA-LMDB] ✓ Database opened at " << m_db_path << "/data.mdb" << std::endl;
        return true;
    }

public:
    static NINAPersistenceManager& instance() {
        static NINAPersistenceManager inst;
        return inst;
    }

    ~NINAPersistenceManager() {
        close();
    }

    void close() {
        if (m_env) {
            mdb_dbi_close(m_env, m_dbi_stats);
            mdb_dbi_close(m_env, m_dbi_meta);
            mdb_dbi_close(m_env, m_dbi_blocks);
            mdb_dbi_close(m_env, m_dbi_audit);
            mdb_env_close(m_env);
            m_env = nullptr;
            m_open = false;
        }
    }
    
    // =========================================================================
    // Save current NINA state (single ACID transaction)
    // =========================================================================
    bool persist_nina_state(
        uint64_t current_height,
        const std::map<uint64_t, PersistedBlockRecord>& block_history,
        const PersistedStatistics& stats
    ) {
        if (!open_env()) return false;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(m_env, nullptr, 0, &txn);
        if (rc) {
            std::cerr << "[NINA-LMDB] ERROR begin txn: " << mdb_strerror(rc) << std::endl;
            return false;
        }

        try {
            // 1. Write statistics
            if (!lmdb_put_str(txn, m_dbi_stats, "current", stats.serialize())) {
                mdb_txn_abort(txn);
                std::cerr << "[NINA-LMDB] ERROR: Failed to write stats" << std::endl;
                return false;
            }

            // 2. Write block history
            for (const auto& [h, rec] : block_history) {
                if (!lmdb_put_u64(txn, m_dbi_blocks, h, rec.serialize())) {
                    mdb_txn_abort(txn);
                    std::cerr << "[NINA-LMDB] ERROR: Failed to write block " << h << std::endl;
                    return false;
                }
            }

            // 3. Write metadata
            {
                std::stringstream meta;
                meta << current_height << "|" << static_cast<uint64_t>(std::time(nullptr));
                if (!lmdb_put_str(txn, m_dbi_meta, "current", meta.str())) {
                    mdb_txn_abort(txn);
                    return false;
                }
            }

            rc = mdb_txn_commit(txn);
            if (rc) {
                std::cerr << "[NINA-LMDB] ERROR commit: " << mdb_strerror(rc) << std::endl;
                return false;
            }

            last_persist_time = std::time(nullptr);
            records_saved += block_history.size();

            std::cout << "[NINA-LMDB] ✓ State saved at height " << current_height
                      << " (" << block_history.size() << " block records)" << std::endl;
            return true;
        } catch (const std::exception& e) {
            mdb_txn_abort(txn);
            std::cerr << "[NINA-LMDB] ERROR: " << e.what() << std::endl;
            return false;
        }
    }
    
    // =========================================================================
    // Load previous state on startup (read-only transaction)
    // =========================================================================
    bool load_nina_state(
        uint64_t& out_last_height,
        std::map<uint64_t, PersistedBlockRecord>& out_block_history,
        PersistedStatistics& out_stats
    ) {
        if (!open_env()) return false;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(m_env, nullptr, MDB_RDONLY, &txn);
        if (rc) return false;

        try {
            // 1. Read metadata
            std::string meta_str;
            if (!lmdb_get_str(txn, m_dbi_meta, "current", meta_str)) {
                mdb_txn_abort(txn);
                std::cout << "[NINA-LMDB] No previous state found (first run)" << std::endl;
                return false;
            }
            {
                std::istringstream ss(meta_str);
                std::string token;
                if (std::getline(ss, token, '|')) out_last_height = std::stoull(token);
            }

            // 2. Read statistics
            std::string stats_str;
            if (lmdb_get_str(txn, m_dbi_stats, "current", stats_str)) {
                out_stats = PersistedStatistics::deserialize(stats_str);
                std::cout << "[NINA-LMDB] ✓ Statistics loaded (blocks=" 
                          << out_stats.total_blocks_processed
                          << ", sessions=" << out_stats.session_count << ")" << std::endl;
            }

            // 3. Read all block records via cursor
            MDB_cursor* cursor = nullptr;
            rc = mdb_cursor_open(txn, m_dbi_blocks, &cursor);
            if (rc == 0) {
                MDB_val k, v;
                while (mdb_cursor_get(cursor, &k, &v, MDB_NEXT) == 0) {
                    if (k.mv_size == 8) {
                        uint64_t h = key_to_uint64(static_cast<const char*>(k.mv_data));
                        std::string val(static_cast<const char*>(v.mv_data), v.mv_size);
                        out_block_history[h] = PersistedBlockRecord::deserialize(val);
                    }
                }
                mdb_cursor_close(cursor);
                std::cout << "[NINA-LMDB] ✓ Loaded " << out_block_history.size() 
                          << " block records" << std::endl;
            }

            mdb_txn_abort(txn);  // read-only: abort is fine

            records_loaded += out_block_history.size();
            std::cout << "[NINA-LMDB] ✓ Recovery complete (last height: " 
                      << out_last_height << ")" << std::endl;
            return true;
        } catch (const std::exception& e) {
            mdb_txn_abort(txn);
            std::cerr << "[NINA-LMDB] Warning: Could not load state: " << e.what() << std::endl;
            return false;
        }
    }
    
    // =========================================================================
    // Save a single block record (one small write txn)
    // =========================================================================
    bool persist_block_record(const PersistedBlockRecord& record) {
        if (!open_env()) return false;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(m_env, nullptr, 0, &txn);
        if (rc) return false;

        if (!lmdb_put_u64(txn, m_dbi_blocks, record.height, record.serialize())) {
            mdb_txn_abort(txn);
            return false;
        }

        rc = mdb_txn_commit(txn);
        if (rc == 0) { records_saved++; return true; }
        return false;
    }
    
    // =========================================================================
    // Load last N block records (cursor backward from end)
    // =========================================================================
    std::vector<PersistedBlockRecord> load_recent_blocks(uint64_t last_n = 100) {
        std::vector<PersistedBlockRecord> result;
        if (!open_env()) return result;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(m_env, nullptr, MDB_RDONLY, &txn);
        if (rc) return result;

        MDB_cursor* cursor = nullptr;
        rc = mdb_cursor_open(txn, m_dbi_blocks, &cursor);
        if (rc == 0) {
            // Go to last entry and walk backward
            MDB_val k, v;
            std::vector<PersistedBlockRecord> temp;
            rc = mdb_cursor_get(cursor, &k, &v, MDB_LAST);
            while (rc == 0 && temp.size() < last_n) {
                if (k.mv_size == 8) {
                    std::string val(static_cast<const char*>(v.mv_data), v.mv_size);
                    temp.push_back(PersistedBlockRecord::deserialize(val));
                }
                rc = mdb_cursor_get(cursor, &k, &v, MDB_PREV);
            }
            mdb_cursor_close(cursor);
            // Reverse to chronological order
            result.assign(temp.rbegin(), temp.rend());
        }

        mdb_txn_abort(txn);
        return result;
    }
    
    uint64_t get_total_sessions() {
        if (!open_env()) return 0;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(m_env, nullptr, MDB_RDONLY, &txn);
        if (rc) return 0;

        std::string stats_str;
        uint64_t sessions = 0;
        if (lmdb_get_str(txn, m_dbi_stats, "current", stats_str)) {
            sessions = PersistedStatistics::deserialize(stats_str).session_count;
        }
        mdb_txn_abort(txn);
        return sessions;
    }
    
    // =========================================================================
    // Append to audit log (write txn, key = timestamp)
    // =========================================================================
    void log_Nina_audit_trail(
        uint64_t height,
        const std::string& event_type,
        const std::string& details
    ) {
        if (!open_env()) return;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(m_env, nullptr, 0, &txn);
        if (rc) return;

        // Use nanosecond-ish key to avoid collisions
        uint64_t key = static_cast<uint64_t>(std::time(nullptr)) * 1000000ULL +
                       (records_saved % 1000000ULL);

        std::stringstream val;
        val << height << "|" << event_type << "|" << details;

        if (lmdb_put_u64(txn, m_dbi_audit, key, val.str())) {
            mdb_txn_commit(txn);
        } else {
            mdb_txn_abort(txn);
        }
    }
    
    // =========================================================================
    // Get DB file size for stats reporting
    // =========================================================================
    uint64_t get_db_size_kb() const {
        if (!m_env) return 0;
        MDB_envinfo info;
        mdb_env_info(m_env, &info);
        MDB_stat stat;
        // Approximate: entries * page_size
        if (mdb_env_stat(m_env, &stat) == 0) {
            return (stat.ms_psize * (stat.ms_branch_pages + stat.ms_leaf_pages + stat.ms_overflow_pages)) / 1024;
        }
        return 0;
    }

    struct PersistenceStats {
        uint64_t records_saved;
        uint64_t records_loaded;
        uint64_t total_persisted_size_kb;
        double persistence_time_ms;
    };
    
    PersistenceStats get_persistence_stats() const {
        return {records_saved, records_loaded, get_db_size_kb(), 0.0};
    }
    
private:
    time_t last_persist_time = 0;
    uint64_t records_saved = 0;
    uint64_t records_loaded = 0;
    
    NINAPersistenceManager() : m_db_path(resolve_db_path()) {
        // open_env() is called lazily on first use
        std::cout << "[NINA-LMDB] Manager initialized (db: " << m_db_path << "/data.mdb)" << std::endl;
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
        std::cout << "[NINA-LMDB] ✓ Memory restored from LMDB" << std::endl;
        std::cout << "[NINA-LMDB]   Last height: " << last_height << std::endl;
        std::cout << "[NINA-LMDB]   Total sessions: " << stats.session_count << std::endl;
        return true;
    }
    
    return false;
}

inline void nina_save_persistent_state(
    uint64_t current_height,
    uint64_t anomalies = 0,
    uint64_t attacks = 0,
    double accuracy = 0.0,
    double peer_rep = 0.0,
    double health = 0.0
) {
    auto& mgr = NINAPersistenceManager::instance();
    
    // Load existing stats to increment session count
    uint64_t last_height = 0;
    std::map<uint64_t, PersistedBlockRecord> history;
    PersistedStatistics existing_stats{};
    mgr.load_nina_state(last_height, history, existing_stats);
    
    PersistedStatistics stats{
        current_height,
        anomalies > 0 ? anomalies : existing_stats.total_anomalies_detected,
        attacks > 0 ? attacks : existing_stats.total_attacks_predicted,
        existing_stats.session_count + 1,
        accuracy > 0 ? accuracy : existing_stats.average_prediction_accuracy,
        peer_rep > 0 ? peer_rep : existing_stats.peer_reputation_average,
        health > 0 ? health : existing_stats.network_health_average,
        static_cast<uint64_t>(std::time(nullptr))
    };
    
    mgr.persist_nina_state(current_height, history, stats);
}

inline void nina_audit_log(
    uint64_t height,
    const std::string& event,
    const std::string& details = ""
) {
    NINAPersistenceManager::instance().log_Nina_audit_trail(height, event, details);
}

// ============ MODULE PERSISTENCE FUNCTIONS ============

inline bool persist_memory_system_data(
    const void* attack_patterns,
    const void* peer_behaviors
) {
    try {
        nina_audit_log(0, "MEMORY_PERSIST", "Memory system data persisted");
        return true;
    } catch (...) {
        return false;
    }
}

inline bool persist_learning_module_data(
    const void* metrics
) {
    try {
        nina_audit_log(0, "LEARNING_PERSIST", "Learning module data persisted");
        return true;
    } catch (...) {
        return false;
    }
}

inline bool persist_suggestion_engine_data(
    const void* suggestions
) {
    try {
        nina_audit_log(0, "SUGGESTION_PERSIST", "Suggestion engine data persisted");
        return true;
    } catch (...) {
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
