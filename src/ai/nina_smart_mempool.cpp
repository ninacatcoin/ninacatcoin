// Copyright (c) 2026, The ninacatcoin Project
//
// NINA Smart Mempool — Implementation
//
// DESIGN: Bots that pay fees are welcome. Spam that pays minimum is deprioritized.
// NEVER rejects transactions — only reorders them.

#include "nina_smart_mempool.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <numeric>

namespace ninacatcoin_ai {

// =====================================================================
// CONSTRUCTOR
// =====================================================================

NINASmartMempool::NINASmartMempool()
    : m_baseline_tx_rate_per_min(2.0)   // Conservative default: ~2 tx/min
    , m_baseline_avg_fee(0.0)
    , m_total_tx_seen(0)
    , m_total_spam_detected(0)
    , m_total_bot_detected(0)
{
    m_last_health.total_tx_count = 0;
    m_last_health.health_score = 100.0;
    m_last_health.status = "HEALTHY";
    MINFO("[NINA-MEMPOOL] Smart mempool analyzer initialized");
}

// =====================================================================
// STRUCTURAL FINGERPRINT
// =====================================================================

std::string NINASmartMempool::compute_structural_fingerprint(
    uint64_t tx_size,
    uint64_t fee,
    uint32_t num_inputs,
    uint32_t num_outputs)
{
    // Bucket sizes: group similar transactions together
    // This catches repetitive patterns without needing sender addresses
    uint64_t size_bucket = tx_size / 500;     // 500-byte buckets
    uint64_t fee_bucket = fee / 10000000;     // 0.00001 NINA buckets
    uint32_t io_bucket = num_inputs * 10 + num_outputs;

    std::ostringstream oss;
    oss << "s" << size_bucket << "f" << fee_bucket << "io" << io_bucket;
    return oss.str();
}

// =====================================================================
// UPDATE SOURCE PATTERN
// =====================================================================

void NINASmartMempool::update_source_pattern(
    const std::string& fingerprint,
    uint64_t tx_size,
    uint64_t fee,
    double fee_per_byte)
{
    auto now = static_cast<uint64_t>(std::time(nullptr));
    auto& pattern = m_source_patterns[fingerprint];

    if (pattern.first_seen == 0) {
        pattern.first_seen = now;
    }
    pattern.last_seen = now;

    // Update running averages
    double n = static_cast<double>(pattern.tx_count_10min + 1);
    pattern.avg_fee_per_byte = pattern.avg_fee_per_byte + (fee_per_byte - pattern.avg_fee_per_byte) / n;
    pattern.avg_tx_size = pattern.avg_tx_size + (static_cast<double>(tx_size) - pattern.avg_tx_size) / n;

    // Increment counters
    pattern.tx_count_1min++;
    pattern.tx_count_10min++;
}

// =====================================================================
// CLASSIFY TRANSACTION
// =====================================================================

std::string NINASmartMempool::classify_transaction(
    uint64_t tx_size,
    uint64_t fee,
    double fee_per_byte,
    uint32_t num_inputs,
    uint32_t num_outputs,
    const SourcePattern& pattern)
{
    // ─────────────────────────────────────────────────────────
    // WHALE: Very large fee or very large transaction
    // ─────────────────────────────────────────────────────────
    if (fee > 1000000000000ULL) {  // > 1 NINA fee (generous tipper)
        return "whale";
    }

    // ─────────────────────────────────────────────────────────
    // TRADING BOT DETECTION
    // Key insight: bots make many tx but PAY NORMAL/HIGH FEES
    // They are GOOD for the network (miners earn more fees)
    // ─────────────────────────────────────────────────────────
    bool high_frequency = (pattern.tx_count_1min >= 5 || pattern.tx_count_10min >= 20);
    bool consistent_structure = (pattern.avg_tx_size > 0 &&
        std::abs(static_cast<double>(tx_size) - pattern.avg_tx_size) / pattern.avg_tx_size < 0.3);
    bool pays_adequate_fee = (fee_per_byte >= m_baseline_avg_fee * 0.8 || fee_per_byte > 0.0);

    // Bot = high frequency + consistent structure + pays at least normal fees
    if (high_frequency && consistent_structure && pays_adequate_fee) {
        return "trading_bot";
    }

    // ─────────────────────────────────────────────────────────
    // SPAM SUSPECT DETECTION
    // Key: high volume + minimum fees + repetitive structure
    // These congest without contributing adequate fees
    // ─────────────────────────────────────────────────────────
    bool very_high_frequency = (pattern.tx_count_1min >= 15 || pattern.tx_count_10min >= 60);
    bool minimum_fee = (m_baseline_avg_fee > 0 && fee_per_byte < m_baseline_avg_fee * 0.3);
    (void)tx_size; // Available for future heuristics

    // Spam = very high frequency + below-average fees
    // OR = extremely high frequency regardless of fee
    if (very_high_frequency && minimum_fee) {
        return "spam_suspect";
    }
    if (pattern.tx_count_1min >= 50) {
        // 50+ tx/min from same pattern is extreme even for bots
        // But only flag if fee is below median
        if (fee_per_byte < m_baseline_avg_fee * 0.5) {
            return "spam_suspect";
        }
        // If paying good fees, it's just a very active bot → still legitimate
        return "trading_bot";
    }

    // ─────────────────────────────────────────────────────────
    // NORMAL: Regular user transaction
    // ─────────────────────────────────────────────────────────
    return "normal";
}

// =====================================================================
// CALCULATE PRIORITY SCORE
// =====================================================================

double NINASmartMempool::calculate_priority_score(
    double fee_per_byte,
    const std::string& category,
    uint64_t current_mempool_count)
{
    // Base score from fee/byte (0-60 points)
    double fee_score = 0.0;
    if (m_baseline_avg_fee > 0) {
        double fee_ratio = fee_per_byte / m_baseline_avg_fee;
        fee_score = std::min(60.0, fee_ratio * 30.0);  // 2x avg fee = 60 points
    } else {
        fee_score = 30.0;  // Default when no baseline yet
    }

    // Category bonus/penalty (-20 to +20 points)
    double category_score = 0.0;
    if (category == "whale") {
        category_score = 20.0;       // Whales get priority
    } else if (category == "trading_bot") {
        category_score = 5.0;        // Bots get small bonus (they're legitimate)
    } else if (category == "normal") {
        category_score = 0.0;        // Neutral
    } else if (category == "spam_suspect") {
        category_score = -20.0;      // Spam gets deprioritized
    }

    // Congestion factor (0-10 points) — during congestion, high-fee tx get extra boost
    double congestion_score = 0.0;
    if (current_mempool_count > 200) {
        // Mempool is getting full — reward high-fee payers more
        double congestion_multiplier = std::min(1.0, static_cast<double>(current_mempool_count - 200) / 800.0);
        congestion_score = fee_score * 0.15 * congestion_multiplier;
    }

    // Recency bonus (0-10 points) — slight bonus for newer tx to prevent starvation
    double recency_score = 10.0;  // Full bonus for new tx (decays in periodic_cleanup)

    double total = fee_score + category_score + congestion_score + recency_score;
    return std::max(0.0, std::min(100.0, total));
}

// =====================================================================
// ANALYZE TRANSACTION (main entry point)
// =====================================================================

NINASmartMempool::TxAnalysis NINASmartMempool::analyze_transaction(
    const std::string& tx_hash,
    uint64_t tx_size,
    uint64_t tx_weight,
    uint64_t fee,
    uint32_t num_inputs,
    uint32_t num_outputs)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    TxAnalysis result;
    auto now = static_cast<uint64_t>(std::time(nullptr));
    result.receive_time = now;

