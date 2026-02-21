// Copyright (c) 2026, The ninacatcoin Project
// NINA Ring Enhancer — Wallet-side ring quality improvement system
//
// This module improves the privacy of ring signatures by:
//   1. Enforcing temporal diversity among decoy outputs
//   2. Preventing block clustering (too many decoys from same block)
//   3. Adapting ring size to blockchain maturity
//   4. Scoring ring quality to detect weak anonymity sets
//   5. Ranking candidate decoys by contribution to ring diversity
//
// IMPORTANT: This is a wallet-side enhancement ONLY.
// It does NOT change consensus rules. No hard fork required.
// Transactions created with enhanced rings are fully compatible
// with standard nodes — they just have better privacy properties.
//
// Paper references:
//   - Miller et al. (2017): https://arxiv.org/pdf/1704.04299/
//   - Goodell et al. CLSAG (2019): https://eprint.iacr.org/2019/654
//   - Möser et al. output merging analysis (2018)

#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

namespace nina {

// ═══════════════════════════════════════════════════════════════════
//  Data structures
// ═══════════════════════════════════════════════════════════════════

/// Represents one member of a ring (real output or decoy)
struct RingMember {
    uint64_t global_index;   ///< Global output index on-chain
    uint64_t block_height;   ///< Block where this output appeared
    bool     is_real;        ///< True only for the actual spent output (wallet-side knowledge)
};

/// Comprehensive quality report for an assembled ring
struct RingQualityReport {
    double   overall_score;           ///< Composite score 0.0 – 100.0
    double   temporal_spread_score;   ///< Entropy of time-bucket distribution (0–100)
    double   block_diversity_score;   ///< Ratio of unique blocks to ring size (0–100)
    double   age_variance_score;      ///< Coefficient-of-variation-based score (0–100)
    double   clustering_resistance;   ///< Resistance to gap-based cluster analysis (0–100)
    uint32_t distinct_blocks;         ///< Number of different blocks represented
    uint32_t time_buckets_covered;    ///< Number of time zones covered (out of 6)
    bool     passes_minimum;          ///< Meets NINA enhanced minimum requirements
    std::string recommendation;       ///< Human-readable diagnostic
};

/// Adaptive ring size recommendation based on blockchain maturity
struct AdaptiveRingSizeConfig {
    uint64_t    min_outputs_for_ring_16;   ///< RCT outputs needed for safe ring 16
    uint64_t    min_outputs_for_ring_21;   ///< RCT outputs needed for safe ring 21
    uint32_t    current_min_ring_size;     ///< Consensus-enforced minimum
    uint32_t    recommended_ring_size;     ///< NINA's recommendation
    std::string maturity_level;            ///< "young" | "growing" | "mature" | "established"
    std::string rationale;                 ///< Explanation of why this size was chosen
};

// ═══════════════════════════════════════════════════════════════════
//  NINARingEnhancer — the core class
// ═══════════════════════════════════════════════════════════════════

class NINARingEnhancer {
public:
    // ── Quality thresholds ──────────────────────────────────────
    // These are tuned for CryptoNote blockchains with 120s block target.

    /// Minimum distinct blocks required in a ring (capped to ring_size-1)
    static constexpr uint32_t MIN_DISTINCT_BLOCKS     = 6;

    /// Minimum time buckets (zones) that should be covered
    static constexpr uint32_t MIN_TIME_BUCKETS        = 4;

    /// Maximum ring members allowed from the same block
    static constexpr uint32_t MAX_SAME_BLOCK_MEMBERS  = 2;

    // ── Blockchain maturity thresholds (in RCT outputs) ─────────
    static constexpr uint64_t MATURITY_YOUNG       =   50000;  ///< < 50K outputs
    static constexpr uint64_t MATURITY_GROWING     =  200000;  ///< < 200K outputs
    static constexpr uint64_t MATURITY_MATURE      = 1000000;  ///< < 1M outputs
    static constexpr uint64_t MATURITY_ESTABLISHED = 5000000;  ///< ≥ 5M outputs

    // ── Safe output counts per ring size ────────────────────────
    // With fewer outputs than these, the ring size would strain diversity.
    static constexpr uint64_t MIN_OUTPUTS_RING_11  =    5000;
    static constexpr uint64_t MIN_OUTPUTS_RING_16  =  100000;
    static constexpr uint64_t MIN_OUTPUTS_RING_21  =  500000;

