// Copyright (c) 2026, The ninacatcoin Project
//
// NINA Network Optimizer — Intelligent P2P connection management
//
// This module DOES NOT modify consensus rules. It provides:
//   1. Peer quality scoring (latency, reliability, version)
//   2. Automatic peer count adjustment based on bandwidth
//   3. Preferred connection to full-chain nodes during sync
//   4. Network health monitoring
//
// All nodes reach the same consensus regardless of these optimizations.
// This only affects HOW FAST and HOW EFFICIENTLY they get there.

#pragma once

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <mutex>
#include <chrono>
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina.network"

namespace ninacatcoin_ai {

class NINANetworkOptimizer {
public:

    // =====================================================================
    // DATA STRUCTURES
    // =====================================================================

    /**
     * @brief Quality assessment for a single peer
     */
    struct PeerScore {
        std::string peer_id;           // Peer identifier
        double quality_score;          // Combined quality (0-100)
        double latency_ms;             // Average ping latency
        double reliability;            // Uptime ratio (0.0-1.0)
        double block_validity_ratio;   // Ratio of valid blocks received
        uint64_t blocks_received;      // Total blocks received from this peer
        uint64_t invalid_blocks;       // Invalid/rejected blocks from this peer
        uint64_t peer_height;          // Last known blockchain height
        bool is_full_node;             // Has complete blockchain (not pruned)
        bool is_synced;                // Is at current chain tip
        std::string version;           // Daemon version string
        uint64_t connected_since;      // Unix timestamp
        uint64_t last_activity;        // Last data exchange timestamp
        std::string recommendation;    // "keep", "deprioritize", "disconnect_candidate"
    };

    /**
     * @brief Recommended network configuration based on measured bandwidth
     */
    struct NetworkConfig {
        uint32_t recommended_out_peers;   // Suggested --out-peers value
        uint32_t recommended_in_peers;    // Suggested --in-peers value
        uint32_t recommended_rate_up;     // KB/s upload limit
        uint32_t recommended_rate_down;   // KB/s download limit
        double   measured_bandwidth_up;   // Measured upload speed (KB/s)
        double   measured_bandwidth_down; // Measured download speed (KB/s)
        std::string bandwidth_tier;       // "low", "medium", "high", "datacenter"
        std::string explanation;          // Human-readable explanation
    };

    /**
     * @brief Overall P2P network health
     */
    struct NetworkHealth {
        uint32_t total_peers;              // Current connected peers
        uint32_t synced_peers;             // Peers at chain tip
        uint32_t full_node_peers;          // Non-pruned peers
        double   average_latency_ms;       // Average latency across peers
        double   average_peer_quality;     // Average quality score
        double   network_diversity;        // Geographic/IP diversity (0-1)
        double   health_score;             // Overall health (0-100)
        std::string  status;               // "EXCELLENT", "GOOD", "DEGRADED", "CRITICAL"
        uint32_t recommended_action;       // 0=nothing, 1=add peers, 2=drop bad peers
        std::string  action_detail;        // Description of recommended action
    };

    // =====================================================================
    // SINGLETON
    // =====================================================================

    static NINANetworkOptimizer& getInstance() {
        static NINANetworkOptimizer instance;
        return instance;
    }

    NINANetworkOptimizer(const NINANetworkOptimizer&) = delete;
    NINANetworkOptimizer& operator=(const NINANetworkOptimizer&) = delete;

    // =====================================================================
    // PEER SCORING
    // =====================================================================

    /**
     * @brief Record a new data point for a peer
     *
     * Called whenever we receive data from a peer.
     * Builds up the quality profile over time.
     *
     * @param peer_id       Unique peer identifier
     * @param latency_ms    Round-trip latency for this exchange
     * @param data_valid    Was the received data (block/tx) valid?
     * @param bytes_received Bytes received in this exchange
     * @param peer_height   Peer's current blockchain height
     * @param peer_version  Peer's daemon version
     * @param is_pruned     Is this a pruned node?
     */
    void record_peer_interaction(
        const std::string& peer_id,
        double latency_ms,
        bool data_valid,
        uint64_t bytes_received,
        uint64_t peer_height,
        const std::string& peer_version,
        bool is_pruned
    );

