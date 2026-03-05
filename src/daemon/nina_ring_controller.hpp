/**
 * NINA Ring Controller — v18 Active Ring Signature Management
 * ============================================================
 *
 * Consensus-layer module: determines the minimum ring size per block
 * based on real-time threat assessment from blockchain data.
 *
 * Before v18 (height < 20000): passive, RCT-threshold based (already in blockchain.cpp)
 * After  v18 (height >= 20000): NINA actively controls ring parameters
 *
 * ALL inputs come from the blockchain itself → deterministic on all nodes.
 * ALL outputs are verified via coinbase tag 0xCB → consensus-safe.
 *
 * Threat Levels:
 *   0 NORMAL   → Ring 16 (mixin 15)
 *   1 ELEVATED → Ring 21 (mixin 20) — suspicious activity
 *   2 HIGH     → Ring 26 (mixin 25) — active attack patterns
 *   3 CRITICAL → Ring 31 (mixin 30) — confirmed attack
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include "cryptonote_config.h"
#include "misc_log_ex.h"
#include "daemon/nina_decision_engine.hpp"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_ring"

namespace ninacatcoin_ai {

// ─── Threat Levels ──────────────────────────────────────────────────────
enum NinaThreatLevel : uint8_t {
    THREAT_NORMAL   = 0,
    THREAT_ELEVATED = 1,
    THREAT_HIGH     = 2,
    THREAT_CRITICAL = 3
};

// ─── Threat Assessment (5 indicators) ───────────────────────────────────
struct NinaThreatAssessment {
    float output_poisoning_score  = 0.0f;   // 0.0 – 1.0
    float temporal_attack_score   = 0.0f;   // 0.0 – 1.0
    float sybil_peer_score        = 0.0f;   // 0.0 – 1.0
    float ring_analysis_score     = 0.0f;   // 0.0 – 1.0
    float network_anomaly_score   = 0.0f;   // 0.0 – 1.0

    float combined_threat() const {
        return std::max({output_poisoning_score,
                         temporal_attack_score,
                         sybil_peer_score,
                         ring_analysis_score,
                         network_anomaly_score});
    }
};

// ─── Ring Decision (output for a given block) ───────────────────────────
struct NinaRingDecision {
    uint64_t height           = 0;
    uint8_t  threat_level     = 0;     // NinaThreatLevel
    uint16_t min_ring_size    = 16;    // ring size (mixin+1)
    float    avg_rqs          = 0.0f;  // average Ring Quality Score this block
    uint64_t model_hash_first8 = 0;
    NinaThreatAssessment assessment;
};

// ─── Per-block output creation stats (sliding window) ───────────────────
struct OutputWindowEntry {
    uint64_t height           = 0;
    uint64_t new_outputs      = 0;
    uint64_t spend_within_10  = 0;   // outputs spent ≤10 blocks after creation
    uint64_t total_txs        = 0;
};

/**
 * NinaRingController
 * Singleton, consensus-critical: must produce identical results on all nodes.
 */
class NinaRingController {
public:
    static NinaRingController& getInstance() {
        static NinaRingController instance;
        return instance;
    }

    // ─── Main entry point (called from blockchain.cpp) ──────────────────
    /**
     * Evaluate the required minimum mixin for a given block.
     * Only active when height >= NINA_V18_RING_ACTIVE_HEIGHT (20000).
     *
     * ARCHITECTURE: NINA DECIDES.
     *   1. Collect all traffic metrics (outputs/tx, volume ramp, spend ratio)
     *   2. Build NinaEvent(RING_THREAT_EVALUATION) with all data
     *   3. NinaDecisionEngine::evaluate() → NINA LLM (temp=0.0) decides
     *   4. If LLM unavailable → fallback with organic traffic detection
     *   5. Apply hysteresis to prevent oscillation
     *   6. Return mixin (never below legacy floor)
     *
     * @param height            Current block height
     * @param num_rct           Total RCT outputs on-chain
     * @param hf_version        Active hard fork version
     * @param outputs_last_100  New outputs created in the last 100 blocks
     * @param spends_within_10  Outputs spent within 10 blocks of creation
     * @param hashrate_drop_pct Hashrate drop percentage vs 60-block avg (0-100)
     * @param sybil_score       Sybil detection score from NINAMemorySystem (0.0-1.0)
     * @param total_txs_last_100 Total transactions in the last 100 blocks
     * @param prev_window_outputs Outputs in the previous 100-block window (for ramp detection)
     * @return              Required minimum mixin (NOT ring size; ring = mixin+1)
     */
    size_t evaluate_min_mixin(
        uint64_t height,
        uint64_t num_rct,
        uint8_t  hf_version,
        uint64_t outputs_last_100,
        uint64_t spends_within_10,
        float    hashrate_drop_pct,
        float    sybil_score,
        uint64_t total_txs_last_100 = 0,
        uint64_t prev_window_outputs = 0)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // ── Pre-v18: delegate to legacy thresholds ──
        if (height < NINA_V18_RING_ACTIVE_HEIGHT) {
            return get_legacy_mixin(num_rct, hf_version);
        }