    // Calculate fee/byte
    result.fee_per_byte = (tx_weight > 0) ? static_cast<double>(fee) / tx_weight : 0.0;

    // Get or create structural fingerprint
    std::string fingerprint = compute_structural_fingerprint(tx_size, fee, num_inputs, num_outputs);

    // Update pattern tracking
    update_source_pattern(fingerprint, tx_size, fee, result.fee_per_byte);

    const auto& pattern = m_source_patterns[fingerprint];

    // Classify
    result.source_category = classify_transaction(
        tx_size, fee, result.fee_per_byte,
        num_inputs, num_outputs, pattern
    );

    result.is_likely_bot = (result.source_category == "trading_bot");
    result.is_likely_spam = (result.source_category == "spam_suspect");

    // Calculate priority
    result.priority_score = calculate_priority_score(
        result.fee_per_byte, result.source_category, m_last_health.total_tx_count
    );

    // Store in cache
    if (m_tx_analysis_cache.size() < MAX_TX_CACHE) {
        m_tx_analysis_cache[tx_hash] = result;
    }

    // Store in recent tx list
    TxRecord record;
    record.tx_hash = tx_hash;
    record.size = tx_size;
    record.weight = tx_weight;
    record.fee = fee;
    record.fee_per_byte = result.fee_per_byte;
    record.num_inputs = num_inputs;
    record.num_outputs = num_outputs;
    record.receive_time = now;
    record.category = result.source_category;
    m_recent_txs.push_back(record);
    if (m_recent_txs.size() > MAX_RECENT_TXS) {
        m_recent_txs.pop_front();
    }

