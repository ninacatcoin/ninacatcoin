// Copyright (c) 2026, The Ninacoin Project
//
// All rights reserved.

#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <atomic>

/**
 * NINA ML Client - Communicates with Python ML Service
 * 
 * Provides a C++ interface to request predictions from the NINA ML service running
 * in a separate Python process. Uses JSON over TCP socket communication.
 * 
 * PHASE 1: Block Validation (anomaly detection)
 * PHASE 2: Difficulty Optimization (LWMA adjustment)
 * PHASE 3: Sybil Detection (peer network analysis)
 * PHASE 4: Gas Price Optimization (supply-demand pricing)
 */

namespace NINA_ML
{

struct MLResponse
{
    bool is_valid;
    double confidence;
    double risk_score;
    double difficulty_multiplier;  // NEW: For PHASE 2 difficulty optimization
    std::string model_version;
    std::map<std::string, std::string> metadata;
    
    MLResponse() : is_valid(false), confidence(0.0), risk_score(1.0), difficulty_multiplier(1.0) {}
};

/**
 * Singleton ML Client
 * 
 * Thread-safe wrapper around socket communication with Python ML service.
 * Handles automatic reconnection, request queuing, and error recovery.
 */
class NINAMLClient
{
public:
    /**
     * Get singleton instance
     */
    static NINAMLClient& get_instance()
    {
        static NINAMLClient instance;
        return instance;
    }

    // Delete copy/move constructors
    NINAMLClient(const NINAMLClient&) = delete;
    NINAMLClient& operator=(const NINAMLClient&) = delete;
    NINAMLClient(NINAMLClient&&) = delete;

    /**
     * Connect to ML service
     * @param host Host address (default: 127.0.0.1)
     * @param port Port number (default: 5556)
     * @return true if connection successful
     */
    bool connect(const std::string& host = "127.0.0.1", uint16_t port = 5556);

    /**
     * Send feature request to ML service and get prediction
     * 
     * @param tier Tier name (e.g., "BLOCK_VALIDATION", "DIFFICULTY_OPTIMIZATION")
     * @param decision_id Unique decision identifier
     * @param features Feature map (feature_name -> feature_value)
     * @param timeout_ms Timeout in milliseconds
     * @return MLResponse with prediction, or empty response on error
     */
    MLResponse featureRequest(
        const std::string& tier,
        const std::string& decision_id,
        const std::map<std::string, double>& features,
        int timeout_ms = 100
    );

    /**
     * Check if connected to ML service
     */
    bool is_connected() const
    {
        return m_connected.load();
    }

    /**
     * Reconnect to ML service
     */
    bool reconnect();

    /**
     * Disconnect gracefully
     */
    void disconnect();

    /**
     * Get model status for a specific phase
     */
    bool getModelStatus(const std::string& phase);

    /**
     * Log a training event for offline learning
     */
    void logTrainingEvent(
        const std::string& phase,
        const std::string& outcome,
        const std::map<std::string, double>& features
    );

private:
    NINAMLClient();
    ~NINAMLClient();

    // Socket management
    int m_socket;
    std::string m_host;
    uint16_t m_port;
    std::atomic<bool> m_connected;
    
    // Thread safety
    mutable std::mutex m_socket_mutex;
    std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    
    // Request queue for async operations
    std::queue<std::string> m_request_queue;
    std::unique_ptr<std::thread> m_queue_thread;
    std::atomic<bool> m_shutdown;
    
    // Internal helper methods
    bool connectSocket();
    std::string buildJsonRequest(
        const std::string& phase,
        const std::string& decision_id,
        const std::map<std::string, double>& features
    );
    
    MLResponse parseJsonResponse(const std::string& json_response);
    
    bool sendRequest(const std::string& json_request);
    
    std::string receiveResponse(int timeout_ms);
};

/**
 * Convenience functions in NINA_ML namespace
 */

/**
 * Validate block using PHASE 1 ML model
 * 
 * @param block_hash Hash of the block
 * @param timestamp Block timestamp
 * @param difficulty Network difficulty
 * @param miner_address Miner pool identifier
 * @param txs_count Number of transactions
 * @param network_health Network health metric (0-1)
 * @param hash_entropy Bit entropy of block hash
 * @param miner_reputation Historical miner success rate (0-1)
 * @return MLResponse with validation result
 */
MLResponse validateBlock(
    const std::string& block_hash,
    uint64_t timestamp,
    double difficulty,
    const std::string& miner_address,
    int txs_count,
    double network_health,
    uint32_t hash_entropy,
    double miner_reputation
);

/**
 * Get difficulty adjustment suggestion using PHASE 2
 * 
 * @param current_difficulty Current network difficulty
 * @param last_block_time Time taken to mine the last block (seconds)
 * @param recent_block_times Recent block times for trend analysis
 * @param current_hashrate Current network hashrate (H/s)
 * @param hashrate_trend_percent Hashrate trend (% change from moving average)
 * @param target_block_time Target block time in seconds
 * @return Suggested difficulty multiplier (1.0 = no change)
 */
double suggestDifficultyAdjustment(
    double current_difficulty,
    uint64_t last_block_time,
    const std::vector<uint64_t>& recent_block_times,
    double current_hashrate,
    double hashrate_trend_percent,
    uint64_t target_block_time = 120
);

/**
 * Analyze peer for Sybil attacks using PHASE 3
 * 
 * @param peer_id Peer identifier
 * @param connection_count Number of connections from this peer
 * @param handshake_timestamp When connection was established
 * @param behavior_score Historical behavior score
 * @return Risk score (0-1), higher = more suspicious
 */
double analyzePeerSybil(
    const std::string& peer_id,
    int connection_count,
    uint64_t handshake_timestamp,
    double behavior_score
);

/**
 * Optimize gas price using PHASE 4
 * 
 * @param current_supply Current network supply utilization (0-1)
 * @param transaction_count Recent transaction count
 * @param base_price Base gas price
 * @return Optimized gas price
 */
double optimizeGasPricing(
    double current_supply,
    int transaction_count,
    double base_price
);

} // namespace NINA_ML
