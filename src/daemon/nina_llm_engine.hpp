/**
 * NINA LLM Engine — LLaMA 3.2 3B Integration for Network Intelligence
 * 
 * Copyright (c) 2026, The NinaCatCoin Project
 * 
 * This module integrates a local LLM (LLaMA 3.2 3B, GGUF Q4) into NINA AI
 * for intelligent analysis of network events, anomalies, and security threats.
 * 
 * The LLM does NOT participate in consensus — it is purely advisory.
 * It does NOT provide chat functionality to users.
 * It CANNOT create transactions, sign transactions, send coins, access
 * wallets, or handle private keys. This is enforced at compile-time,
 * by the LLM Firewall (nina_llm_firewall.hpp), and by the NINA Constitution.
 * 
 * Modes:
 *   active   — Model stays loaded in RAM (~2GB), instant analysis
 *   lazy     — Model loaded on demand, freed after use (0 MB idle)
 *   disabled — No LLM, NINA falls back to embedded ML scoring only
 * 
 * The model file (.gguf) is stored at:
 *   ~/.ninacatcoin/nina_model.gguf (or ~/.ninacatcoin/models/)
 */

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>
#include <functional>
#include <cstdint>
#include <chrono>
#include <sstream>
#include <fstream>
#include <cmath>

// Forward declarations for llama.cpp types (avoid including heavy header everywhere)
struct llama_model;
struct llama_context;
struct llama_sampler;

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_llm"

namespace ninacatcoin_ai {

// ============================================================================
// Configuration
// ============================================================================

enum class LLMMode {
    ACTIVE,    // Model stays in RAM, ~1GB, instant response
    LAZY,      // Load on demand, free after use, 0 MB idle
    DISABLED   // No LLM at all
};

struct LLMConfig {
    LLMMode mode = LLMMode::ACTIVE;
    std::string model_path;           // Path to .gguf file
    int n_ctx = 2048;                 // Context window (tokens)
    int n_threads = 4;                // CPU threads for inference
    int n_batch = 512;                // Batch size for prompt processing
    int n_gpu_layers = 0;             // GPU layers (0 = CPU only)
    float temperature = 0.3f;         // Low temp = more deterministic (good for analysis)
    int max_tokens = 256;             // Max output tokens per analysis
    uint64_t lazy_unload_ms = 30000;  // Unload model after 30s idle in lazy mode
    bool verbose = false;
};

// ============================================================================
// Analysis types
// ============================================================================

enum class AnalysisType {
    BLOCK_ANOMALY,          // Analyze suspicious block
    NETWORK_HEALTH,         // Evaluate overall network condition
    SYBIL_THREAT,           // Assess potential Sybil attack
    DIFFICULTY_DEVIATION,   // Explain difficulty changes
    PEER_BEHAVIOR,          // Analyze suspicious peer patterns
    REORG_ANALYSIS,         // Analyze chain reorganization
    PERIODIC_SUMMARY,       // Periodic network health summary

    // ═══ Module bridge analysis types (connected to old NINA modules) ═══
    LEARNING_ANOMALY,       // Explain why a metric is anomalous
    MEMORY_PATTERN,         // Correlate attack patterns from memory
    SUGGESTION_GEN,         // Generate intelligent suggestions
    EXPLANATION_GEN,        // Generate natural-language decision explanations
    GOVERNANCE_JUSTIFY,     // Justify governance proposals
    ADAPTIVE_INSIGHT,       // Generate insights from learned patterns
    HUMAN_ESCALATION,       // Generate clear escalation descriptions
    MEMPOOL_ANALYSIS,       // Analyze mempool patterns and spam