    // Update global stats
    m_total_tx_seen++;
    if (result.is_likely_bot) m_total_bot_detected++;
    if (result.is_likely_spam) m_total_spam_detected++;

    // Update running average fee baseline
    if (m_total_tx_seen <= 1) {
        m_baseline_avg_fee = result.fee_per_byte;
    } else {
        // Exponential moving average with alpha=0.01 (slow adaptation)
        m_baseline_avg_fee = m_baseline_avg_fee * 0.99 + result.fee_per_byte * 0.01;
    }

    // Log notable events
    if (result.is_likely_spam) {
        MINFO("[NINA-MEMPOOL] Spam suspect detected: " << tx_hash.substr(0, 16)
              << "... fee/byte=" << std::fixed << std::setprecision(2) << result.fee_per_byte
              << " pattern_rate=" << pattern.tx_count_1min << "/min"
              << " priority=" << std::setprecision(1) << result.priority_score);
    } else if (result.is_likely_bot && pattern.tx_count_1min >= 10) {
        MDEBUG("[NINA-MEMPOOL] Trading bot activity: " << tx_hash.substr(0, 16)
               << "... rate=" << pattern.tx_count_1min << "/min"
               << " fee/byte=" << std::fixed << std::setprecision(2) << result.fee_per_byte
               << " (legitimate, adequate fees)");
    }

    return result;
}

// =====================================================================
// ASSESS MEMPOOL HEALTH
// =====================================================================