    /**
     * @brief Get quality score for a specific peer
     *
     * @param peer_id Peer identifier
     * @return PeerScore with quality assessment, or empty score if unknown
     */
    PeerScore get_peer_score(const std::string& peer_id);

    /**
     * @brief Get all peer scores, sorted by quality (best first)
     *
     * @return Vector of PeerScore sorted by quality_score descending
     */
    std::vector<PeerScore> get_all_peer_scores();

    /**
     * @brief Identify the worst peer (candidate for disconnection)
     *
     * When we need to make room for a better peer, this identifies
     * the lowest-quality connection to drop.
     *
     * @return PeerScore of the worst peer, or empty if no peers tracked
     */
    PeerScore get_worst_peer();

    /**
     * @brief Identify the best peers for sync (full nodes, low latency, synced)
     *
     * @param count How many peers to return
     * @return Vector of best peers for blockchain synchronization
     */
    std::vector<PeerScore> get_best_sync_peers(size_t count = 4);

    /**
     * @brief Mark a peer as disconnected
     *
     * @param peer_id Peer that disconnected
     */
    void notify_peer_disconnected(const std::string& peer_id);

    // =====================================================================
    // BANDWIDTH MEASUREMENT & AUTO-TUNING
    // =====================================================================

    /**
     * @brief Record a bandwidth measurement sample
     *
     * Called periodically to track actual network throughput.
     *
     * @param bytes_sent     Total bytes sent in measurement period
     * @param bytes_received Total bytes received in measurement period
     * @param period_seconds Duration of measurement period
     */
    void record_bandwidth_sample(
        uint64_t bytes_sent,
        uint64_t bytes_received,
        double period_seconds
    );

    /**
     * @brief Get recommended network configuration
     *
     * Based on measured bandwidth, returns optimal peer counts
     * and rate limits. The daemon can choose to apply these or not.
     *
     * @return NetworkConfig with recommendations
     */
    NetworkConfig get_recommended_config();

    // =====================================================================
    // NETWORK HEALTH
    // =====================================================================

    /**
     * @brief Assess overall P2P network health
     *
     * @param our_height Our current blockchain height
     * @return NetworkHealth assessment
     */
    NetworkHealth assess_network_health(uint64_t our_height);

    /**
     * @brief Generate detailed network report for logging/RPC
     */
    std::string generate_network_report(uint64_t our_height);

    /**
     * @brief Periodic maintenance (call from on_idle or timer)
     */
    void periodic_maintenance();

private:
    NINANetworkOptimizer();
    ~NINANetworkOptimizer() = default;

    mutable std::mutex m_mutex;

    // =====================================================================
    // INTERNAL DATA
    // =====================================================================

    // Peer tracking
    struct PeerData {
        std::string peer_id;
        std::deque<double> latency_samples;      // Last N latency measurements
        uint64_t blocks_received;
        uint64_t invalid_blocks;
        uint64_t total_bytes_received;
        uint64_t peer_height;
        std::string version;
        bool is_pruned;
        uint64_t connected_since;
        uint64_t last_activity;
        bool active;   // Currently connected
        static constexpr size_t MAX_LATENCY_SAMPLES = 50;
    };

    std::map<std::string, PeerData> m_peers;

    // Bandwidth measurement
    struct BandwidthSample {
        double upload_kbps;
        double download_kbps;
        uint64_t timestamp;
    };

    std::deque<BandwidthSample> m_bandwidth_history;
    static constexpr size_t MAX_BANDWIDTH_SAMPLES = 100;
    double m_smoothed_upload_kbps;
    double m_smoothed_download_kbps;

    // Network stats
    NetworkHealth m_last_health;

    // =====================================================================
    // INTERNAL METHODS
    // =====================================================================

    /**
     * @brief Calculate quality score for a peer based on all data points
     */
    double calculate_peer_quality(const PeerData& data, uint64_t our_height);

    /**
     * @brief Determine bandwidth tier from measured speeds
     */
    std::string classify_bandwidth_tier(double upload_kbps, double download_kbps);

    /**
     * @brief Calculate recommended peer counts for a bandwidth tier
     */
    void calculate_optimal_peers(
        const std::string& tier,
        double upload_kbps,
        double download_kbps,
        uint32_t& out_peers,
        uint32_t& in_peers
    );
};

} // namespace ninacatcoin_ai