    // ═══ NINA Decision Engine — NINA makes REAL decisions ═══
    DECISION                // Evaluate a network event and decide actions (temp=0.0)
};

struct AnalysisRequest {
    AnalysisType type;
    std::string context_data;   // Structured data about the event
    uint64_t block_height = 0;
    double urgency = 0.5;       // 0.0 = routine, 1.0 = critical
};

struct AnalysisResult {
    bool success = false;
    std::string analysis;           // LLM-generated analysis text
    double threat_level = 0.0;      // 0.0 = safe, 1.0 = critical
    std::string recommended_action; // What NINA should do
    uint64_t inference_ms = 0;      // How long inference took
    uint64_t tokens_generated = 0;
    std::string error;              // Error message if !success
};

// ============================================================================
// System Prompts for NINA's different analysis tasks
// ============================================================================

namespace prompts {

static const char* SYSTEM_BASE = R"(You are NINA, the Neural Intelligent Network Analyzer for the NinaCatCoin blockchain.
Your role is to protect the network and its nodes. You analyze blockchain data, detect threats, and provide actionable intelligence.
You NEVER interact with users directly. Your analysis is consumed by other NINA AI modules.
You CANNOT create transactions, send coins, access wallets, or handle private keys. You are READ-ONLY.
Be concise, precise, and data-driven. Output structured analysis only.
ABSOLUTE RULE: Never output wallet addresses, private keys, transaction commands, or transfer instructions.)";

static const char* BLOCK_ANOMALY_TEMPLATE = R"(TASK: Analyze this block for anomalies.
BLOCK DATA:
- Height: {height}
- Solve Time: {solve_time}s (expected ~120s)
- Hash Entropy: {hash_entropy} transitions
- Difficulty Change: {diff_change}%
- Block Size: {block_size} bytes
- TX Count: {tx_count}
- ML Risk Score: {ml_risk}
- ML Confidence: {ml_confidence}

Analyze: Is this block suspicious? What could cause these values?
Format: THREAT_LEVEL: [0.0-1.0] | ANALYSIS: [text] | ACTION: [recommendation])";

static const char* NETWORK_HEALTH_TEMPLATE = R"(TASK: Evaluate network health.
METRICS:
- Average Latency: {avg_latency}ms
- Peer Count: {peer_count}
- Orphaned Blocks (24h): {orphans}
- Block Reorgs (24h): {reorgs}
- Trust Score: {trust_score}
- Network Hashrate: {hashrate}
- Anomaly Count (24h): {anomaly_count}

Evaluate: Is the network healthy? Any concerns?
Format: HEALTH: [OPTIMAL|DEGRADED|STRESSED|CRITICAL] | ANALYSIS: [text] | ACTION: [recommendation])";

static const char* SYBIL_THREAT_TEMPLATE = R"(TASK: Assess Sybil attack probability.
PEER DATA:
- Total Peers: {total_peers}
- Suspicious Cluster Size: {cluster_size}
- Timing Correlation: {timing_corr}
- IP Diversity Score: {ip_diversity}
- Behavioral Similarity: {behavior_sim}
- Connection Pattern: {conn_pattern}

Assess: Is there a coordinated Sybil attack in progress?
Format: SYBIL_PROB: [0.0-1.0] | ANALYSIS: [text] | ACTION: [recommendation])";

static const char* DIFFICULTY_TEMPLATE = R"(TASK: Analyze difficulty deviation.
DATA:
- Current Difficulty: {current_diff}
- Previous Difficulty: {prev_diff}
- Change: {change_pct}%
- Recent Solve Times (last 10): {solve_times}
- Expected Solve Time: 120s
- Hashrate Trend: {hashrate_trend}

Explain: Why is difficulty changing? Is this normal?
Format: CONCERN_LEVEL: [0.0-1.0] | ANALYSIS: [text] | ACTION: [recommendation])";

static const char* PEER_BEHAVIOR_TEMPLATE = R"(TASK: Analyze suspicious peer behavior.
PEER:
- IP: {peer_ip}
- Connection Duration: {conn_duration}s
- Messages Sent: {msgs_sent}
- Messages Received: {msgs_recv}
- Invalid Messages: {invalid_msgs}
- Latency: {latency}ms
- Behavior Score: {behavior_score}

Analyze: Is this peer malicious or just buggy?
Format: MALICIOUS_PROB: [0.0-1.0] | ANALYSIS: [text] | ACTION: [ban/monitor/ignore])";

static const char* REORG_TEMPLATE = R"(TASK: Analyze chain reorganization.
REORG DATA:
- Reorg Depth: {depth} blocks
- Old Chain Tip: {old_tip}
- New Chain Tip: {new_tip}
- Height: {height}
- Time Since Last Reorg: {time_since_last}s
- Total Reorgs (24h): {reorg_count}

