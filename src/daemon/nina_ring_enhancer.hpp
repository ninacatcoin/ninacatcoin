/**
 * NINA Ring Enhancer — Wallet-Side Intelligent Decoy Selection
 * =============================================================
 *
 * Wallet-layer module: improves the QUALITY of ring signatures without
 * changing consensus rules.  A transaction with any valid ring still
 * validates; the enhancer helps users create BETTER rings proactively.
 *
 * Features:
 *   1. Adaptive Spending Model (ASM) — replaces static gamma distribution
 *   2. Ring Quality Score (RQS) — 0.0 to 1.0 quality metric
 *   3. Poison Avoidance — excludes suspected poisoned outputs
 *   4. Cross-TX Independence — prevents ring overlap between user TXs
 *
 * Key rule: this is wallet-side ONLY.  It cannot reject blocks or TXs.
 *           If RQS < threshold, the wallet retries with different decoys.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <numeric>
#include <string>
#include <mutex>
#include <random>
#include <functional>
#include "cryptonote_config.h"
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_ring_enhancer"

namespace ninacatcoin_ai {

// ─── Time bucket boundaries (in blocks from tip) ────────────────────────
// 6 buckets for temporal spread analysis
static constexpr uint64_t RING_TIME_BUCKETS[] = {
    100, 1000, 5000, 20000, 100000, UINT64_MAX
};
static constexpr size_t RING_NUM_BUCKETS = 6;

// ─── Ring Quality Score weights (sum = 1.0) ─────────────────────────────
static constexpr float RQS_W_AGE_DISTRIB     = 0.25f;
static constexpr float RQS_W_AMOUNT_PLAUS    = 0.15f;
static constexpr float RQS_W_POISON_AVOID    = 0.20f;
static constexpr float RQS_W_TEMPORAL_DIV    = 0.15f;
static constexpr float RQS_W_CROSS_TX_INDEP  = 0.10f;
static constexpr float RQS_W_HIST_PATTERN    = 0.15f;

// ─── Maturity stages ────────────────────────────────────────────────────
enum ChainMaturity {
    MATURITY_YOUNG       = 0,  // < 50K outputs  — relaxed
    MATURITY_GROWING     = 1,  // < 200K          — moderate
    MATURITY_MATURE      = 2,  // < 1M            — strict
    MATURITY_ESTABLISHED = 3   // >= 5M           — maximum quality
};

// ─── Output candidate for ring member selection ─────────────────────────
struct DecoyCandidate {
    uint64_t global_index  = 0;
    uint64_t block_height  = 0;
    uint64_t age_blocks    = 0;   // tip - block_height
    float    poison_score  = 0.0f;
    bool     is_real       = false; // true for the actual spend
};

// ─── Ring Quality Score breakdown ───────────────────────────────────────
struct RingQualityBreakdown {
    float age_distribution   = 0.0f;
    float amount_plausibility= 0.0f;
    float poison_avoidance   = 0.0f;
    float temporal_diversity  = 0.0f;
    float cross_tx_independence = 0.0f;
    float historical_pattern = 0.0f;
    float total              = 0.0f;

    std::string to_string() const {
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "RQS=%.3f (age=%.2f amt=%.2f poison=%.2f temp=%.2f xTX=%.2f hist=%.2f)",
            total, age_distribution, amount_plausibility, poison_avoidance,
            temporal_diversity, cross_tx_independence, historical_pattern);
        return std::string(buf);
    }
};

// ─── Adaptive Spending Model histogram ──────────────────────────────────
struct SpendingModel {
    // Age distribution: probability weight for each bucket
    float bucket_weights[RING_NUM_BUCKETS] = {0.35f, 0.25f, 0.18f, 0.12f, 0.07f, 0.03f};
    uint64_t training_height = 0;
    uint64_t sample_count    = 0;

    void normalize() {
        float sum = 0.0f;
        for (size_t i = 0; i < RING_NUM_BUCKETS; i++) sum += bucket_weights[i];
        if (sum > 0.0f) {
            for (size_t i = 0; i < RING_NUM_BUCKETS; i++) bucket_weights[i] /= sum;
        }
    }
};

/**
 * NinaRingEnhancer
 * Singleton, wallet-side.
 */
class NinaRingEnhancer {
public:
    static NinaRingEnhancer& getInstance() {
        static NinaRingEnhancer instance;
        return instance;
    }

