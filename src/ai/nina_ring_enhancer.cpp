// Copyright (c) 2026, The ninacatcoin Project
// NINA Ring Enhancer — Implementation
//
// Wallet-side privacy enhancement for ring signature decoy selection.
// Analyzes and improves the quality of assembled rings using information-
// theoretic metrics (Shannon entropy, coefficient of variation, gap analysis).
//
// This module is CONSENSUS SAFE: it only affects how the wallet *chooses*
// decoys, not how the network *validates* them. A transaction built with
// enhanced rings is indistinguishable from one built with standard selection.

#include "nina_ring_enhancer.hpp"

#include <numeric>
#include <sstream>
#include <unordered_map>

// Logging (optional — gracefully degrade if epee not available)
#ifdef NINACATCOIN_AI_MODULE
  #include "misc_log_ex.h"
  #define NINA_RING_LOG(msg)  MINFO("[NINARingEnhancer] " << msg)
  #define NINA_RING_DBG(msg)  MDEBUG("[NINARingEnhancer] " << msg)
#else
  #define NINA_RING_LOG(msg)  ((void)0)
  #define NINA_RING_DBG(msg)  ((void)0)
#endif

namespace nina {

// ═══════════════════════════════════════════════════════════════════
//  Static utility
// ═══════════════════════════════════════════════════════════════════

uint32_t NINARingEnhancer::get_time_bucket(uint64_t member_height, uint64_t current_height)
{
    if (member_height > current_height) return 0;
    const uint64_t age = current_height - member_height;

    for (size_t i = 0; i < NUM_TIME_BUCKETS - 1; ++i) {
        if (age < TIME_BUCKET_BOUNDS[i])
            return static_cast<uint32_t>(i);
    }
    return static_cast<uint32_t>(NUM_TIME_BUCKETS - 1);
}

// ═══════════════════════════════════════════════════════════════════
//  evaluate_ring  —  the main quality assessment
// ═══════════════════════════════════════════════════════════════════

RingQualityReport NINARingEnhancer::evaluate_ring(
    const std::vector<RingMember>& ring,
    uint64_t current_height
) const
{
    RingQualityReport report{};

    if (ring.size() < 2) {
        report.overall_score   = 0.0;
        report.passes_minimum  = false;
        report.recommendation  = "Ring too small for meaningful privacy";
        return report;
    }

    // ── Individual dimension scores (0–100) ─────────────────────
    report.temporal_spread_score = calculate_temporal_spread(ring, current_height) * 100.0;
    report.block_diversity_score = calculate_block_diversity(ring) * 100.0;
    report.age_variance_score    = calculate_age_variance(ring, current_height) * 100.0;
    report.clustering_resistance = calculate_clustering_resistance(ring, current_height) * 100.0;

    // ── Distinct blocks ─────────────────────────────────────────
    std::set<uint64_t> blocks;
    for (const auto& m : ring) blocks.insert(m.block_height);
    report.distinct_blocks = static_cast<uint32_t>(blocks.size());

    // ── Time buckets ────────────────────────────────────────────
    report.time_buckets_covered = count_time_buckets(ring, current_height);

    // ── Weighted composite score ────────────────────────────────
    //   30% temporal spread   — most important for resisting timing analysis
    //   25% clustering resist — catches gap-based deanonymization
    //   25% block diversity   — prevents same-block correlation
    //   20% age variance      — ensures natural-looking age distribution
    report.overall_score =
        report.temporal_spread_score * 0.30 +
        report.block_diversity_score * 0.25 +
        report.age_variance_score    * 0.20 +
        report.clustering_resistance * 0.25;

    // ── Minimum requirements ────────────────────────────────────
    const uint32_t min_blocks  = std::min(MIN_DISTINCT_BLOCKS,
                                          static_cast<uint32_t>(ring.size() - 1));
    const uint32_t min_buckets = std::min(MIN_TIME_BUCKETS,
                                          static_cast<uint32_t>(ring.size() / 3));
    report.passes_minimum =
        report.distinct_blocks      >= min_blocks  &&
        report.time_buckets_covered >= min_buckets  &&
        report.overall_score        >= 40.0;

    // ── Human-readable recommendation ───────────────────────────
    std::ostringstream rec;
    if (report.overall_score >= 80.0) {
        rec << "Excellent ring quality — strong privacy guarantees";
    } else if (report.overall_score >= 60.0) {
        rec << "Good ring quality";
        if (report.temporal_spread_score < 55.0)
            rec << "; temporal spread could be wider";
        if (report.block_diversity_score < 55.0)
            rec << "; some block clustering detected";
    } else if (report.overall_score >= 40.0) {
        rec << "Acceptable ring quality";
        if (report.temporal_spread_score < 50.0)
            rec << "; need more temporally diverse decoys";
        if (report.block_diversity_score < 50.0)
            rec << "; too many decoys from the same blocks";
        if (report.clustering_resistance < 50.0)
            rec << "; inter-member gaps too uniform";
    } else {
        rec << "POOR ring quality — privacy may be compromised";
        if (report.distinct_blocks < min_blocks)
            rec << "; only " << report.distinct_blocks
                << " distinct blocks (need " << min_blocks << ")";
        if (report.time_buckets_covered < min_buckets)
            rec << "; outputs concentrated in " << report.time_buckets_covered
                << " time zones (need " << min_buckets << ")";
    }
    report.recommendation = rec.str();

    NINA_RING_DBG("Ring quality: " << report.overall_score
        << " (temporal=" << report.temporal_spread_score
        << " blocks=" << report.block_diversity_score
        << " age=" << report.age_variance_score
        << " cluster=" << report.clustering_resistance
        << ") buckets=" << report.time_buckets_covered
        << " distinct_blocks=" << report.distinct_blocks);

    return report;
}

// ═══════════════════════════════════════════════════════════════════
//  get_adaptive_config  —  ring size recommendation
// ═══════════════════════════════════════════════════════════════════

AdaptiveRingSizeConfig NINARingEnhancer::get_adaptive_config(
    uint64_t total_rct_outputs,
    uint64_t blockchain_height,
    uint32_t hardfork_version,
    uint32_t consensus_min_ring
) const
{
    AdaptiveRingSizeConfig config{};
    config.min_outputs_for_ring_16 = MIN_OUTPUTS_RING_16;
    config.min_outputs_for_ring_21 = MIN_OUTPUTS_RING_21;
    config.current_min_ring_size   = consensus_min_ring;

    std::ostringstream rationale;

    if (total_rct_outputs < MATURITY_YOUNG) {
        config.maturity_level       = "young";
        config.recommended_ring_size = consensus_min_ring;
        rationale << total_rct_outputs << " RCT outputs — blockchain is young. "
                  << "Ring " << consensus_min_ring << " is the safe maximum. "
                  << "Larger rings would reuse decoys, harming privacy.";
    }
    else if (total_rct_outputs < MATURITY_GROWING) {
        config.maturity_level       = "growing";
        config.recommended_ring_size = std::max(consensus_min_ring, 11U);
        rationale << total_rct_outputs << " RCT outputs — blockchain is growing. "
                  << "Ring 11 provides good decoy diversity. "
                  << "Ring 16 not yet safe (need " << MIN_OUTPUTS_RING_16 << " outputs).";
    }
    else if (total_rct_outputs < MATURITY_MATURE) {
        config.maturity_level       = "mature";
        config.recommended_ring_size = std::max(consensus_min_ring, 16U);
        rationale << total_rct_outputs << " RCT outputs — blockchain is mature. "
                  << "Ring 16 is now safe with sufficient decoy diversity. "
                  << "This will be activated at HF17 via consensus.";
    }
    else {
        config.maturity_level       = "established";
        config.recommended_ring_size = std::max(consensus_min_ring, 21U);
        rationale << total_rct_outputs << " RCT outputs — blockchain is established. "
                  << "Ring 21 provides maximum privacy. "
                  << "The chain has enough diversity for rings up to 26.";
    }

    config.rationale = rationale.str();

    NINA_RING_LOG("Adaptive config: " << config.maturity_level
        << " (" << total_rct_outputs << " outputs)"
        << " → ring size " << config.recommended_ring_size
        << " (consensus min " << consensus_min_ring << ")");

    return config;
}

// ═══════════════════════════════════════════════════════════════════
//  would_improve_ring  —  incremental quality check
// ═══════════════════════════════════════════════════════════════════

bool NINARingEnhancer::would_improve_ring(
    const std::vector<RingMember>& current_ring,
    const RingMember&              candidate,
    uint64_t                       current_height
) const
{
    // ── Rule 1: Reject if block is already overcrowded ──────────
    uint32_t same_block_count = 0;
    for (const auto& m : current_ring) {
        if (m.block_height == candidate.block_height)
            ++same_block_count;
    }
    if (same_block_count >= MAX_SAME_BLOCK_MEMBERS) {
        NINA_RING_DBG("Rejecting candidate from block " << candidate.block_height
            << ": already " << same_block_count << " members from that block");
        return false;
    }

    // ── Rule 2: New time bucket always improves ─────────────────
    const uint32_t candidate_bucket = get_time_bucket(candidate.block_height, current_height);
    bool bucket_is_new = true;
    for (const auto& m : current_ring) {
        if (get_time_bucket(m.block_height, current_height) == candidate_bucket) {
            bucket_is_new = false;
            break;
        }
    }
    if (bucket_is_new) {
        NINA_RING_DBG("Candidate adds new time bucket " << candidate_bucket);
        return true;
    }

    // ── Rule 3: Compare temporal spread before/after ────────────
    const double current_spread = calculate_temporal_spread(current_ring, current_height);

    auto test_ring = current_ring;
    test_ring.push_back(candidate);
    const double new_spread = calculate_temporal_spread(test_ring, current_height);

    // Accept if spread doesn't get worse (ties allowed — the gamma picker's
    // statistical properties should still hold for the overall distribution)
    return new_spread >= current_spread;
}

// ═══════════════════════════════════════════════════════════════════
//  suggest_target_heights  —  ideal output distribution
// ═══════════════════════════════════════════════════════════════════

std::vector<uint64_t> NINARingEnhancer::suggest_target_heights(
    uint64_t current_height,
    uint32_t ring_size,
    uint64_t real_output_height
) const
{
    std::vector<uint64_t> targets;
    if (ring_size < 2 || current_height < 100) return targets;

    const uint32_t num_decoys = ring_size - 1;
    targets.reserve(num_decoys);

    // ── Allocate decoys to time buckets proportional to gamma ───
    // These weights approximate the gamma distribution's natural density.
    // More recent buckets get more decoys (matching real spend patterns).
    //
    //  Bucket  │  Age (blocks)  │  Weight  │  Meaning
    //  ────────┼────────────────┼──────────┼──────────────────
    //    0     │    0 – 100     │  0.38    │  Very recent
    //    1     │  100 – 1000    │  0.25    │  Days old
    //    2     │  1000 – 5000   │  0.16    │  ~1 week
    //    3     │  5000 – 20000  │  0.11    │  ~1 month
    //    4     │ 20000 – 100K   │  0.07    │  ~5 months
    //    5     │  100K+         │  0.03    │  Old history
    static const double bucket_weights[NUM_TIME_BUCKETS] = {
        0.38, 0.25, 0.16, 0.11, 0.07, 0.03
    };

    for (size_t bucket = 0; bucket < NUM_TIME_BUCKETS && targets.size() < num_decoys; ++bucket) {
        // At least 1 decoy per active bucket, scaled by weight
        const uint32_t n_in_bucket = static_cast<uint32_t>(
            std::max(1.0, std::round(num_decoys * bucket_weights[bucket])));

        const uint64_t bucket_start = (bucket == 0) ? 0 : TIME_BUCKET_BOUNDS[bucket - 1];
        uint64_t bucket_end = (bucket < NUM_TIME_BUCKETS - 1)
                              ? TIME_BUCKET_BOUNDS[bucket]
                              : current_height;

        // Clamp to blockchain size
        if (bucket_start >= current_height) break;
        bucket_end = std::min(bucket_end, current_height);

        const uint64_t range = bucket_end - bucket_start;

        for (uint32_t i = 0; i < n_in_bucket && targets.size() < num_decoys; ++i) {
            // Distribute evenly within bucket with slight offset to avoid edges
            const uint64_t age = bucket_start + (range * (i + 1)) / (n_in_bucket + 1);
            const uint64_t target_height = (current_height > age) ? current_height - age : 0;
            targets.push_back(target_height);
        }
    }

    NINA_RING_DBG("Suggested " << targets.size() << " target heights for ring size " << ring_size);
    return targets;
}

// ═══════════════════════════════════════════════════════════════════
//  rank_decoy_quality  —  sort decoy candidates by value
// ═══════════════════════════════════════════════════════════════════

std::vector<size_t> NINARingEnhancer::rank_decoy_quality(
    const std::vector<RingMember>& candidates,
    uint64_t current_height
) const
{
    if (candidates.empty()) return {};

    // ── Pre-compute group statistics ────────────────────────────
    std::map<uint64_t, uint32_t> block_counts;
    std::map<uint32_t, uint32_t> bucket_counts;
    for (const auto& c : candidates) {
        block_counts[c.block_height]++;
        bucket_counts[get_time_bucket(c.block_height, current_height)]++;
    }

    const double expected_per_bucket =
        static_cast<double>(candidates.size()) / NUM_TIME_BUCKETS;

    // ── Score each candidate ────────────────────────────────────
    std::vector<std::pair<double, size_t>> scores;
    scores.reserve(candidates.size());

    for (size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        double score = 50.0;  // Neutral baseline

        const uint32_t bucket       = get_time_bucket(c.block_height, current_height);
        const uint32_t in_same_block  = block_counts[c.block_height];
        const uint32_t in_same_bucket = bucket_counts[bucket];

        // === Block clustering penalty ===
        // Multiple outputs from same block are a fingerprint risk
        if (in_same_block > 1)
            score -= 15.0 * (in_same_block - 1);

        // === Time-bucket overcrowding penalty ===
        if (in_same_bucket > expected_per_bucket * 1.5)
            score -= 10.0;

        // === Underrepresented bucket bonus ===
        // Outputs in rare buckets improve temporal entropy the most
        if (in_same_bucket <= 1)
            score += 15.0;

        // === Age bonus: mid-range outputs are most valuable ===
        // Very fresh or very old outputs are over-represented in naive selection
        const uint64_t age = (current_height > c.block_height)
                             ? current_height - c.block_height : 0;
        if (age >= 100 && age <= 50000)
            score += 10.0;

        // === Unique block bonus ===
        if (in_same_block == 1)
            score += 5.0;

        scores.emplace_back(score, i);
    }

    // ── Sort descending by score ────────────────────────────────
    std::sort(scores.begin(), scores.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    std::vector<size_t> ranked;
    ranked.reserve(scores.size());
    for (const auto& s : scores) ranked.push_back(s.second);

    return ranked;
}

// ═══════════════════════════════════════════════════════════════════
//  Private: calculate_temporal_spread
// ═══════════════════════════════════════════════════════════════════
//
// Uses Shannon entropy to measure how evenly ring members are
// distributed across time buckets. Maximum entropy = perfectly
// uniform distribution across all buckets.

double NINARingEnhancer::calculate_temporal_spread(
    const std::vector<RingMember>& ring,
    uint64_t current_height
) const
{
    if (ring.size() < 2) return 0.0;

    // Count members per time bucket
    std::map<uint32_t, uint32_t> bucket_counts;
    for (const auto& m : ring)
        bucket_counts[get_time_bucket(m.block_height, current_height)]++;

    // Shannon entropy: H = -Σ p_i·log₂(p_i)
    double entropy = 0.0;
    const double n = static_cast<double>(ring.size());
    for (const auto& [bucket, count] : bucket_counts) {
        const double p = count / n;
        if (p > 0.0) entropy -= p * std::log2(p);
    }

    // Normalize to [0, 1]: max entropy = log₂(min(NUM_BUCKETS, ring_size))
    const double max_entropy = std::log2(static_cast<double>(
        std::min(static_cast<size_t>(NUM_TIME_BUCKETS), ring.size())));

    return (max_entropy > 0.0) ? std::min(1.0, entropy / max_entropy) : 0.0;
}

// ═══════════════════════════════════════════════════════════════════
//  Private: calculate_block_diversity
// ═══════════════════════════════════════════════════════════════════
//
// Measures what fraction of ring members come from unique blocks.
// Applies a penalty when any single block contributes more than
// MAX_SAME_BLOCK_MEMBERS outputs (a fingerprinting vector).

double NINARingEnhancer::calculate_block_diversity(
    const std::vector<RingMember>& ring
) const
{
    if (ring.size() < 2) return 0.0;

    // Unique block ratio
    std::set<uint64_t> unique_blocks;
    for (const auto& m : ring) unique_blocks.insert(m.block_height);
    const double ratio = static_cast<double>(unique_blocks.size()) / ring.size();

    // Find worst-case concentration in a single block
    std::map<uint64_t, uint32_t> block_counts;
    for (const auto& m : ring) block_counts[m.block_height]++;

    uint32_t max_in_block = 0;
    for (const auto& [block, count] : block_counts)
        max_in_block = std::max(max_in_block, count);

    // Penalize: each member above the threshold subtracts 0.3
    double concentration_penalty = 0.0;
    if (max_in_block > MAX_SAME_BLOCK_MEMBERS)
        concentration_penalty = 0.3 * (max_in_block - MAX_SAME_BLOCK_MEMBERS);

    return std::max(0.0, std::min(1.0, ratio - concentration_penalty));
}

// ═══════════════════════════════════════════════════════════════════
//  Private: calculate_age_variance
// ═══════════════════════════════════════════════════════════════════
//
// Uses the coefficient of variation (CV = σ/μ) of output ages.
// A CV around 1.0–2.0 indicates natural-looking diversity:
//   CV ≈ 0   → all outputs have similar age (suspicious)
//   CV ≈ 1.5 → healthy mix of recent and older outputs
//   CV > 2.0 → extreme outlier present (still acceptable)

double NINARingEnhancer::calculate_age_variance(
    const std::vector<RingMember>& ring,
    uint64_t current_height
) const
{
    if (ring.size() < 2) return 0.0;

    std::vector<double> ages;
    ages.reserve(ring.size());
    for (const auto& m : ring) {
        const double age = static_cast<double>(
            (current_height > m.block_height) ? current_height - m.block_height : 0);
        ages.push_back(age);
    }

    const double mean = std::accumulate(ages.begin(), ages.end(), 0.0) / ages.size();
    if (mean < 1.0) return 0.0;

    double sq_sum = 0.0;
    for (double a : ages) sq_sum += (a - mean) * (a - mean);
    const double stddev = std::sqrt(sq_sum / ages.size());

    // Coefficient of variation
    const double cv = stddev / mean;

    // Map CV to [0, 1] score:
    //   CV < 0.1  → very poor (all same age)
    //   CV ~ 1.5  → ideal
    //   CV > 2.0  → still good
    if (cv < 0.1) return cv / 0.1 * 0.3;      // Ramp up from 0 to 0.3
    if (cv <= 2.0) return 0.3 + 0.7 * std::min(1.0, cv / 1.5);
    return 0.9;  // Above 2.0 is fine but not perfect
}

// ═══════════════════════════════════════════════════════════════════
//  Private: calculate_clustering_resistance
// ═══════════════════════════════════════════════════════════════════
//
// Analyzes the gaps between consecutive ring members (sorted by height).
// A ring with many tiny gaps (≤2 blocks) suggests outputs were cheaply
// generated in adjacent blocks — a potential poisoned-output attack.
// Overly uniform gaps are also suspicious (machine-like pattern).

double NINARingEnhancer::calculate_clustering_resistance(
    const std::vector<RingMember>& ring,
    uint64_t /* current_height */
) const
{
    if (ring.size() < 3) return 0.5;

    // Sort heights
    std::vector<uint64_t> heights;
    heights.reserve(ring.size());
    for (const auto& m : ring) heights.push_back(m.block_height);
    std::sort(heights.begin(), heights.end());

    // Compute inter-member gaps
    std::vector<uint64_t> gaps;
    gaps.reserve(heights.size() - 1);
    for (size_t i = 1; i < heights.size(); ++i)
        gaps.push_back(heights[i] - heights[i - 1]);

    if (gaps.empty()) return 0.0;

    // Count tiny gaps (≤2 blocks apart → suspicious proximity)
    uint32_t tiny_gaps = 0;
    for (uint64_t g : gaps) {
        if (g <= 2) ++tiny_gaps;
    }
    const double tiny_ratio = static_cast<double>(tiny_gaps) / gaps.size();

    // Check gap uniformity (a perfectly uniform gap pattern is artificial)
    const uint64_t max_gap = *std::max_element(gaps.begin(), gaps.end());
    const uint64_t min_gap = *std::min_element(gaps.begin(), gaps.end());
    const double gap_ratio = (max_gap > 0)
                             ? static_cast<double>(min_gap) / max_gap
                             : 1.0;

    // Build score
    double score = 1.0;
    score -= tiny_ratio * 0.6;       // Penalize many tiny gaps
    if (gap_ratio > 0.8) score -= 0.2;  // Overly uniform = artificial

    return std::max(0.0, std::min(1.0, score));
}

// ═══════════════════════════════════════════════════════════════════
//  Private: count_time_buckets
// ═══════════════════════════════════════════════════════════════════

uint32_t NINARingEnhancer::count_time_buckets(
    const std::vector<RingMember>& ring,
    uint64_t current_height
) const
{
    std::set<uint32_t> buckets;
    for (const auto& m : ring)
        buckets.insert(get_time_bucket(m.block_height, current_height));
    return static_cast<uint32_t>(buckets.size());
}

} // namespace nina
