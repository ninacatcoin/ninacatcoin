/**
 * NINA Block Data Logger
 * 
 * Records REAL blockchain features per block into a CSV file for ML training.
 * This is the bridge between the C++ daemon and the Python training pipeline.
 * 
 * Features recorded per block:
 *   - height, timestamp, solve_time, difficulty, cumulative_difficulty
 *   - txs_count, block_size_bytes
 *   - network_health (computed), miner_diversity (computed), hash_entropy
 *   - ml_confidence, ml_risk_score (from PHASE 1 if available)
 *   - block_accepted (1 = accepted by consensus, 0 = rejected)
 * 
 * Output: ~/.ninacatcoin/ml_training_data.csv (append mode)
 */

#pragma once

#include <string>
#include <cstdint>
#include <mutex>
#include <fstream>

namespace ninacatcoin_ai {

class BlockDataLogger {
public:
    static BlockDataLogger& getInstance();
    
    /**
     * Initialize logger. Creates CSV file with header if it doesn't exist.
     * @param data_dir Path to ninacatcoin data directory (e.g., ~/.ninacatcoin)
     * @return true if file opened successfully
     */
    bool initialize(const std::string& data_dir);
    
    /**
     * Log a block's features for future ML training.
     * Thread-safe. Appends one row to CSV.
     */
    void log_block(
        uint64_t height,
        uint64_t timestamp,
        uint64_t solve_time,
        uint64_t difficulty,
        uint64_t cumulative_difficulty,
        uint32_t txs_count,
        uint64_t block_size_bytes,
        double network_health,
        double miner_diversity,
        uint32_t hash_entropy,
        double ml_confidence,
        double ml_risk_score,
        bool block_accepted
    );
    
    /**
     * Flush pending writes to disk
     */
    void flush();
    
    /**
     * Get path to the training data CSV
     */
    std::string get_csv_path() const { return m_csv_path; }
    
    /**
     * Get number of blocks logged this session
     */
    uint64_t get_blocks_logged() const { return m_blocks_logged; }

private:
    BlockDataLogger() = default;
    ~BlockDataLogger();
    
    BlockDataLogger(const BlockDataLogger&) = delete;
    BlockDataLogger& operator=(const BlockDataLogger&) = delete;
    
    std::mutex m_mutex;
    std::ofstream m_file;
    std::string m_csv_path;
    uint64_t m_blocks_logged = 0;
    bool m_initialized = false;
};

} // namespace ninacatcoin_ai