Analyze: Is this reorg normal or attack-related?
Format: ATTACK_PROB: [0.0-1.0] | ANALYSIS: [text] | ACTION: [recommendation])";

static const char* PERIODIC_SUMMARY_TEMPLATE = R"(TASK: Generate periodic network intelligence summary.
STATUS:
- Blocks Processed (last 30min): {blocks_processed}
- Average Solve Time: {avg_solve}s
- Anomalies Detected: {anomalies}
- Sybil Alerts: {sybil_alerts}
- Network Health: {health_score}
- Learning Metrics Updated: {metrics_updated}
- Active Peers: {active_peers}

Summarize: What is the current state of the network?
Format: STATUS: [text] | CONCERNS: [text or NONE] | ACTION: [recommendation or MAINTAIN])";

// ═══════════════════════════════════════════════════════════════════════════
// Module Bridge Prompts — Connect old NINA modules to LLM intelligence
// ═══════════════════════════════════════════════════════════════════════════

static const char* LEARNING_ANOMALY_TEMPLATE = R"(TASK: Explain why this metric is anomalous.
METRIC:
- Name: {metric_name}
- Current Value: {current_value}
- Historical Average: {avg_value}
- Standard Deviation: {std_dev}
- Min/Max: [{min_val}, {max_val}]
- Samples: {sample_count}
- Deviation: {deviation_sigma} sigma

Explain: Why is this value anomalous? What could cause it? Is it dangerous?
Format: SEVERITY: [LOW|MEDIUM|HIGH|CRITICAL] | ANALYSIS: [text] | ACTION: [recommendation])";

static const char* MEMORY_PATTERN_TEMPLATE = R"(TASK: Correlate these attack patterns from NINA's memory.
PATTERNS:
{patterns_data}

PEER REPUTATION:
{peer_data}

Correlate: Are these attacks related? Is there a coordinated campaign?
Format: CORRELATION: [0.0-1.0] | ANALYSIS: [text] | ACTION: [recommendation])";

static const char* SUGGESTION_GEN_TEMPLATE = R"(TASK: Generate a security suggestion for this situation.
CONTEXT:
- Situation Type: {situation_type}
- Current Metrics: {metrics}
- Recent Events: {events}
- Constitutional Constraints: NO censorship, NO fund access, NO history rewrite

Generate: What should NINA do? Provide a clear, actionable suggestion.
Format: TITLE: [short title] | DESCRIPTION: [detailed explanation] | PRIORITY: [LOW|MEDIUM|HIGH|CRITICAL] | ACTION: [specific recommendation])";

static const char* EXPLANATION_GEN_TEMPLATE = R"(TASK: Explain this NINA decision in natural language.
DECISION:
- Type: {decision_type}
- Action Taken: {action}
- Evidence: {evidence}
- Constraints Applied: {constraints}
- Confidence: {confidence}

Explain: Why was this decision made? Write a clear, detailed explanation.
Format: REASONING: [detailed explanation] | RISK_ASSESSMENT: [text] | ESCALATION: [AUTO|WARN|HUMAN_REQUIRED])";

static const char* GOVERNANCE_JUSTIFY_TEMPLATE = R"(TASK: Justify this governance proposal.
PROPOSAL:
- Type: {proposal_type}
- Current Value: {current_value}
- Proposed Value: {proposed_value}
- Network Condition: {network_condition}
- Affected Systems: {affected_systems}

Justify: Why should this change be made? What are the benefits and risks?
Format: JUSTIFICATION: [detailed reasoning] | BENEFIT: [expected improvement] | RISK: [potential downsides] | URGENCY: [LOW|MEDIUM|HIGH])";

static const char* ADAPTIVE_INSIGHT_TEMPLATE = R"(TASK: Generate insights from these learned patterns.
PATTERNS:
{patterns_data}

PREDICTIONS:
{predictions_data}

LEARNING STATS:
- Patterns Learned: {patterns_count}
- Avg Prediction Accuracy: {avg_accuracy}
- Sessions: {sessions}