    // ─── Update the Adaptive Spending Model ─────────────────────────────
    /**
     * Train ASM from observed spending ages on-chain.
     * Called every NINA_RING_ASM_UPDATE_INTERVAL blocks.
     *
     * @param spending_ages  Vector of (age_in_blocks) for real spends observed
     * @param current_height Current chain height
     */
    void update_spending_model(const std::vector<uint64_t>& spending_ages,
                               uint64_t current_height)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (spending_ages.empty()) return;

        // Build histogram over the defined buckets
        float new_weights[RING_NUM_BUCKETS] = {0};
        for (uint64_t age : spending_ages) {
            for (size_t b = 0; b < RING_NUM_BUCKETS; b++) {
                if (age < RING_TIME_BUCKETS[b]) {
                    new_weights[b] += 1.0f;
                    break;
                }
            }
        }

        // Normalize
        float total = static_cast<float>(spending_ages.size());
        for (size_t b = 0; b < RING_NUM_BUCKETS; b++) {
            new_weights[b] /= total;
        }

        // Exponential moving average merge with existing model (stability)
        const float alpha = 0.3f; // learning rate
        for (size_t b = 0; b < RING_NUM_BUCKETS; b++) {
            m_model.bucket_weights[b] = (1.0f - alpha) * m_model.bucket_weights[b]
                                       + alpha * new_weights[b];
        }
        m_model.normalize();
        m_model.training_height = current_height;
        m_model.sample_count += spending_ages.size();

