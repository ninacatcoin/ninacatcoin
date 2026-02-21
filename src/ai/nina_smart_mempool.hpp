// Copyright (c) 2026, The ninacatcoin Project
//
// NINA Smart Mempool — Intelligent transaction pool analysis
//
// This module DOES NOT modify consensus rules. It provides:
//   1. Transaction prioritization scoring (fee/byte efficiency)
//   2. Spam detection (distinguishing bots from attacks)
//   3. Mempool health monitoring and alerting
//   4. Source reputation tracking
//
// KEY DESIGN PRINCIPLE:
//   Legitimate trading bots (high-frequency, normal/high fees) are WELCOME.
//   Only low-fee bulk spam from concentrated sources is deprioritized.
//   NINA never rejects transactions — only re-orders them in the mempool.

#pragma once

#include <cstdint>
#include <string>
#include <deque>
#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina.mempool"

namespace ninacatcoin_ai {

class NINASmartMempool {
public:

    // =====================================================================
    // DATA STRUCTURES
    // =====================================================================

    /**
     * @brief Per-transaction analysis result
     */
    struct TxAnalysis {
        double fee_per_byte;           // Fee efficiency ratio
        double priority_score;         // Combined priority (0-100, higher = better)
        bool   is_likely_bot;          // Detected as automated trading (NOT spam)
        bool   is_likely_spam;         // Detected as low-value spam
        std::string source_category;   // "normal", "trading_bot", "spam_suspect", "whale"
        uint64_t receive_time;         // When NINA first saw this tx
    };

    /**
     * @brief Tracking data for a transaction source (approximate, ring-sig safe)
     *
     * We track by tx structural patterns, NOT by sender address (which is
     * hidden in CryptoNote). Patterns include: tx size, fee level, timing.
     */
    struct SourcePattern {
        uint64_t tx_count_1min;        // Tx matching this pattern in last 60s
        uint64_t tx_count_10min;       // Tx matching this pattern in last 10min
        double   avg_fee_per_byte;     // Average fee/byte for this pattern
        double   avg_tx_size;          // Average tx size in bytes
        uint64_t first_seen;           // Unix timestamp of first tx in window
        uint64_t last_seen;            // Unix timestamp of most recent tx
        std::string category;          // Classified category
    };

    /**
     * @brief Overall mempool health snapshot
     */
    struct MempoolHealth {
        uint64_t total_tx_count;       // Current total tx in mempool
        uint64_t total_weight_bytes;   // Total weight of all mempool tx
        double   avg_fee_per_byte;     // Average fee/byte across pool
        double   median_fee_per_byte;  // Median fee/byte
        double   health_score;         // 0-100 (100 = perfectly healthy)
        std::string status;            // "HEALTHY", "BUSY", "CONGESTED", "UNDER_ATTACK"
        uint64_t estimated_clear_blocks; // Blocks to clear the current mempool
        uint64_t bot_tx_count;         // Tx identified as legitimate bots
        uint64_t spam_tx_count;        // Tx flagged as potential spam
        uint64_t normal_tx_count;      // Regular user transactions
    };

    // =====================================================================
    // SINGLETON
    // =====================================================================

    static NINASmartMempool& getInstance() {
        static NINASmartMempool instance;
        return instance;
    }

    NINASmartMempool(const NINASmartMempool&) = delete;
    NINASmartMempool& operator=(const NINASmartMempool&) = delete;

    // =====================================================================
    // CORE FUNCTIONS
    // =====================================================================

    /**
     * @brief Analyze a new transaction entering the mempool
     *
     * Called when a tx passes validation and is about to be added.
     * Returns analysis + priority score. Does NOT reject any tx.
     *
     * @param tx_hash    Transaction hash (hex string)
     * @param tx_size    Transaction size in bytes
     * @param tx_weight  Transaction weight (for fee calculation)
     * @param fee        Transaction fee in atomic units
     * @param num_inputs Number of inputs
     * @param num_outputs Number of outputs
     * @return TxAnalysis with priority scoring
     */
    TxAnalysis analyze_transaction(
        const std::string& tx_hash,
        uint64_t tx_size,
        uint64_t tx_weight,
        uint64_t fee,
        uint32_t num_inputs,
        uint32_t num_outputs
    );

    /**
     * @brief Get current mempool health assessment
     *
     * Analyzes the overall state of the mempool and returns health metrics.
     *
     * @param current_tx_count   Number of tx currently in mempool
     * @param current_weight     Total weight of mempool in bytes
     * @param max_weight         Maximum configured mempool weight
     * @return MempoolHealth snapshot
     */
    MempoolHealth assess_mempool_health(
        uint64_t current_tx_count,
        uint64_t current_weight,
        uint64_t max_weight
    );

    /**
     * @brief Notify NINA that a transaction was included in a block
     *
     * Helps NINA learn which types of transactions get confirmed faster.
     *
     * @param tx_hash Transaction hash
     * @param wait_time_seconds How long the tx waited in mempool
     */
    void notify_tx_confirmed(const std::string& tx_hash, uint64_t wait_time_seconds);