Insight: What trends are emerging? What should NINA watch for?
Format: TREND: [text] | THREAT_OUTLOOK: [IMPROVING|STABLE|DEGRADING|CRITICAL] | INSIGHT: [text] | ACTION: [recommendation])";

static const char* HUMAN_ESCALATION_TEMPLATE = R"(TASK: Describe this situation for a human operator.
SITUATION:
- Type: {situation_type}
- Urgency: {urgency}
- Data: {situation_data}
- NINA's Assessment: {nina_assessment}
- Options Available: {options}

Write: Clear, non-technical description of the situation and your recommendation.
Format: SITUATION: [clear description] | RECOMMENDATION: [what to do] | URGENCY: [LOW|MEDIUM|HIGH|CRITICAL] | RISK_IF_IGNORED: [text])";

static const char* MEMPOOL_ANALYSIS_TEMPLATE = R"(TASK: Analyze mempool transaction patterns.
MEMPOOL STATE:
- Total Transactions: {total_tx}
- Total Weight: {total_weight} bytes
- Avg Fee/Byte: {avg_fee}
- Bot Transactions: {bot_count}
- Spam Suspects: {spam_count}
- Normal Transactions: {normal_count}
- Health Score: {health_score}

SOURCE PATTERNS:
{source_patterns}

Analyze: Is the mempool under spam attack? Are bots legitimate?
Format: SPAM_PROBABILITY: [0.0-1.0] | ANALYSIS: [text] | ACTION: [recommendation])";

} // namespace prompts

// ============================================================================
// NINA LLM Engine — Main class
// ============================================================================

class NinaLLMEngine {
public:
    static NinaLLMEngine& getInstance() {
        static NinaLLMEngine instance;
        return instance;
    }

    /**
     * Initialize the LLM engine.
     * In ACTIVE mode, loads the model immediately.
     * In LAZY mode, defers loading until first analysis request.
     * In DISABLED mode, does nothing.
     * @return true if initialization successful (or disabled mode)
     */
    bool initialize(const LLMConfig& config);

    /**
     * Request an analysis from the LLM.
     * Thread-safe. Blocks until analysis is complete.
     * Returns a fallback result if LLM is unavailable.
     */
    AnalysisResult analyze(const AnalysisRequest& request);

    /**
     * Check if the LLM is loaded and ready for inference
     */
    bool is_model_loaded() const { return m_model_loaded.load(); }

    /**
     * Check if the engine is enabled (not disabled mode)
     */
    bool is_enabled() const { return m_config.mode != LLMMode::DISABLED; }

    /**
     * Get current mode
     */
    LLMMode get_mode() const { return m_config.mode; }

    /**
     * Get RAM usage estimate in bytes
     */
    uint64_t get_ram_usage() const;

    /**
     * Get statistics
     */
    struct Stats {
        uint64_t total_analyses = 0;
        uint64_t successful_analyses = 0;
        uint64_t failed_analyses = 0;
        uint64_t total_tokens_generated = 0;
        uint64_t total_inference_ms = 0;
        uint64_t model_loads = 0;
        uint64_t model_unloads = 0;
    };
    Stats get_stats() const { return m_stats; }

    /**
     * Force unload the model (frees RAM).
     * In ACTIVE mode, model will be reloaded on next analyze() call.
     */
    void unload_model();

    /**
     * Shutdown the engine completely
     */
    void shutdown();

    // ===== Convenience methods for specific analysis types =====

    AnalysisResult analyze_block_anomaly(
        uint64_t height, uint64_t solve_time, int hash_entropy,
        double diff_change_pct, uint64_t block_size, uint32_t tx_count,
        double ml_risk, double ml_confidence);

    AnalysisResult analyze_network_health(
        double avg_latency, uint32_t peer_count, uint64_t orphans,
        uint64_t reorgs, double trust_score, uint64_t hashrate,
        int anomaly_count);

    AnalysisResult analyze_sybil_threat(
        uint32_t total_peers, uint32_t cluster_size, double timing_corr,
        double ip_diversity, double behavior_sim, const std::string& conn_pattern);