        MINFO("[NINA-RING-ASM] Model updated at height " << current_height
              << " from " << spending_ages.size() << " samples"
              << " buckets=[" << m_model.bucket_weights[0]
              << "," << m_model.bucket_weights[1]
              << "," << m_model.bucket_weights[2]
              << "," << m_model.bucket_weights[3]
              << "," << m_model.bucket_weights[4]
              << "," << m_model.bucket_weights[5] << "]");
    }

    // ─── Record a poison-scored output ──────────────────────────────────
    /**
     * Feed output clusters for poison scoring.
     * @param global_index  Output global index
     * @param poison_score  0.0 (clean) to 1.0 (definitely poisoned)
     */
    void record_output_poison_score(uint64_t global_index, float poison_score) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (poison_score > 0.3f) {
            m_poison_scores[global_index] = poison_score;
        }
        // Prune old entries (keep max 50K)
        while (m_poison_scores.size() > 50000) {
            m_poison_scores.erase(m_poison_scores.begin());
        }
    }

    // ─── Record recent ring usage for cross-TX independence ─────────────
    void record_used_ring_members(const std::set<uint64_t>& members) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_recent_ring_members.push_back(members);
        // Keep last 10 transactions
        if (m_recent_ring_members.size() > 10) {
            m_recent_ring_members.erase(m_recent_ring_members.begin());
        }
    }

    // ─── Compute Ring Quality Score ─────────────────────────────────────
    /**
     * Evaluate a candidate ring.
     *
     * @param candidates     All ring members (real + decoys)
     * @param chain_height   Current tip height
     * @param num_rct_outputs Total RCT outputs on chain
     * @return               Quality breakdown including total score
     */
    RingQualityBreakdown compute_quality(
        const std::vector<DecoyCandidate>& candidates,
        uint64_t chain_height,
        uint64_t num_rct_outputs) const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        RingQualityBreakdown rqs;
        ChainMaturity maturity = get_maturity(num_rct_outputs);

        rqs.age_distribution    = score_age_distribution(candidates);
        rqs.amount_plausibility = score_amount_plausibility(candidates, chain_height);
        rqs.poison_avoidance    = score_poison_avoidance(candidates);
        rqs.temporal_diversity   = score_temporal_diversity(candidates, maturity);
        rqs.cross_tx_independence = score_cross_tx_independence(candidates);
        rqs.historical_pattern  = score_historical_pattern(candidates, chain_height);

        rqs.total = RQS_W_AGE_DISTRIB     * rqs.age_distribution
                  + RQS_W_AMOUNT_PLAUS    * rqs.amount_plausibility
                  + RQS_W_POISON_AVOID    * rqs.poison_avoidance
                  + RQS_W_TEMPORAL_DIV    * rqs.temporal_diversity
                  + RQS_W_CROSS_TX_INDEP  * rqs.cross_tx_independence
                  + RQS_W_HIST_PATTERN    * rqs.historical_pattern;

        return rqs;
    }

    // ─── Select decoys using ASM (replaces gamma distribution) ──────────
    /**
     * Sample a decoy age from NINA's learned spending model.
     * Returns age in blocks from tip.
     *
     * @param max_age  Maximum valid age (chain height)
     * @return         Sampled age in blocks
     */
    uint64_t sample_decoy_age(uint64_t max_age) const {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Pick a bucket according to model weights
        float r = static_cast<float>(m_rng()) / static_cast<float>(m_rng.max());
        float cumulative = 0.0f;
        size_t chosen_bucket = RING_NUM_BUCKETS - 1;
        for (size_t b = 0; b < RING_NUM_BUCKETS; b++) {
            cumulative += m_model.bucket_weights[b];
            if (r <= cumulative) {
                chosen_bucket = b;
                break;
            }
        }

        // Sample uniformly within the bucket's block range
        uint64_t low  = (chosen_bucket == 0) ? 1 : RING_TIME_BUCKETS[chosen_bucket - 1];
        uint64_t high = std::min(RING_TIME_BUCKETS[chosen_bucket], max_age);
        if (high <= low) return low;

        uint64_t range = high - low;
        uint64_t age = low + (m_rng() % range);
        return std::min(age, max_age);
    }

    // ─── Check if output should be excluded (poisoned) ──────────────────
    bool is_poisoned(uint64_t global_index) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_poison_scores.find(global_index);
        if (it == m_poison_scores.end()) return false;
        return it->second >= NINA_RING_POISON_THRESHOLD;
    }

    // ─── Get current spending model (for serialization) ─────────────────
    SpendingModel get_model() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_model;
    }

    // ─── Set model from deserialized data ───────────────────────────────
    void set_model(const SpendingModel& m) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_model = m;
    }

    /**
     * Serialize ASM model to string for LMDB persistence.
     */
    std::string serialize_model() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        char buf[512];
        std::snprintf(buf, sizeof(buf),
            "H:%llu|N:%llu|W:%.6f,%.6f,%.6f,%.6f,%.6f,%.6f",
            (unsigned long long)m_model.training_height,
            (unsigned long long)m_model.sample_count,
            m_model.bucket_weights[0], m_model.bucket_weights[1],
            m_model.bucket_weights[2], m_model.bucket_weights[3],
            m_model.bucket_weights[4], m_model.bucket_weights[5]);
        return std::string(buf);
    }

    /**
     * Deserialize ASM model from LMDB string.
     */
    bool deserialize_model(const std::string& data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        unsigned long long h = 0, n = 0;
        float w[6] = {0};
        int rc = std::sscanf(data.c_str(),
            "H:%llu|N:%llu|W:%f,%f,%f,%f,%f,%f",
            &h, &n, &w[0], &w[1], &w[2], &w[3], &w[4], &w[5]);
        if (rc != 8) return false;
        m_model.training_height = static_cast<uint64_t>(h);
        m_model.sample_count = static_cast<uint64_t>(n);
        for (int i = 0; i < 6; i++) m_model.bucket_weights[i] = w[i];
        m_model.normalize();
        MINFO("[NINA-RING-ASM] Model restored: height=" << h
              << " samples=" << n);
        return true;
    }