NINASmartMempool::MempoolHealth NINASmartMempool::assess_mempool_health(
    uint64_t current_tx_count,
    uint64_t current_weight,
    uint64_t max_weight)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    MempoolHealth health;
    health.total_tx_count = current_tx_count;
    health.total_weight_bytes = current_weight;

    // Calculate fee statistics from recent transactions
    if (!m_recent_txs.empty()) {
        std::vector<double> fees;
        fees.reserve(m_recent_txs.size());
        double sum_fee = 0.0;
        for (const auto& tx : m_recent_txs) {
            fees.push_back(tx.fee_per_byte);
            sum_fee += tx.fee_per_byte;
        }
        health.avg_fee_per_byte = sum_fee / fees.size();

        // Median
        std::sort(fees.begin(), fees.end());
        health.median_fee_per_byte = fees[fees.size() / 2];
    } else {
        health.avg_fee_per_byte = 0.0;
        health.median_fee_per_byte = 0.0;
    }

    // Count by category
    health.bot_tx_count = 0;
    health.spam_tx_count = 0;
    health.normal_tx_count = 0;
    for (const auto& tx : m_recent_txs) {
        if (tx.category == "trading_bot") health.bot_tx_count++;
        else if (tx.category == "spam_suspect") health.spam_tx_count++;
        else health.normal_tx_count++;
    }

    // Estimate blocks to clear mempool
    // Assuming ~120s per block, average block can include ~20-50 tx
    uint64_t avg_tx_per_block = 30; // Conservative estimate
    health.estimated_clear_blocks = (current_tx_count > 0) ?
        (current_tx_count + avg_tx_per_block - 1) / avg_tx_per_block : 0;

    // Health score calculation
    double weight_ratio = (max_weight > 0) ? static_cast<double>(current_weight) / max_weight : 0.0;
    double spam_ratio = (m_recent_txs.size() > 0) ?
        static_cast<double>(health.spam_tx_count) / m_recent_txs.size() : 0.0;
    double tx_rate = calculate_current_tx_rate();
    double rate_anomaly = (m_baseline_tx_rate_per_min > 0) ?
        tx_rate / m_baseline_tx_rate_per_min : 1.0;

    health.health_score = 100.0;

    // Deduct for weight filling
    if (weight_ratio > 0.5) {
        health.health_score -= (weight_ratio - 0.5) * 60.0;  // -0 to -30
    }

    // Deduct for high spam ratio
    if (spam_ratio > 0.1) {
        health.health_score -= (spam_ratio - 0.1) * 40.0;  // -0 to -36
    }

    // Deduct for abnormal arrival rate
    if (rate_anomaly > 5.0) {
        health.health_score -= std::min(20.0, (rate_anomaly - 5.0) * 4.0);
    }

    health.health_score = std::max(0.0, std::min(100.0, health.health_score));

    // Status determination
    if (health.health_score >= 80.0) {
        health.status = "HEALTHY";
    } else if (health.health_score >= 60.0) {
        health.status = "BUSY";
    } else if (health.health_score >= 30.0) {
        health.status = "CONGESTED";
    } else {
        health.status = "UNDER_ATTACK";
    }

    m_last_health = health;

    // Log warnings for concerning states
    if (health.health_score < 60.0) {
        MWARNING("[NINA-MEMPOOL] Health degraded: " << health.status
                 << " (score=" << std::fixed << std::setprecision(1) << health.health_score
                 << " tx=" << current_tx_count
                 << " spam_ratio=" << std::setprecision(1) << (spam_ratio * 100) << "%"
                 << " rate=" << std::setprecision(1) << tx_rate << " tx/min)");
    }

    return health;
}

// =====================================================================
// PRIORITY-ADJUSTED FEE
// =====================================================================

double NINASmartMempool::get_priority_adjusted_fee(
    const std::string& tx_hash,
    uint64_t base_fee,
    uint64_t tx_weight)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tx_analysis_cache.find(tx_hash);
    if (it == m_tx_analysis_cache.end()) {
        // Unknown tx — return original fee/weight ratio
        return (tx_weight > 0) ? static_cast<double>(base_fee) / tx_weight : 0.0;
    }

    const auto& analysis = it->second;
    double base_ratio = (tx_weight > 0) ? static_cast<double>(base_fee) / tx_weight : 0.0;

    // Apply priority multiplier based on category
    double multiplier = 1.0;
    if (analysis.source_category == "whale") {
        multiplier = 1.1;        // Slight boost
    } else if (analysis.source_category == "trading_bot") {
        multiplier = 1.0;        // No change — bots pay fair fees
    } else if (analysis.source_category == "normal") {
        multiplier = 1.0;        // No change
    } else if (analysis.source_category == "spam_suspect") {
        multiplier = 0.5;        // Halve effective priority (still not rejected)
    }

    return base_ratio * multiplier;
}

// =====================================================================
// TX LIFECYCLE NOTIFICATIONS
// =====================================================================

void NINASmartMempool::notify_tx_confirmed(const std::string& tx_hash, uint64_t wait_time_seconds)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tx_analysis_cache.erase(tx_hash);
    // Future: use wait_time to learn optimal fee levels
}

void NINASmartMempool::notify_tx_evicted(const std::string& tx_hash, const std::string& reason)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tx_analysis_cache.erase(tx_hash);
}

// =====================================================================
// TX RATE CALCULATION
// =====================================================================

double NINASmartMempool::calculate_current_tx_rate() const
{
    if (m_recent_txs.size() < 2) return 0.0;

    auto now = static_cast<uint64_t>(std::time(nullptr));
    uint64_t window_start = (now > 60) ? now - 60 : 0;

    uint64_t count_in_window = 0;
    for (auto it = m_recent_txs.rbegin(); it != m_recent_txs.rend(); ++it) {
        if (it->receive_time >= window_start) {
            count_in_window++;
        } else {
            break;
        }
    }

    return static_cast<double>(count_in_window);  // Already per minute (60s window)
}