        // ── Compute organic traffic metrics from sliding window ──
        // If caller didn't provide txs_last_100, estimate from our own window
        uint64_t txs = total_txs_last_100;
        uint64_t prev_out = prev_window_outputs;
        if (txs == 0 && !m_output_window.empty()) {
            for (const auto& [h, entry] : m_output_window) {
                txs += entry.total_txs;
            }
        }
        if (prev_out == 0 && m_prev_window_outputs > 0) {
            prev_out = m_prev_window_outputs;
        }

        // ── v18+: ASK NINA to evaluate the traffic pattern ──
        NinaEvent event(NinaEventType::RING_THREAT_EVALUATION);
        event.set("height", height);
        event.set("num_rct_outputs", num_rct);
        event.set("outputs_last_100", outputs_last_100);
        event.set("txs_last_100", txs);
        event.set("spends_within_10", spends_within_10);
        event.set("prev_window_outputs", prev_out);
        event.set("hashrate_drop_pct", static_cast<double>(hashrate_drop_pct));
        event.set("sybil_score", static_cast<double>(sybil_score));

        // Compute outputs_per_tx for the prompt (so LLM has the insight)
        float outputs_per_tx = (txs > 0)
            ? static_cast<float>(outputs_last_100) / static_cast<float>(txs)
            : 0.0f;
        float volume_ramp = (prev_out > 0)
            ? static_cast<float>(outputs_last_100) / static_cast<float>(prev_out)
            : 1.0f;
        float spend_ratio = (outputs_last_100 > 0)
            ? static_cast<float>(spends_within_10) / static_cast<float>(outputs_last_100)
            : 0.0f;
        event.set("outputs_per_tx", static_cast<double>(outputs_per_tx));
        event.set("volume_ramp", static_cast<double>(volume_ramp));
        event.set("spend_ratio", static_cast<double>(spend_ratio));

        // \u2500\u2500 NINA DECIDES \u2500\u2500
        auto actions = NinaDecisionEngine::getInstance().evaluate(event);

        NinaThreatLevel new_level = THREAT_NORMAL;  // default
        bool nina_decided = false;

        for (const auto& action : actions) {
            if (action.type == NinaActionType::ADJUST_RING_THREAT) {
                uint8_t tl = 0;
                try { tl = static_cast<uint8_t>(std::stoi(action.get_param("threat_level", "0"))); }
                catch (...) {}
                if (tl > 3) tl = 3;
                new_level = static_cast<NinaThreatLevel>(tl);
                nina_decided = true;

                MINFO("[NINA-RING] NINA decided threat=" << threat_level_name(new_level)
                      << " confidence=" << action.confidence
                      << " organic=" << action.get_param("is_organic", "unknown")
                      << " reason: " << action.reasoning);

                NinaDecisionEngine::getInstance().record_action_executed(NinaActionType::ADJUST_RING_THREAT);
                break;
            }
        }

        if (!nina_decided) {
            // NINA returned no ring action — use old assessment as safety net
            NinaThreatAssessment assessment = compute_threat(
                height, num_rct, outputs_last_100, spends_within_10,
                hashrate_drop_pct, sybil_score);
            new_level = level_from_score(assessment.combined_threat());
            m_last_assessment = assessment;
            MINFO("[NINA-RING] No NINA ring decision — using legacy assessment, threat="
                  << threat_level_name(new_level));
        }

        // Hysteresis: require sustained threat before escalating
        new_level = apply_hysteresis(height, new_level);

        // Record the decision
        m_current_level = new_level;

        size_t mixin = mixin_for_level(new_level);

        // Never go below the legacy RCT-threshold floor
        size_t legacy_floor = get_legacy_mixin(num_rct, hf_version);
        mixin = std::max(mixin, legacy_floor);

        m_last_decision.height         = height;
        m_last_decision.threat_level   = static_cast<uint8_t>(new_level);
        m_last_decision.min_ring_size  = static_cast<uint16_t>(mixin + 1);

        if (new_level > THREAT_NORMAL) {
            MINFO("[NINA-RING] Height " << height
                  << " threat=" << threat_level_name(new_level)
                  << " min_ring=" << (mixin + 1)
                  << " decided_by=" << (nina_decided ? "NINA_LLM" : "LEGACY_FALLBACK"));
        }