private:
    NinaRingEnhancer() : m_rng(42) {} // deterministic seed for consensus compat
    NinaRingEnhancer(const NinaRingEnhancer&) = delete;
    NinaRingEnhancer& operator=(const NinaRingEnhancer&) = delete;

    mutable std::mutex m_mutex;
    mutable std::mt19937_64 m_rng;

    SpendingModel m_model;
    std::map<uint64_t, float> m_poison_scores;                   // global_index → poison score
    std::vector<std::set<uint64_t>> m_recent_ring_members;       // last N TX ring member sets

    // ─── Maturity classification ────────────────────────────────────────
    static ChainMaturity get_maturity(uint64_t num_rct) {
        if (num_rct >= 5000000) return MATURITY_ESTABLISHED;
        if (num_rct >= 1000000) return MATURITY_MATURE;
        if (num_rct >= 200000)  return MATURITY_GROWING;
        return MATURITY_YOUNG;
    }

    // ─── Maturity-aware minimum requirements ────────────────────────────
    static size_t min_distinct_blocks(ChainMaturity m) {
        switch (m) {
            case MATURITY_ESTABLISHED: return 10;
            case MATURITY_MATURE:      return 8;
            case MATURITY_GROWING:     return 6;
            default:                   return 4;
        }
    }
    static size_t min_time_buckets(ChainMaturity m) {
        switch (m) {
            case MATURITY_ESTABLISHED: return 5;
            case MATURITY_MATURE:      return 4;
            case MATURITY_GROWING:     return 4;
            default:                   return 3;
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // RQS Component Scoring Functions
    // ═══════════════════════════════════════════════════════════════════

    // ── 1. Age Distribution: KL divergence from ASM ─────────────────────
    float score_age_distribution(const std::vector<DecoyCandidate>& ring) const {
        if (ring.empty()) return 0.0f;

        // Build ring age histogram
        float ring_hist[RING_NUM_BUCKETS] = {0};
        for (const auto& c : ring) {
            for (size_t b = 0; b < RING_NUM_BUCKETS; b++) {
                if (c.age_blocks < RING_TIME_BUCKETS[b]) {
                    ring_hist[b] += 1.0f;
                    break;
                }
            }
        }
        float total = static_cast<float>(ring.size());
        for (size_t b = 0; b < RING_NUM_BUCKETS; b++) ring_hist[b] /= total;

        // KL divergence: D_KL(ring || model)
        float kl = 0.0f;
        for (size_t b = 0; b < RING_NUM_BUCKETS; b++) {
            float p = ring_hist[b];
            float q = m_model.bucket_weights[b];
            if (p > 0.001f && q > 0.001f) {
                kl += p * std::log(p / q);
            }
        }

        // Map KL to score: KL < 0.01 → 1.0, KL > 0.1 → 0.0
        if (kl <= 0.01f) return 1.0f;
        if (kl >= 0.1f)  return 0.0f;
        return 1.0f - (kl - 0.01f) / 0.09f;
    }

    // ── 2. Amount Plausibility ──────────────────────────────────────────
    // With RingCT, amounts are hidden.  Proxy: age correlates with amount.
    // Penalize if all decoys are in a very narrow age band (suspicious).
    float score_amount_plausibility(const std::vector<DecoyCandidate>& ring,
                                    uint64_t chain_height) const {
        if (ring.size() < 2) return 0.0f;

        // Compute coefficient of variation of ages
        double sum = 0, sum2 = 0;
        for (const auto& c : ring) {
            double a = static_cast<double>(c.age_blocks);
            sum += a;
            sum2 += a * a;
        }
        double n = static_cast<double>(ring.size());
        double mean = sum / n;
        if (mean < 1.0) return 0.5f; // too young to evaluate
        double variance = (sum2 / n) - (mean * mean);
        double cv = std::sqrt(std::max(0.0, variance)) / mean;

        // cv > 1.5 → good spread → 1.0;  cv < 0.3 → clustered → 0.0
        if (cv >= 1.5) return 1.0f;
        if (cv <= 0.3) return 0.0f;
        return static_cast<float>((cv - 0.3) / 1.2);
    }

    // ── 3. Poison Avoidance ─────────────────────────────────────────────
    float score_poison_avoidance(const std::vector<DecoyCandidate>& ring) const {
        if (ring.empty()) return 1.0f;

        float total_penalty = 0.0f;
        for (const auto& c : ring) {
            if (c.is_real) continue; // don't poison-score the real output
            auto it = m_poison_scores.find(c.global_index);
            if (it != m_poison_scores.end()) {
                if (it->second > 0.5f) total_penalty += it->second;
            }
        }

        size_t decoy_count = ring.size() - 1; // exclude real
        if (decoy_count == 0) return 1.0f;
        float avg_penalty = total_penalty / static_cast<float>(decoy_count);
        return std::max(0.0f, 1.0f - avg_penalty * 2.0f);
    }

    // ── 4. Temporal Diversity ───────────────────────────────────────────
    // Shannon entropy across time buckets
    float score_temporal_diversity(const std::vector<DecoyCandidate>& ring,
                                   ChainMaturity maturity) const {
        if (ring.empty()) return 0.0f;

        // Count members per time bucket
        float bucket_counts[RING_NUM_BUCKETS] = {0};
        for (const auto& c : ring) {
            for (size_t b = 0; b < RING_NUM_BUCKETS; b++) {
                if (c.age_blocks < RING_TIME_BUCKETS[b]) {
                    bucket_counts[b] += 1.0f;
                    break;
                }
            }
        }

        // Count occupied buckets
        size_t occupied = 0;
        for (size_t b = 0; b < RING_NUM_BUCKETS; b++) {
            if (bucket_counts[b] > 0) occupied++;
        }

        // Shannon entropy
        float n = static_cast<float>(ring.size());
        float entropy = 0.0f;
        for (size_t b = 0; b < RING_NUM_BUCKETS; b++) {
            float p = bucket_counts[b] / n;
            if (p > 0.0001f) entropy -= p * std::log2(p);
        }

        // Max entropy = log2(NUM_BUCKETS) ≈ 2.585
        float max_entropy = std::log2(static_cast<float>(RING_NUM_BUCKETS));
        float norm_entropy = entropy / max_entropy;

        // Check minimum bucket requirement
        size_t min_buckets = min_time_buckets(maturity);
        if (occupied < min_buckets) {
            norm_entropy *= 0.5f; // penalty for insufficient spread
        }

        return std::min(1.0f, norm_entropy);
    }

    // ── 5. Cross-TX Independence ────────────────────────────────────────
    // Penalize if this ring shares many members with recent user TXs
    float score_cross_tx_independence(const std::vector<DecoyCandidate>& ring) const {
        if (m_recent_ring_members.empty()) return 1.0f;

        std::set<uint64_t> current_members;
        for (const auto& c : ring) current_members.insert(c.global_index);

        size_t max_overlap = 0;
        for (const auto& prev : m_recent_ring_members) {
            size_t overlap = 0;
            for (uint64_t idx : current_members) {
                if (prev.count(idx)) overlap++;
            }
            max_overlap = std::max(max_overlap, overlap);
        }

        if (max_overlap == 0) return 1.0f;
        float overlap_ratio = static_cast<float>(max_overlap) / static_cast<float>(ring.size());
        return std::max(0.0f, 1.0f - overlap_ratio * 3.0f); // >33% overlap → 0.0
    }

    // ── 6. Historical Pattern ───────────────────────────────────────────
    // Check for known vulnerable ring fingerprints from research papers
    float score_historical_pattern(const std::vector<DecoyCandidate>& ring,
                                   uint64_t chain_height) const {
        if (ring.size() < 3) return 0.0f;

        float score = 1.0f;

        // Pattern 1: "Exponential Staircase" — ages increase exponentially
        // This is the gamma distribution fingerprint.  If sorted ages show
        // ratio > 1.8 between consecutive ages for >80% of pairs, flag it.
        {
            std::vector<uint64_t> ages;
            for (const auto& c : ring) ages.push_back(c.age_blocks);
            std::sort(ages.begin(), ages.end());

            size_t staircase_count = 0;
            for (size_t i = 1; i < ages.size(); i++) {
                if (ages[i-1] > 0 && static_cast<float>(ages[i]) / static_cast<float>(ages[i-1]) > 1.8f) {
                    staircase_count++;
                }
            }
            float staircase_ratio = static_cast<float>(staircase_count) / static_cast<float>(ages.size() - 1);
            if (staircase_ratio > 0.8f) score -= 0.3f;
        }

        // Pattern 2: "Block clustering" — max 2 members from same block
        {
            std::map<uint64_t, size_t> block_counts;
            for (const auto& c : ring) block_counts[c.block_height]++;
            for (const auto& [blk, cnt] : block_counts) {
                if (cnt > 2) {
                    score -= 0.2f * static_cast<float>(cnt - 2);
                }
            }
        }

        // Pattern 3: Distinct blocks check
        {
            std::set<uint64_t> unique_blocks;
            for (const auto& c : ring) unique_blocks.insert(c.block_height);
            // Need at least 6 distinct blocks (maturity-adjusted in full impl)
            if (unique_blocks.size() < 6 && ring.size() >= 11) {
                score -= 0.2f;
            }
        }

        return std::max(0.0f, std::min(1.0f, score));
    }
};

} // namespace ninacatcoin_ai
