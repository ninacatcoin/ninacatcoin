/**
 * NINA Block Data Logger Implementation
 * 
 * Records real blockchain features per block into CSV for ML training.
 * This gives the ML pipeline real data instead of fabricated features.
 */

#include "nina_block_data_logger.hpp"
#include "misc_log_ex.h"
#include <cstdlib>
#include <iomanip>
#include <filesystem>

namespace ninacatcoin_ai {

BlockDataLogger& BlockDataLogger::getInstance()
{
    static BlockDataLogger instance;
    return instance;
}

BlockDataLogger::~BlockDataLogger()
{
    if (m_file.is_open()) {
        m_file.flush();
        m_file.close();
    }
}

bool BlockDataLogger::initialize(const std::string& data_dir)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_initialized) return true;
    
    m_csv_path = data_dir + "/ml_training_data.csv";
    
    // Check if file exists (to decide whether to write header)
    bool file_exists = false;
    {
        std::ifstream check(m_csv_path);
        file_exists = check.good();
    }
    
    // Open in append mode
    m_file.open(m_csv_path, std::ios::app);
    if (!m_file.is_open()) {
        MERROR("[BLOCK-LOGGER] Failed to open " << m_csv_path);
        return false;
    }
    
    // Write CSV header if new file
    if (!file_exists) {
        m_file << "height,timestamp,solve_time,difficulty,cumulative_difficulty,"
               << "txs_count,block_size_bytes,network_health,miner_diversity,"
               << "hash_entropy,ml_confidence,ml_risk_score,block_accepted"
               << std::endl;
        MINFO("[BLOCK-LOGGER] Created new training data file: " << m_csv_path);
    } else {
        MINFO("[BLOCK-LOGGER] Appending to existing training data: " << m_csv_path);
    }
    
    m_initialized = true;
    m_blocks_logged = 0;
    return true;
}

void BlockDataLogger::log_block(
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
    bool block_accepted)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    // Auto-initialize on first call using $HOME/.ninacatcoin
    if (!m_initialized) {
        std::string data_dir;
        const char* home = std::getenv("HOME");
        if (!home) home = std::getenv("USERPROFILE");
        if (home) {
            data_dir = std::string(home) + "/.ninacatcoin";
        } else {
            data_dir = ".ninacatcoin";
        }
        // Unlock temporarily for initialize (it acquires m_mutex)
        // — but we already hold the lock, so just inline the init logic:
        m_csv_path = data_dir + "/ml_training_data.csv";
        bool file_exists = false;
        {
            std::ifstream check(m_csv_path);
            file_exists = check.good();
        }
        m_file.open(m_csv_path, std::ios::app);
        if (!m_file.is_open()) {
            MERROR("[BLOCK-LOGGER] Auto-init failed to open " << m_csv_path);
            return;
        }
        if (!file_exists) {
            m_file << "height,timestamp,solve_time,difficulty,cumulative_difficulty,"
                   << "txs_count,block_size_bytes,network_health,miner_diversity,"
                   << "hash_entropy,ml_confidence,ml_risk_score,block_accepted"
                   << std::endl;
            MINFO("[BLOCK-LOGGER] Auto-created training data file: " << m_csv_path);
        } else {
            MINFO("[BLOCK-LOGGER] Auto-appending to: " << m_csv_path);
        }
        m_initialized = true;
        m_blocks_logged = 0;
    }
    
    if (!m_file.is_open()) return;
    
    m_file << height << ","
           << timestamp << ","
           << solve_time << ","
           << difficulty << ","
           << cumulative_difficulty << ","
           << txs_count << ","
           << block_size_bytes << ","
           << std::fixed << std::setprecision(6) << network_health << ","
           << std::fixed << std::setprecision(6) << miner_diversity << ","
           << hash_entropy << ","
           << std::fixed << std::setprecision(6) << ml_confidence << ","
           << std::fixed << std::setprecision(6) << ml_risk_score << ","
           << (block_accepted ? 1 : 0)
           << std::endl;
    
    m_blocks_logged++;
    
    // Flush every 100 blocks to ensure data isn't lost
    if (m_blocks_logged % 100 == 0) {
        m_file.flush();
        MINFO("[BLOCK-LOGGER] " << m_blocks_logged << " blocks logged to " << m_csv_path);
    }
}

void BlockDataLogger::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        m_file.flush();
    }
}

} // namespace ninacatcoin_ai