    /**
     * @brief Notify NINA that a transaction was evicted (mempool full)
     *
     * @param tx_hash Transaction hash
     * @param reason  Why it was evicted ("low_fee", "timeout", "double_spend")
     */
    void notify_tx_evicted(const std::string& tx_hash, const std::string& reason);

    /**
     * @brief Get priority-adjusted fee for sorting purposes
     *
     * Returns an adjusted fee that accounts for NINA's spam detection.
     * Legitimate bots keep their original fee. Spam suspects get a
     * penalty that pushes them lower in the sorting order.
     *
     * @param tx_hash   Transaction hash
     * @param base_fee  Original fee from the transaction
     * @param tx_weight Transaction weight
     * @return Adjusted fee for sorting (never negative, never above base_fee)
     */
    double get_priority_adjusted_fee(
        const std::string& tx_hash,
        uint64_t base_fee,
        uint64_t tx_weight
    );

    /**
     * @brief Generate human-readable mempool report for logging/RPC
     *
     * @return Formatted string with mempool intelligence
     */
    std::string generate_mempool_report();

    /**
     * @brief Periodic cleanup of stale tracking data
     *
     * Should be called from on_idle() or equivalent periodic timer.
     */
    void periodic_cleanup();

    /**
     * @brief Get statistics for RPC dashboard
     */
    MempoolHealth get_last_health() const;

private:
    NINASmartMempool();
    ~NINASmartMempool() = default;

    mutable std::mutex m_mutex;

    // =====================================================================
    // INTERNAL TRACKING
    // =====================================================================

    // Recent transaction records for pattern analysis
    struct TxRecord {
        std::string tx_hash;
        uint64_t    size;
        uint64_t    weight;
        uint64_t    fee;
        double      fee_per_byte;
        uint32_t    num_inputs;
        uint32_t    num_outputs;
        uint64_t    receive_time;     // unix timestamp
        std::string category;         // classified category
    };

    std::deque<TxRecord> m_recent_txs;                  // Last N transactions seen
    static constexpr size_t MAX_RECENT_TXS = 2000;

    // Pattern tracking by structural fingerprint (size_bucket + fee_bucket)
    std::unordered_map<std::string, SourcePattern> m_source_patterns;

    // Per-tx analysis cache (tx_hash -> analysis)
    std::unordered_map<std::string, TxAnalysis> m_tx_analysis_cache;
    static constexpr size_t MAX_TX_CACHE = 5000;

    // Mempool baseline statistics (learned from history)
    double m_baseline_tx_rate_per_min;   // Normal tx/minute rate
    double m_baseline_avg_fee;           // Normal average fee
    uint64_t m_total_tx_seen;            // Total tx processed by NINA
    uint64_t m_total_spam_detected;      // Total classified as spam
    uint64_t m_total_bot_detected;       // Total classified as bot

    // Last health assessment
    MempoolHealth m_last_health;

    // =====================================================================
    // INTERNAL METHODS
    // =====================================================================

    /**
     * @brief Generate a structural fingerprint for pattern matching
     *
     * Groups transactions by size bucket and fee bucket to detect
     * repetitive patterns without needing sender addresses.
     */
    std::string compute_structural_fingerprint(
        uint64_t tx_size,
        uint64_t fee,
        uint32_t num_inputs,
        uint32_t num_outputs
    );

    /**
     * @brief Classify a transaction based on its characteristics
     *
     * Categories:
     *   "normal"       - Regular user transaction
     *   "trading_bot"  - High-frequency, adequate fees (LEGITIMATE)
     *   "whale"        - Very large tx or very high fee
     *   "spam_suspect" - High frequency, minimum fees, repetitive
     */
    std::string classify_transaction(
        uint64_t tx_size,
        uint64_t fee,
        double fee_per_byte,
        uint32_t num_inputs,
        uint32_t num_outputs,
        const SourcePattern& pattern
    );

    /**
     * @brief Calculate priority score for a transaction
     *
     * Factors:
     *   - fee_per_byte (primary, ~60% weight)
     *   - category bonus/penalty (~20% weight)
     *   - mempool congestion factor (~10% weight)
     *   - recency (~10% weight)
     *
     * Trading bots with good fees score HIGH (they help the network).
     * Spam suspects with minimum fees score LOW (they congest without paying).
     */
    double calculate_priority_score(
        double fee_per_byte,
        const std::string& category,
        uint64_t current_mempool_count
    );

    /**
     * @brief Update pattern tracking with new transaction
     */
    void update_source_pattern(
        const std::string& fingerprint,
        uint64_t tx_size,
        uint64_t fee,
        double fee_per_byte
    );

    /**
     * @brief Calculate current tx arrival rate (tx/minute)
     */
    double calculate_current_tx_rate() const;
};

} // namespace ninacatcoin_ai