        return mixin;
    }

    // ─── Grace period check ─────────────────────────────────────────────
    /**
     * Check if a transaction with the given mixin should be accepted
     * during a ring size increase grace window.
     */
    bool is_in_grace_period(uint64_t height, size_t tx_mixin) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (height < NINA_V18_RING_ACTIVE_HEIGHT) return false;
        if (m_last_level_change_height == 0) return false;
        uint64_t since = height - m_last_level_change_height;
        if (since > NINA_RING_GRACE_BLOCKS) return false;
        // Accept the previous required mixin during grace
        size_t prev_mixin = mixin_for_level(m_previous_level);
        return (tx_mixin == prev_mixin);
    }

    // ─── Accessors ──────────────────────────────────────────────────────
    NinaThreatLevel      get_current_level()      const { return m_current_level; }
    NinaRingDecision     get_last_decision()      const { return m_last_decision; }
    NinaThreatAssessment get_last_assessment()     const { return m_last_assessment; }

    static const char* threat_level_name(NinaThreatLevel l) {
        switch (l) {
            case THREAT_NORMAL:   return "NORMAL";
            case THREAT_ELEVATED: return "ELEVATED";
            case THREAT_HIGH:     return "HIGH";
            case THREAT_CRITICAL: return "CRITICAL";
            default:              return "UNKNOWN";
        }
    }

    /**
     * Serialize the current ring decision to a compact string for LMDB / coinbase tag.
     * Format: "TL:X|RING:Y|POISON:Z|TEMP:W|SYBIL:V|NET:U"
     */
    std::string serialize_decision() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "TL:%u|RING:%u|POISON:%.3f|TEMP:%.3f|SYBIL:%.3f|RANAL:%.3f|NET:%.3f",
            m_last_decision.threat_level,
            m_last_decision.min_ring_size,
            m_last_assessment.output_poisoning_score,
            m_last_assessment.temporal_attack_score,
            m_last_assessment.sybil_peer_score,
            m_last_assessment.ring_analysis_score,
            m_last_assessment.network_anomaly_score);
        return std::string(buf);
    }

    /**
     * Feed observed output creation data for the sliding window.
     * Called once per block from nina_advanced_observe_block().
     */
    void observe_block_outputs(uint64_t height, uint64_t new_outputs,
                               uint64_t spend_within_10, uint64_t total_txs) {
        std::lock_guard<std::mutex> lock(m_mutex);
        OutputWindowEntry e;
        e.height = height;
        e.new_outputs = new_outputs;
        e.spend_within_10 = spend_within_10;
        e.total_txs = total_txs;
        m_output_window[height] = e;

        // Prune entries older than analysis window
        if (m_output_window.size() > NINA_RING_THREAT_ANALYSIS_WINDOW + 20) {
            // Before pruning, snapshot the outgoing window as "previous" for ramp detection
            uint64_t prune_outputs = 0;
            auto it = m_output_window.begin();
            while (it != m_output_window.end() && it->first + NINA_RING_THREAT_ANALYSIS_WINDOW + 20 < height) {
                prune_outputs += it->second.new_outputs;
                it = m_output_window.erase(it);
            }
            if (prune_outputs > 0)
                m_prev_window_outputs = prune_outputs;
        }
    }