    // ── Time bucket boundaries (blocks from chain tip) ──────────
    //  Bucket 0:      0 –    100  (~3.3 hours)
    //  Bucket 1:    100 –  1,000  (~1.4 days)
    //  Bucket 2:  1,000 –  5,000  (~7 days)
    //  Bucket 3:  5,000 – 20,000  (~28 days)
    //  Bucket 4: 20,000 –100,000  (~139 days)
    //  Bucket 5: 100,000+         (older)
    static constexpr uint64_t TIME_BUCKET_BOUNDS[] = {100, 1000, 5000, 20000, 100000};
    static constexpr size_t   NUM_TIME_BUCKETS     = 6;

    NINARingEnhancer() = default;

    // ═════════════════════════════════════════════════════════════
    //  Public API
    // ═════════════════════════════════════════════════════════════

    /// Evaluate the quality of an assembled ring.
    /// @param ring           Vector of ring members (real + decoys)
    /// @param current_height Current blockchain height
    /// @return Detailed quality report
    RingQualityReport evaluate_ring(
        const std::vector<RingMember>& ring,
        uint64_t current_height
    ) const;

    /// Get adaptive ring size recommendation based on chain maturity.
    /// @param total_rct_outputs   Total unlocked RCT outputs on chain
    /// @param blockchain_height   Current chain height
    /// @param hardfork_version    Active HF version
    /// @param consensus_min_ring  Minimum ring size enforced by consensus
    /// @return Configuration with recommended ring size and rationale
    AdaptiveRingSizeConfig get_adaptive_config(
        uint64_t total_rct_outputs,
        uint64_t blockchain_height,
        uint32_t hardfork_version,
        uint32_t consensus_min_ring
    ) const;

    /// Check whether a candidate decoy would improve ring quality.
    /// Used during decoy selection to prefer better picks.
    /// @param current_ring   Ring assembled so far
    /// @param candidate      Proposed new member
    /// @param current_height Current blockchain height
    /// @return true if candidate improves diversity
    bool would_improve_ring(
        const std::vector<RingMember>& current_ring,
        const RingMember& candidate,
        uint64_t current_height
    ) const;

    /// Suggest target block heights for well-distributed decoy selection.
    /// These heights represent ideal temporal positions; the caller
    /// should pick the closest available output to each target.
    /// @param current_height     Tip of the chain
    /// @param ring_size          Desired ring size
    /// @param real_output_height Block height of the real output
    /// @return Vector of target heights (one per decoy needed)
    std::vector<uint64_t> suggest_target_heights(
        uint64_t current_height,
        uint32_t ring_size,
        uint64_t real_output_height
    ) const;

    /// Rank candidate decoys by quality contribution.
    /// Returns indices into the candidates vector, best first.
    /// @param candidates     All candidate decoys
    /// @param current_height Current blockchain height
    /// @return Indices sorted from highest to lowest quality
    std::vector<size_t> rank_decoy_quality(
        const std::vector<RingMember>& candidates,
        uint64_t current_height
    ) const;

    /// Utility: classify a block height into its time bucket.
    static uint32_t get_time_bucket(uint64_t member_height, uint64_t current_height);

private:
    // ── Scoring sub-functions ───────────────────────────────────

    /// Shannon entropy of time-bucket distribution, normalized to [0,1]
    double calculate_temporal_spread(
        const std::vector<RingMember>& ring,
        uint64_t current_height
    ) const;

    /// Ratio of unique blocks to ring size, with concentration penalty
    double calculate_block_diversity(
        const std::vector<RingMember>& ring
    ) const;

    /// Coefficient-of-variation of output ages, mapped to [0,1]
    double calculate_age_variance(
        const std::vector<RingMember>& ring,
        uint64_t current_height
    ) const;

    /// Gap-analysis resistance score: penalizes tiny inter-member gaps
    double calculate_clustering_resistance(
        const std::vector<RingMember>& ring,
        uint64_t current_height
    ) const;

    /// Count how many of the 6 time buckets are represented
    uint32_t count_time_buckets(
        const std::vector<RingMember>& ring,
        uint64_t current_height
    ) const;
};

} // namespace nina