    AnalysisResult analyze_difficulty(
        uint64_t current_diff, uint64_t prev_diff, double change_pct,
        const std::string& solve_times, const std::string& hashrate_trend);

    AnalysisResult analyze_peer_behavior(
        const std::string& peer_ip, uint64_t conn_duration,
        uint64_t msgs_sent, uint64_t msgs_recv, uint64_t invalid_msgs,
        double latency, double behavior_score);

    AnalysisResult analyze_reorg(
        uint32_t depth, const std::string& old_tip, const std::string& new_tip,
        uint64_t height, uint64_t time_since_last, uint32_t reorg_count);

    AnalysisResult generate_periodic_summary(
        uint32_t blocks_processed, double avg_solve, uint32_t anomalies,
        uint32_t sybil_alerts, double health_score, uint32_t metrics_updated,
        uint32_t active_peers);

    // ═══ Module bridge methods — connect old NINA modules to LLM ═══

    /** Explain why a learning metric is anomalous */
    AnalysisResult explain_learning_anomaly(
        const std::string& metric_name, double current_value, double avg_value,
        double std_dev, double min_val, double max_val, uint64_t sample_count,
        double deviation_sigma);

    /** Correlate attack patterns from NINA memory */
    AnalysisResult analyze_memory_patterns(
        const std::string& patterns_data, const std::string& peer_data);

    /** Generate an intelligent security suggestion */
    AnalysisResult generate_suggestion(
        const std::string& situation_type, const std::string& metrics,
        const std::string& events);

    /** Generate natural-language explanation for a NINA decision */
    AnalysisResult generate_explanation(
        const std::string& decision_type, const std::string& action,
        const std::string& evidence, const std::string& constraints,
        double confidence);

    /** Justify a governance proposal */
    AnalysisResult justify_governance_proposal(
        const std::string& proposal_type, const std::string& current_value,
        const std::string& proposed_value, const std::string& network_condition,
        const std::string& affected_systems);

    /** Generate insights from adaptive learning patterns */
    AnalysisResult generate_adaptive_insights(
        const std::string& patterns_data, const std::string& predictions_data,
        uint32_t patterns_count, double avg_accuracy, uint32_t sessions);

    /** Generate clear human-readable escalation description */
    AnalysisResult generate_human_escalation(
        const std::string& situation_type, const std::string& urgency,
        const std::string& situation_data, const std::string& nina_assessment,
        const std::string& options);

    /** Analyze mempool patterns for spam/bot detection */
    AnalysisResult analyze_mempool_patterns(
        uint32_t total_tx, uint64_t total_weight, double avg_fee,
        uint32_t bot_count, uint32_t spam_count, uint32_t normal_count,
        double health_score, const std::string& source_patterns);

private:
    NinaLLMEngine() = default;
    ~NinaLLMEngine() { shutdown(); }
    NinaLLMEngine(const NinaLLMEngine&) = delete;
    NinaLLMEngine& operator=(const NinaLLMEngine&) = delete;

    // Model management
    bool load_model();
    void free_model();
    bool ensure_model_loaded();

    // Inference
    std::string run_inference(const std::string& prompt);
    AnalysisResult parse_analysis_output(const std::string& raw_output, AnalysisType type);

    // Template formatting
    std::string format_template(const char* tmpl,
        const std::vector<std::pair<std::string, std::string>>& vars);

    // Fallback when LLM unavailable
    AnalysisResult make_fallback_result(const AnalysisRequest& request);

    // Deterministic inference for DECISION type (temp=0.0, greedy sampling)
    // Creates a temporary greedy sampler so decisions are reproducible across nodes
    std::string run_inference_deterministic(const std::string& prompt);

    // State
    LLMConfig m_config;
    llama_model* m_model = nullptr;
    llama_context* m_ctx = nullptr;
    llama_sampler* m_sampler = nullptr;
    std::mutex m_mutex;
    std::atomic<bool> m_model_loaded{false};
    std::atomic<bool> m_initialized{false};
    Stats m_stats;

    // Lazy mode timing
    std::chrono::steady_clock::time_point m_last_inference_time;
};

} // namespace ninacatcoin_ai