private:
    NinaRingController() = default;
    NinaRingController(const NinaRingController&) = delete;
    NinaRingController& operator=(const NinaRingController&) = delete;

    mutable std::mutex m_mutex;

    NinaThreatLevel      m_current_level  = THREAT_NORMAL;
    NinaThreatLevel      m_previous_level = THREAT_NORMAL;
    NinaThreatAssessment m_last_assessment;
    NinaRingDecision     m_last_decision;

    uint64_t m_last_level_change_height   = 0;
    uint64_t m_sustained_higher_count     = 0;  // consecutive blocks at higher level
    uint64_t m_sustained_lower_count      = 0;  // consecutive blocks at lower level
    uint64_t m_prev_window_outputs        = 0;  // outputs in previous 100-block window

    std::map<uint64_t, OutputWindowEntry> m_output_window;

    // ─── Legacy RCT-threshold mixin (v17 / pre-v18 fallback) ────────────
    static size_t get_legacy_mixin(uint64_t num_rct, uint8_t hf_version) {
        size_t base = hf_version >= HF_VERSION_MIN_MIXIN_15 ? 15
                    : hf_version >= HF_VERSION_MIN_MIXIN_10 ? 10
                    : hf_version >= HF_VERSION_MIN_MIXIN_6  ? 6
                    : hf_version >= HF_VERSION_MIN_MIXIN_4  ? 4
                    : 2;
        if (num_rct >= NINA_RING_21_RCT_THRESHOLD)
            return std::max(base, static_cast<size_t>(20));
        if (num_rct >= NINA_RING_16_RCT_THRESHOLD)
            return std::max(base, static_cast<size_t>(15));
        return base;
    }

    // ─── Compute 5 threat indicators ────────────────────────────────────
    NinaThreatAssessment compute_threat(
        uint64_t height,
        uint64_t num_rct,
        uint64_t outputs_last_100,
        uint64_t spends_within_10,
        float    hashrate_drop_pct,
        float    sybil_score)
    {
        NinaThreatAssessment a;

        // ── 1. Output Poisoning Score ──
        // Normal: ~50-200 new outputs per 100 blocks
        // Attack: >2000 from clustered sources
        const float expected_outputs = 200.0f;
        const float poison_threshold = 2000.0f;
        if (outputs_last_100 > expected_outputs) {
            float excess = static_cast<float>(outputs_last_100) - expected_outputs;
            a.output_poisoning_score = std::min(1.0f, excess / (poison_threshold - expected_outputs));
        }

        // ── 2. Temporal Attack Score ──
        // Outputs spent very quickly after creation = EAE attack pattern
        // Normal: <5% spent within 10 blocks
        float spend_ratio = outputs_last_100 > 0
            ? static_cast<float>(spends_within_10) / static_cast<float>(outputs_last_100)
            : 0.0f;
        const float normal_spend_ratio = 0.05f;
        if (spend_ratio > normal_spend_ratio) {
            a.temporal_attack_score = std::min(1.0f,
                (spend_ratio - normal_spend_ratio) / (0.3f - normal_spend_ratio));
        }

        // ── 3. Sybil Peer Score ──
        // Directly from the Sybil detector module
        a.sybil_peer_score = std::min(1.0f, std::max(0.0f, sybil_score));

        // ── 4. Ring Analysis Score ──
        // Detect if ring members are being systematically eliminated.
        // Proxy: if many outputs are being spent in short temporal clusters,
        // it suggests coordinated deanonymization attempts.
        // Using the ratio of quick-spend outputs weighted higher.
        if (spend_ratio > 0.15f && outputs_last_100 > 500) {
            a.ring_analysis_score = std::min(1.0f,
                (spend_ratio - 0.15f) / 0.35f * (static_cast<float>(outputs_last_100) / 1000.0f));
        }

        // ── 5. Network Anomaly Score ──
        // Hashrate drops, mempool flooding, unusual fee structures
        float hr_score = 0.0f;
        if (hashrate_drop_pct > 20.0f) {
            hr_score = std::min(1.0f, (hashrate_drop_pct - 20.0f) / 60.0f);
        }
        // Combine with output volume anomaly
        float volume_anomaly = outputs_last_100 > 1000
            ? std::min(1.0f, (static_cast<float>(outputs_last_100) - 1000.0f) / 2000.0f)
            : 0.0f;
        a.network_anomaly_score = std::max(hr_score, volume_anomaly * 0.7f);

        return a;
    }

    // ─── Map score → level ──────────────────────────────────────────────
    static NinaThreatLevel level_from_score(float score) {
        if (score >= 0.9f) return THREAT_CRITICAL;
        if (score >= 0.7f) return THREAT_HIGH;
        if (score >= 0.4f) return THREAT_ELEVATED;
        return THREAT_NORMAL;
    }

    // ─── Map level → mixin ──────────────────────────────────────────────
    static size_t mixin_for_level(NinaThreatLevel level) {
        switch (level) {
            case THREAT_CRITICAL: return NINA_RING_THREAT_CRITICAL_MIXIN;
            case THREAT_HIGH:     return NINA_RING_THREAT_HIGH_MIXIN;
            case THREAT_ELEVATED: return NINA_RING_THREAT_ELEVATED_MIXIN;
            default:              return NINA_RING_THREAT_NORMAL_MIXIN;
        }
    }

    // ─── Hysteresis: prevent oscillation ────────────────────────────────
    NinaThreatLevel apply_hysteresis(uint64_t height, NinaThreatLevel proposed) {
        if (proposed > m_current_level) {
            // Escalation: require sustained higher threat
            m_sustained_higher_count++;
            m_sustained_lower_count = 0;
            if (m_sustained_higher_count >= NINA_RING_THREAT_HYSTERESIS_UP) {
                m_previous_level = m_current_level;
                m_last_level_change_height = height;
                m_sustained_higher_count = 0;
                return proposed;
            }
            return m_current_level;  // Not sustained long enough
        }
        else if (proposed < m_current_level) {
            // De-escalation: require prolonged calm
            m_sustained_lower_count++;
            m_sustained_higher_count = 0;
            if (m_sustained_lower_count >= NINA_RING_THREAT_HYSTERESIS_DOWN) {
                m_previous_level = m_current_level;
                m_last_level_change_height = height;
                m_sustained_lower_count = 0;
                return proposed;
            }
            return m_current_level;  // Not calm long enough
        }
        else {
            // Same level
            m_sustained_higher_count = 0;
            m_sustained_lower_count = 0;
            return m_current_level;
        }
    }
};

} // namespace ninacatcoin_ai