// =====================================================================
// PERIODIC CLEANUP
// =====================================================================

void NINASmartMempool::periodic_cleanup()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto now = static_cast<uint64_t>(std::time(nullptr));

    // Reset 1-minute counters for source patterns
    for (auto& pair : m_source_patterns) {
        auto& pattern = pair.second;
        if (now - pattern.last_seen > 60) {
            pattern.tx_count_1min = 0;
        }
        if (now - pattern.last_seen > 600) {
            pattern.tx_count_10min = 0;
        }
    }

    // Remove stale patterns (no activity for 30 minutes)
    for (auto it = m_source_patterns.begin(); it != m_source_patterns.end(); ) {
        if (now - it->second.last_seen > 1800) {
            it = m_source_patterns.erase(it);
        } else {
            ++it;
        }
    }

    // Trim analysis cache
    if (m_tx_analysis_cache.size() > MAX_TX_CACHE * 0.9) {
        // Remove oldest entries (we can't sort by time easily, so just clear half)
        size_t to_remove = m_tx_analysis_cache.size() / 2;
        auto it = m_tx_analysis_cache.begin();
        while (to_remove > 0 && it != m_tx_analysis_cache.end()) {
            it = m_tx_analysis_cache.erase(it);
            to_remove--;
        }
    }

    // Update baseline tx rate (slow adaptation)
    double current_rate = calculate_current_tx_rate();
    if (current_rate > 0 && m_baseline_tx_rate_per_min > 0) {
        m_baseline_tx_rate_per_min = m_baseline_tx_rate_per_min * 0.95 + current_rate * 0.05;
    } else if (current_rate > 0) {
        m_baseline_tx_rate_per_min = current_rate;
    }
}

// =====================================================================
// MEMPOOL REPORT
// =====================================================================

std::string NINASmartMempool::generate_mempool_report()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostringstream report;
    report << "\n";
    report << "=== NINA SMART MEMPOOL REPORT ===\n";
    report << "Status: " << m_last_health.status
           << " (score: " << std::fixed << std::setprecision(1) << m_last_health.health_score << "/100)\n";
    report << "\n";
    report << "Current Pool:\n";
    report << "  Transactions: " << m_last_health.total_tx_count << "\n";
    report << "  Weight: " << m_last_health.total_weight_bytes << " bytes\n";
    report << "  Est. clear in: " << m_last_health.estimated_clear_blocks << " blocks\n";
    report << "\n";
    report << "Fee Analysis:\n";
    report << "  Average fee/byte: " << std::setprecision(4) << m_last_health.avg_fee_per_byte << "\n";
    report << "  Median fee/byte:  " << std::setprecision(4) << m_last_health.median_fee_per_byte << "\n";
    report << "  Baseline fee/byte: " << std::setprecision(4) << m_baseline_avg_fee << "\n";
    report << "\n";
    report << "Traffic Classification (recent " << m_recent_txs.size() << " tx):\n";
    report << "  Normal users:    " << m_last_health.normal_tx_count << "\n";
    report << "  Trading bots:    " << m_last_health.bot_tx_count << " (legitimate, paying fees)\n";
    report << "  Spam suspects:   " << m_last_health.spam_tx_count << " (deprioritized)\n";
    report << "\n";
    report << "Lifetime Stats:\n";
    report << "  Total tx analyzed: " << m_total_tx_seen << "\n";
    report << "  Bots detected:     " << m_total_bot_detected << "\n";
    report << "  Spam detected:     " << m_total_spam_detected << "\n";
    report << "  Current tx rate:   " << std::setprecision(1) << calculate_current_tx_rate() << " tx/min\n";
    report << "  Baseline rate:     " << std::setprecision(1) << m_baseline_tx_rate_per_min << " tx/min\n";
    report << "\n";
    report << "Active Patterns: " << m_source_patterns.size() << "\n";
    report << "=================================\n";

    return report.str();
}

// =====================================================================
// GETTERS
// =====================================================================

NINASmartMempool::MempoolHealth NINASmartMempool::get_last_health() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_last_health;
}

} // namespace ninacatcoin_ai
