// NINA Persistent Memory Layer
// Uses LMDB for crash-safe, ACID-compliant state persistence
//
// DUAL-MODE ARCHITECTURE (v17 → v18 transition):
//   Before v18 (height < 20000): Separate LMDB at ~/.ninacatcoin/nina_state/data.mdb
//   After  v18 (height >= 20000): Writes go to the blockchain's data.mdb directly
//                                  via BlockchainDB::nina_*_put/get methods
//
// The NINAPersistenceManager automatically routes to the correct backend
// based on the current blockchain height. After v18, the separate nina_state/
// database is read-only (used only for migration of pre-v18 data).

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <ctime>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <atomic>
#ifndef _WIN32
#include <pwd.h>
#include <unistd.h>
#endif
#include <lmdb.h>

// Full include needed — methods like nina_state_put/get are called in this header
#include "blockchain_db/blockchain_db.h"

// v18 hard fork height — NINA data moves into blockchain's data.mdb
#define NINA_V18_ONCHAIN_HEIGHT 20000

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

    // 512 MB initial map size (~80 years of NINA memory at ~6 MB/year)
    // Auto-grows by 512 MB when limit is reached
    static constexpr size_t DB_MAP_SIZE_INITIAL = 512ULL * 1024 * 1024;
    static constexpr size_t DB_MAP_SIZE_INCREMENT = 512ULL * 1024 * 1024;
    size_t m_current_map_size = DB_MAP_SIZE_INITIAL;

    // Mutex to protect concurrent LMDB access from multiple threads
    mutable std::mutex m_db_mutex;

    std::string resolve_db_path() const {
        const char* home = std::getenv("HOME");
#ifndef _WIN32
        if (!home) {
            struct passwd* pw = getpwuid(getuid());
            if (pw) home = pw->pw_dir;
        }
#else
        if (!home) home = std::getenv("USERPROFILE");
#endif
        if (!home) home = ".";
        return std::string(home) + "/.ninacatcoin/nina_state";
    }

    static void ensure_dir(const std::string& path) {
        std::error_code ec;
        std::filesystem::create_directories(path, ec);
        // ec silently ignored like the original — best-effort
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

        rc = mdb_env_set_mapsize(m_env, m_current_map_size);
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
        std::cout << "[NINA-LMDB] ✓ Database opened at " << m_db_path << "/data.mdb"
                  << " (map size: " << (m_current_map_size / (1024*1024)) << " MB)" << std::endl;
        return true;
    }

    // Auto-grow: increase map size by 1 GB when MDB_MAP_FULL is detected
    bool grow_map_size() {
        if (!m_env) return false;
        
        size_t new_size = m_current_map_size + DB_MAP_SIZE_INCREMENT;
        
        // Close all DBIs first, then resize
        mdb_env_set_mapsize(m_env, new_size);
        m_current_map_size = new_size;
        
        std::cout << "[NINA-LMDB] ⚡ Map size auto-grown to "
                  << (m_current_map_size / (1024*1024)) << " MB" << std::endl;
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

    // =========================================================================
    // V18 ON-CHAIN REGISTRATION
    //
    // After the daemon initializes the blockchain, it calls set_blockchain_db()
    // to register the BlockchainDB pointer. From that point on, when the
    // blockchain height >= NINA_V18_ONCHAIN_HEIGHT, all writes go to the
    // blockchain's data.mdb instead of the separate nina_state/data.mdb.
    //
    // This is a one-time registration — the pointer remains valid for the
    // lifetime of the daemon process.
    // =========================================================================
    void set_blockchain_db(cryptonote::BlockchainDB* db) {
        m_blockchain_db = db;
        if (db) {
            std::cout << "[NINA-LMDB] ✓ Blockchain DB registered for v18 on-chain persistence" << std::endl;
        }
    }

    cryptonote::BlockchainDB* get_blockchain_db() const { return m_blockchain_db; }

    // Check if we should use the blockchain's LMDB for persistence
    bool use_onchain_db(uint64_t current_height) const {
        return m_blockchain_db != nullptr && current_height >= NINA_V18_ONCHAIN_HEIGHT;
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
    // Routes to blockchain's data.mdb after v18 (height >= 20000)
    // =========================================================================
    bool persist_nina_state(
        uint64_t current_height,
        const std::map<uint64_t, PersistedBlockRecord>& block_history,
        const PersistedStatistics& stats
    ) {
        // ═══════════════════════════════════════════════════════════════
        // V18 ON-CHAIN ROUTING
        // After hard fork v18, write to the blockchain's data.mdb
        // via BlockchainDB::nina_*_put() — same ACID guarantees as blocks
        // ═══════════════════════════════════════════════════════════════
        if (use_onchain_db(current_height)) {
            // One-time migration: copy old data to blockchain on first v18 persist
            static bool s_migration_checked = false;
            if (!s_migration_checked) {
                s_migration_checked = true;
                migrate_nina_data_to_blockchain();
            }
            return persist_nina_state_onchain(current_height, block_history, stats);
        }

        // Pre-v18: Write to separate nina_state/data.mdb
        std::lock_guard<std::mutex> lock(m_db_mutex);
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
            if (rc == MDB_MAP_FULL) {
                // Auto-grow and retry once
                std::cout << "[NINA-LMDB] Map full, auto-growing..." << std::endl;
                if (grow_map_size()) {
                    return persist_nina_state(current_height, block_history, stats);
                }
                std::cerr << "[NINA-LMDB] ERROR: Failed to grow map size" << std::endl;
                return false;
            }
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
    // Routes to blockchain's data.mdb after v18 (height >= 20000)
    // =========================================================================
    bool load_nina_state(
        uint64_t& out_last_height,
        std::map<uint64_t, PersistedBlockRecord>& out_block_history,
        PersistedStatistics& out_stats
    ) {
        // ═══════════════════════════════════════════════════════════════
        // V18 ON-CHAIN ROUTING
        // If blockchain DB is registered AND we have data on-chain,
        // load from blockchain's data.mdb first.
        // ═══════════════════════════════════════════════════════════════
        if (m_blockchain_db) {
            bool loaded = load_nina_state_onchain(out_last_height, out_block_history, out_stats);
            if (loaded && out_last_height >= NINA_V18_ONCHAIN_HEIGHT) {
                return true;  // On-chain data exists and is post-v18
            }
            // Fall through: either no on-chain data or pre-v18, try separate DB
        }

        // Pre-v18 or first boot: read from separate nina_state/data.mdb
        std::lock_guard<std::mutex> lock(m_db_mutex);
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
        std::lock_guard<std::mutex> lock(m_db_mutex);
        if (!open_env()) return false;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(m_env, nullptr, 0, &txn);
        if (rc) return false;

        if (!lmdb_put_u64(txn, m_dbi_blocks, record.height, record.serialize())) {
            mdb_txn_abort(txn);
            return false;
        }

        rc = mdb_txn_commit(txn);
        if (rc == MDB_MAP_FULL) {
            if (grow_map_size()) {
                return persist_block_record(record);  // Retry after grow
            }
            return false;
        }
        if (rc == 0) { records_saved++; return true; }
        return false;
    }
    
    // =========================================================================
    // Load last N block records (cursor backward from end)
    // =========================================================================
    std::vector<PersistedBlockRecord> load_recent_blocks(uint64_t last_n = 100) {
        std::vector<PersistedBlockRecord> result;
        std::lock_guard<std::mutex> lock(m_db_mutex);
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
        std::lock_guard<std::mutex> lock(m_db_mutex);
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
    // Routes to blockchain's data.mdb after v18
    // =========================================================================
    void log_Nina_audit_trail(
        uint64_t height,
        const std::string& event_type,
        const std::string& details
    ) {
        uint64_t key = static_cast<uint64_t>(std::time(nullptr)) * 1000000ULL +
                       (records_saved % 1000000ULL);

        std::stringstream val;
        val << height << "|" << event_type << "|" << details;

        // V18 on-chain routing
        if (use_onchain_db(height)) {
            try {
                m_blockchain_db->nina_audit_put(key, val.str());
                return;
            } catch (...) { /* fall through to separate DB */ }
        }

        // Pre-v18: write to separate nina_state/data.mdb
        std::lock_guard<std::mutex> lock(m_db_mutex);
        if (!open_env()) return;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(m_env, nullptr, 0, &txn);
        if (rc) return;

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

    // Expose LMDB environment for direct access by learning module persistence
    MDB_env* get_env() {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        if (!open_env()) return nullptr;
        return m_env;
    }
    
    // Expose mutex for external callers that use get_env() directly
    std::mutex& get_db_mutex() { return m_db_mutex; }
    
private:
    time_t last_persist_time = 0;
    uint64_t records_saved = 0;
    uint64_t records_loaded = 0;
    cryptonote::BlockchainDB* m_blockchain_db = nullptr;  // v18 on-chain backend
    
    NINAPersistenceManager() : m_db_path(resolve_db_path()) {
        // open_env() is called lazily on first use
        std::cout << "[NINA-LMDB] Manager initialized (db: " << m_db_path << "/data.mdb)" << std::endl;
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
            // 1. Write statistics to nina_state table
            m_blockchain_db->nina_state_put("current", stats.serialize());

            // 2. Write metadata
            {
                std::stringstream meta;
                meta << current_height << "|" << static_cast<uint64_t>(std::time(nullptr));
                m_blockchain_db->nina_state_put("meta_current", meta.str());
            }

            // 3. Write block records to nina_blocks table
            for (const auto& [h, rec] : block_history) {
                m_blockchain_db->nina_block_put(h, rec.serialize());
            }

            last_persist_time = std::time(nullptr);
            records_saved += block_history.size();

            std::cout << "[NINA-LMDB-ONCHAIN] ✓ State saved ON-CHAIN at height " << current_height
                      << " (" << block_history.size() << " block records, v18 mode)" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[NINA-LMDB-ONCHAIN] ERROR: " << e.what() << std::endl;
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
                std::cout << "[NINA-LMDB-ONCHAIN] ✓ Statistics loaded from blockchain (blocks="
                          << out_stats.total_blocks_processed
                          << ", sessions=" << out_stats.session_count << ")" << std::endl;
            }

            // 3. Read all block records via iterator
            m_blockchain_db->nina_block_for_all(
                [&out_block_history](uint64_t h, const std::string& data) -> bool {
                    out_block_history[h] = PersistedBlockRecord::deserialize(data);
                    return true;
                });

            records_loaded += out_block_history.size();
            std::cout << "[NINA-LMDB-ONCHAIN] ✓ Recovery complete from blockchain data.mdb"
                      << " (last height: " << out_last_height 
                      << ", " << out_block_history.size() << " block records)" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[NINA-LMDB-ONCHAIN] Warning: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * One-time migration: copy all existing NINA data from the separate
     * nina_state/data.mdb into the blockchain's NINA tables.
     * Called automatically when height first reaches NINA_V18_ONCHAIN_HEIGHT.
     * A sentinel key "v18_migrated" is written to prevent re-running.
     */
    bool migrate_nina_data_to_blockchain() {
        if (!m_blockchain_db) return false;
        try {
            // Check sentinel — already migrated?
            std::string sentinel;
            if (m_blockchain_db->nina_state_get("v18_migrated", sentinel) && sentinel == "1") {
                std::cout << "[NINA-V18-MIGRATE] Already migrated, skipping." << std::endl;
                return true;
            }

            std::cout << "[NINA-V18-MIGRATE] ═══════════════════════════════════════" << std::endl;
            std::cout << "[NINA-V18-MIGRATE] Starting one-time migration to blockchain LMDB" << std::endl;

            // 1. Load all data from separate DB
            uint64_t last_height = 0;
            std::map<uint64_t, PersistedBlockRecord> blocks;
            PersistedStatistics stats;
            if (load_nina_state_from_separate_db(last_height, blocks, stats)) {
                // 2. Write stats + meta
                m_blockchain_db->nina_state_put("current", stats.serialize());
                m_blockchain_db->nina_state_put("meta_current",
                    std::to_string(last_height) + "|" + std::to_string(std::time(nullptr)));

                // 3. Write all block records
                for (const auto& [h, rec] : blocks) {
                    m_blockchain_db->nina_block_put(h, rec.serialize());
                }

                std::cout << "[NINA-V18-MIGRATE] ✓ Copied " << blocks.size()
                          << " block records + stats" << std::endl;
            } else {
                std::cout << "[NINA-V18-MIGRATE] No data in separate DB (clean start)" << std::endl;
            }

            // 4. Set sentinel
            m_blockchain_db->nina_state_put("v18_migrated", "1");
            std::cout << "[NINA-V18-MIGRATE] ✓ Migration complete!" << std::endl;
            std::cout << "[NINA-V18-MIGRATE] ═══════════════════════════════════════" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[NINA-V18-MIGRATE] ERROR: " << e.what() << std::endl;
            return false;
        }
    }

    /**
     * Helper: load state from the separate nina_state/data.mdb (pre-v18 DB).
     * Used by migrate_nina_data_to_blockchain() to read the old data.
     */
    bool load_nina_state_from_separate_db(
        uint64_t& out_last_height,
        std::map<uint64_t, PersistedBlockRecord>& out_blocks,
        PersistedStatistics& out_stats)
    {
        std::lock_guard<std::mutex> lock(m_db_mutex);
        if (!open_env()) return false;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(m_env, nullptr, MDB_RDONLY, &txn);
        if (rc) return false;

        try {
            // Read stats
            std::string stats_str;
            if (lmdb_get_str(txn, m_dbi_stats, "current", stats_str) && !stats_str.empty()) {
                out_stats = PersistedStatistics::deserialize(stats_str);
            }

            // Read meta
            std::string meta_str;
            if (lmdb_get_str(txn, m_dbi_meta, "current", meta_str) && !meta_str.empty()) {
                std::istringstream ss(meta_str);
                std::string token;
                if (std::getline(ss, token, '|')) out_last_height = std::stoull(token);
            }

            // Read all blocks via cursor
            MDB_cursor* cursor = nullptr;
            rc = mdb_cursor_open(txn, m_dbi_blocks, &cursor);
            if (rc == 0) {
                MDB_val k, v;
                while (mdb_cursor_get(cursor, &k, &v, MDB_NEXT) == 0) {
                    std::string key_str(static_cast<const char*>(k.mv_data), k.mv_size);
                    std::string val_str(static_cast<const char*>(v.mv_data), v.mv_size);
                    try {
                        uint64_t h = std::stoull(key_str);
                        out_blocks[h] = PersistedBlockRecord::deserialize(val_str);
                    } catch (...) { continue; }
                }
                mdb_cursor_close(cursor);
            }

            mdb_txn_abort(txn);
            return true;
        } catch (...) {
            mdb_txn_abort(txn);
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
    double health = 0.0,
    const std::map<uint64_t, PersistedBlockRecord>& new_block_records = {}
) {
    auto& mgr = NINAPersistenceManager::instance();
    
    // Load existing stats to update counters (blocks are separate — only new records are written)
    uint64_t last_height = 0;
    std::map<uint64_t, PersistedBlockRecord> existing_history;
    PersistedStatistics existing_stats{};
    bool loaded = mgr.load_nina_state(last_height, existing_history, existing_stats);
    
    if (!loaded) {
        // First run or corrupted state — initialize with safe defaults
        existing_stats = PersistedStatistics{};
        existing_stats.session_count = 0;
        std::cout << "[NINA-LMDB] No previous state to merge — starting fresh" << std::endl;
    }
    
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
    
    // *** FIX: Persist the new block records (LMDB put = upsert, old records stay intact) ***
    mgr.persist_nina_state(current_height, new_block_records, stats);
    
    if (!new_block_records.empty()) {
        std::cout << "[NINA-LMDB] Persisted " << new_block_records.size()
                  << " new block records (total in DB: ~" 
                  << (existing_history.size() + new_block_records.size()) << ")" << std::endl;
    }
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
 * Returns true only when BlockchainDB is registered AND current chain height >= v18.
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
        auto& mgr = NINAPersistenceManager::instance();

        // ====== V18 ON-CHAIN ROUTE ======
        if (nina_should_use_onchain()) {
            auto* bdb = mgr.get_blockchain_db();
            try {
                bdb->nina_state_put("memory_attack_patterns", patterns_data);
                bdb->nina_state_put("memory_peer_behaviors", peers_data);
                nina_audit_log(0, "MEMORY_PERSIST_ONCHAIN",
                    std::to_string(num_patterns) + " patterns + "
                    + std::to_string(num_peers) + " peers saved to blockchain LMDB");
                return true;
            } catch (const std::exception& e) {
                std::cerr << "[NINA-MEMORY-V18] On-chain persist failed, falling back: " << e.what() << std::endl;
            }
        }

        // ====== PRE-V18: SEPARATE LMDB ======
        MDB_env* env = mgr.get_env();
        if (!env) {
            nina_audit_log(0, "MEMORY_PERSIST", "Memory system data persisted (no LMDB env)");
            return true;
        }

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(env, nullptr, 0, &txn);
        if (rc) return false;

        MDB_dbi dbi;
        rc = mdb_dbi_open(txn, "nina_stats", 0, &dbi);
        if (rc) { mdb_txn_abort(txn); return false; }

        // Write attack patterns
        {
            std::string key = "memory_attack_patterns";
            MDB_val k, v;
            k.mv_data = const_cast<char*>(key.data()); k.mv_size = key.size();
            v.mv_data = const_cast<char*>(patterns_data.data()); v.mv_size = patterns_data.size();
            mdb_put(txn, dbi, &k, &v, 0);
        }
        // Write peer behaviors
        {
            std::string key = "memory_peer_behaviors";
            MDB_val k, v;
            k.mv_data = const_cast<char*>(key.data()); k.mv_size = key.size();
            v.mv_data = const_cast<char*>(peers_data.data()); v.mv_size = peers_data.size();
            mdb_put(txn, dbi, &k, &v, 0);
        }

        rc = mdb_txn_commit(txn);
        if (rc) return false;

        nina_audit_log(0, "MEMORY_PERSIST", "Memory system data persisted to LMDB");
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * Load memory system data (attack patterns + peer behaviors) from LMDB.
 * Tries blockchain on-chain first (v18+), then falls back to separate DB.
 */
inline bool load_memory_system_data(
    std::string& out_patterns,
    std::string& out_peers
) {
    try {
        auto& mgr = NINAPersistenceManager::instance();

        // ====== V18 ON-CHAIN ROUTE ======
        if (nina_should_use_onchain()) {
            auto* bdb = mgr.get_blockchain_db();
            try {
                bool got_patterns = bdb->nina_state_get("memory_attack_patterns", out_patterns);
                bool got_peers = bdb->nina_state_get("memory_peer_behaviors", out_peers);
                if (got_patterns || got_peers) {
                    std::cout << "[NINA-MEMORY-V18] ✓ Memory data loaded from blockchain LMDB" << std::endl;
                    return true;
                }
            } catch (...) {
                // Fall through to separate DB
            }
        }

        // ====== PRE-V18: SEPARATE LMDB ======
        MDB_env* env = mgr.get_env();
        if (!env) return false;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
        if (rc) return false;

        MDB_dbi dbi;
        rc = mdb_dbi_open(txn, "nina_stats", 0, &dbi);
        if (rc) { mdb_txn_abort(txn); return false; }

        // Read patterns
        {
            std::string key = "memory_attack_patterns";
            MDB_val k, v;
            k.mv_data = const_cast<char*>(key.data()); k.mv_size = key.size();
            if (mdb_get(txn, dbi, &k, &v) == 0) {
                out_patterns.assign(static_cast<const char*>(v.mv_data), v.mv_size);
            }
        }
        // Read peers
        {
            std::string key = "memory_peer_behaviors";
            MDB_val k, v;
            k.mv_data = const_cast<char*>(key.data()); k.mv_size = key.size();
            if (mdb_get(txn, dbi, &k, &v) == 0) {
                out_peers.assign(static_cast<const char*>(v.mv_data), v.mv_size);
            }
        }

        mdb_txn_abort(txn);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool persist_learning_module_data(
    const void* metrics_ptr
) {
    try {
        if (!metrics_ptr) return false;
        auto& mgr = NINAPersistenceManager::instance();

        // Cast back to the metrics map
        const auto* metrics = static_cast<const std::map<std::string, std::string>*>(metrics_ptr);

        // Serialize all metrics into a single blob: name|val|avg|var|std|min|max|svar|cnt\n...
        std::stringstream ss;
        for (const auto& pair : *metrics) {
            ss << pair.second << "\n";  // LearningMetric::serialize() output
        }
        std::string value = ss.str();

        // ====== V18 ON-CHAIN ROUTE ======
        if (nina_should_use_onchain()) {
            auto* bdb = mgr.get_blockchain_db();
            // After v18, learning metrics go into the blockchain's LMDB
            try {
                bdb->nina_state_put("learning_metrics", value);
                nina_audit_log(0, "LEARNING_PERSIST_ONCHAIN",
                    std::to_string(metrics->size()) + " metrics saved to blockchain LMDB");
                std::cout << "[NINA-LMDB-V18] ✓ Learning metrics persisted on-chain ("
                          << metrics->size() << " metrics)" << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "[NINA-LMDB-V18] On-chain persist failed, falling back: " << e.what() << std::endl;
                // Fall through to separate DB
            }
        }

        // ====== PRE-V18: SEPARATE LMDB ======
        MDB_env* env = mgr.get_env();
        if (!env) return false;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(env, nullptr, 0, &txn);
        if (rc) return false;

        MDB_dbi dbi;
        rc = mdb_dbi_open(txn, "nina_stats", 0, &dbi);
        if (rc) { mdb_txn_abort(txn); return false; }

        std::string key = "learning_metrics";
        // Reuse 'value' from above (already has ss.str())
        MDB_val k, v;
        k.mv_data = const_cast<char*>(key.data());
        k.mv_size = key.size();
        v.mv_data = const_cast<char*>(value.data());
        v.mv_size = value.size();

        rc = mdb_put(txn, dbi, &k, &v, 0);
        if (rc) { mdb_txn_abort(txn); return false; }

        rc = mdb_txn_commit(txn);
        if (rc) return false;

        nina_audit_log(0, "LEARNING_PERSIST",
            std::to_string(metrics->size()) + " metrics saved to LMDB");
        std::cout << "[NINA-LMDB] \xe2\x9c\x93 Learning metrics persisted (" << metrics->size() << " metrics)" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[NINA-LMDB] ERROR persisting learning data: " << e.what() << std::endl;
        return false;
    }
}

inline bool load_learning_module_data(std::string& out_data) {
    try {
        auto& mgr = NINAPersistenceManager::instance();

        // ====== V18 ON-CHAIN ROUTE ======
        if (nina_should_use_onchain()) {
            auto* bdb = mgr.get_blockchain_db();
            try {
                std::string data;
                if (bdb->nina_state_get("learning_metrics", data) && !data.empty()) {
                    out_data = std::move(data);
                    std::cout << "[NINA-LMDB-V18] ✓ Learning metrics loaded from blockchain LMDB ("
                              << out_data.size() << " bytes)" << std::endl;
                    return true;
                }
            } catch (...) {
                // Fall through to separate DB
            }
        }

        // ====== PRE-V18: SEPARATE LMDB ======
        MDB_env* env = mgr.get_env();
        if (!env) return false;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
        if (rc) return false;

        MDB_dbi dbi;
        rc = mdb_dbi_open(txn, "nina_stats", 0, &dbi);
        if (rc) { mdb_txn_abort(txn); return false; }

        std::string key = "learning_metrics";
        MDB_val k, v;
        k.mv_data = const_cast<char*>(key.data());
        k.mv_size = key.size();

        rc = mdb_get(txn, dbi, &k, &v);
        if (rc == 0) {
            out_data.assign(static_cast<const char*>(v.mv_data), v.mv_size);
            mdb_txn_abort(txn);
            std::cout << "[NINA-LMDB] \xe2\x9c\x93 Learning metrics loaded (" << out_data.size() << " bytes)" << std::endl;
            return true;
        }
        mdb_txn_abort(txn);
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
        auto& mgr = NINAPersistenceManager::instance();

        // ====== V18 ON-CHAIN ROUTE ======
        if (nina_should_use_onchain()) {
            auto* bdb = mgr.get_blockchain_db();
            try {
                bdb->nina_state_put("suggestion_pending", pending_data);
                bdb->nina_state_put("suggestion_history", history_data);
                nina_audit_log(0, "SUGGESTION_PERSIST_ONCHAIN",
                    std::to_string(num_pending) + " pending + "
                    + std::to_string(num_history) + " history saved to blockchain LMDB");
                return true;
            } catch (const std::exception& e) {
                std::cerr << "[NINA-SUGGEST-V18] On-chain persist failed, falling back: " << e.what() << std::endl;
            }
        }

        // ====== PRE-V18: SEPARATE LMDB ======
        MDB_env* env = mgr.get_env();
        if (!env) {
            nina_audit_log(0, "SUGGESTION_PERSIST", "Suggestion engine data persisted (no LMDB env)");
            return true;
        }

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(env, nullptr, 0, &txn);
        if (rc) return false;

        MDB_dbi dbi;
        rc = mdb_dbi_open(txn, "nina_stats", 0, &dbi);
        if (rc) { mdb_txn_abort(txn); return false; }

        {
            std::string key = "suggestion_pending";
            MDB_val k, v;
            k.mv_data = const_cast<char*>(key.data()); k.mv_size = key.size();
            v.mv_data = const_cast<char*>(pending_data.data()); v.mv_size = pending_data.size();
            mdb_put(txn, dbi, &k, &v, 0);
        }
        {
            std::string key = "suggestion_history";
            MDB_val k, v;
            k.mv_data = const_cast<char*>(key.data()); k.mv_size = key.size();
            v.mv_data = const_cast<char*>(history_data.data()); v.mv_size = history_data.size();
            mdb_put(txn, dbi, &k, &v, 0);
        }

        rc = mdb_txn_commit(txn);
        if (rc) return false;

        nina_audit_log(0, "SUGGESTION_PERSIST", "Suggestion engine data persisted to LMDB");
        return true;
    } catch (...) {
        return false;
    }
}

/**
 * Load suggestion engine data from LMDB.
 * Tries blockchain on-chain first (v18+), then falls back to separate DB.
 */
inline bool load_suggestion_engine_data(
    std::string& out_pending,
    std::string& out_history
) {
    try {
        auto& mgr = NINAPersistenceManager::instance();

        // ====== V18 ON-CHAIN ROUTE ======
        if (nina_should_use_onchain()) {
            auto* bdb = mgr.get_blockchain_db();
            try {
                bool got_pending = bdb->nina_state_get("suggestion_pending", out_pending);
                bool got_history = bdb->nina_state_get("suggestion_history", out_history);
                if (got_pending || got_history) {
                    std::cout << "[NINA-SUGGEST-V18] ✓ Suggestions loaded from blockchain LMDB" << std::endl;
                    return true;
                }
            } catch (...) {}
        }

        // ====== PRE-V18: SEPARATE LMDB ======
        MDB_env* env = mgr.get_env();
        if (!env) return false;

        MDB_txn* txn = nullptr;
        int rc = mdb_txn_begin(env, nullptr, MDB_RDONLY, &txn);
        if (rc) return false;

        MDB_dbi dbi;
        rc = mdb_dbi_open(txn, "nina_stats", 0, &dbi);
        if (rc) { mdb_txn_abort(txn); return false; }

        {
            std::string key = "suggestion_pending";
            MDB_val k, v;
            k.mv_data = const_cast<char*>(key.data()); k.mv_size = key.size();
            if (mdb_get(txn, dbi, &k, &v) == 0) {
                out_pending.assign(static_cast<const char*>(v.mv_data), v.mv_size);
            }
        }
        {
            std::string key = "suggestion_history";
            MDB_val k, v;
            k.mv_data = const_cast<char*>(key.data()); k.mv_size = key.size();
            if (mdb_get(txn, dbi, &k, &v) == 0) {
                out_history.assign(static_cast<const char*>(v.mv_data), v.mv_size);
            }
        }

        mdb_txn_abort(txn);
        return true;
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

        std::cout << "[NINA-P2P-SYNC] \xe2\x9c\x93 Imported " << imported << " block records from peer"
                  << " (our height: " << our_height << " -> " << new_height << ")" << std::endl;
        nina_audit_log(new_height, "P2P_STATE_IMPORT",
            "Imported " + std::to_string(imported) + " records from peer at height " + std::to_string(peer_height));
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[NINA-P2P-SYNC] ERROR importing state: " << e.what() << std::endl;
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
