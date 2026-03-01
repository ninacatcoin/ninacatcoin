/**
 * NINA LLM Module Bridge — Connects old NINA modules to LLaMA intelligence
 * 
 * Copyright (c) 2026, The NinaCatCoin Project
 * 
 * This bridge provides the integration layer between NINA's existing modules
 * (learning, memory, suggestions, explanations, governance, adaptive learning,
 * human collaboration, smart mempool) and the new LLaMA 3.2 3B LLM Engine.
 * 
 * Design principles:
 *   - Non-blocking: If LLM is disabled/unavailable, modules work as before
 *   - Safe: All LLM calls go through the firewall (nina_llm_firewall.hpp)
 *   - Additive: LLM enhances existing modules, never replaces their logic
 *   - Async-safe: Bridge methods can be called from any thread
 * 
 * Each module gets an "enhance_*" method that takes the module's existing
 * output and returns an LLM-enriched version.
 */

#pragma once

#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include "nina_llm_engine.hpp"
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_llm_bridge"

namespace ninacatcoin_ai {

class NinaLLMBridge {
public:
    static NinaLLMBridge& getInstance() {
        static NinaLLMBridge instance;
        return instance;
    }

    /**
     * Check if LLM enhancement is available.
     * Modules should check this before calling enhance methods
     * to avoid unnecessary overhead.
     */
    bool is_available() const {
        auto& llm = NinaLLMEngine::getInstance();
        return llm.is_enabled() && llm.is_model_loaded();
    }

    // ═══════════════════════════════════════════════════════════════════
    // 1. LEARNING MODULE BRIDGE
    // ═══════════════════════════════════════════════════════════════════

    /**
     * Enhance anomaly detection with LLM explanation.
     * Called when NINALearningModule::isAnomaly() returns true.
     * Returns a rich explanation of why the metric is anomalous.
     */
    std::string enhance_anomaly_explanation(
        const std::string& metric_name,
        double current_value, double avg_value, double std_dev,
        double min_val, double max_val, uint64_t sample_count)
    {
        if (!is_available()) return "";

        double deviation = (std_dev > 0.0) ? std::abs(current_value - avg_value) / std_dev : 0.0;

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.explain_learning_anomaly(
                metric_name, current_value, avg_value, std_dev,
                min_val, max_val, sample_count, deviation);
            if (result.success) {
                MINFO("[NINA-LLM-BRIDGE] Learning anomaly explained: " << metric_name
                      << " (" << result.inference_ms << "ms)");
                return result.analysis;
            }
        } catch (...) {}
        return "";
    }

    /**
     * Enhance the learning report with LLM summary.
     * Called by NINALearningModule::generateLearningReport().
     */
    std::string enhance_learning_report(const std::string& base_report, int anomaly_count)
    {
        if (!is_available() || anomaly_count == 0) return base_report;

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.generate_suggestion(
                "LEARNING_REPORT",
                base_report,
                "anomaly_count=" + std::to_string(anomaly_count));
            if (result.success) {
                return base_report + "\n\n[NINA LLM Analysis]\n" + result.analysis;
            }
        } catch (...) {}
        return base_report;
    }

    // ═══════════════════════════════════════════════════════════════════
    // 2. MEMORY SYSTEM BRIDGE
    // ═══════════════════════════════════════════════════════════════════

    /**
     * Correlate attack patterns using LLM.
     * Called by NINAMemorySystem::analyzePatterns().
     */
    struct PatternCorrelation {
        double correlation_score = 0.0;
        std::string analysis;
        std::string recommended_action;
    };

    PatternCorrelation correlate_attack_patterns(
        const std::string& patterns_summary,
        const std::string& peer_reputation_summary)
    {
        PatternCorrelation pc;
        if (!is_available()) return pc;

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.analyze_memory_patterns(patterns_summary, peer_reputation_summary);
            if (result.success) {
                pc.correlation_score = result.threat_level;
                pc.analysis = result.analysis;
                pc.recommended_action = result.recommended_action;
                MINFO("[NINA-LLM-BRIDGE] Pattern correlation: " << pc.correlation_score
                      << " (" << result.inference_ms << "ms)");
            }
        } catch (...) {}
        return pc;
    }

    // ═══════════════════════════════════════════════════════════════════
    // 3. SUGGESTION ENGINE BRIDGE
    // ═══════════════════════════════════════════════════════════════════

    /**
     * Generate an LLM-powered suggestion.
     * Returns fields suitable for Suggestion struct.
     */
    struct LLMSuggestion {
        std::string title;
        std::string description;
        std::string priority;  // "LOW", "MEDIUM", "HIGH", "CRITICAL"
        std::string action;
        bool valid = false;
    };

    LLMSuggestion generate_smart_suggestion(
        const std::string& situation_type,
        const std::string& current_metrics,
        const std::string& recent_events)
    {
        LLMSuggestion s;
        if (!is_available()) return s;

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.generate_suggestion(situation_type, current_metrics, recent_events);
            if (result.success) {
                // Parse structured fields from LLM output
                s.title = extract_field(result.analysis, "TITLE");
                s.description = extract_field(result.analysis, "DESCRIPTION");
                s.priority = extract_field(result.analysis, "PRIORITY");
                s.action = result.recommended_action;
                s.valid = !s.title.empty();
                MINFO("[NINA-LLM-BRIDGE] Suggestion generated: " << s.title
                      << " (" << result.inference_ms << "ms)");
            }
        } catch (...) {}
        return s;
    }

    // ═══════════════════════════════════════════════════════════════════
    // 4. EXPLANATION ENGINE BRIDGE
    // ═══════════════════════════════════════════════════════════════════

    /**
     * Generate natural-language explanation for a NINA decision.
     * Replaces the templated strings in NInaExplanationEngine.
     */
    struct LLMExplanation {
        std::string reasoning;
        std::string risk_assessment;
        std::string escalation_level;  // "AUTO", "WARN", "HUMAN_REQUIRED"
        bool valid = false;
    };

    LLMExplanation explain_decision(
        const std::string& decision_type,
        const std::string& action_taken,
        const std::vector<std::string>& evidence,
        const std::vector<std::string>& constraints,
        double confidence)
    {
        LLMExplanation exp;
        if (!is_available()) return exp;

        // Serialize vectors for the prompt
        std::string ev_str = join_strings(evidence, "; ");
        std::string con_str = join_strings(constraints, "; ");

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.generate_explanation(
                decision_type, action_taken, ev_str, con_str, confidence);
            if (result.success) {
                exp.reasoning = extract_field(result.analysis, "REASONING");
                if (exp.reasoning.empty()) exp.reasoning = result.analysis;
                exp.risk_assessment = extract_field(result.analysis, "RISK_ASSESSMENT");
                exp.escalation_level = extract_field(result.analysis, "ESCALATION");
                if (exp.escalation_level.empty()) exp.escalation_level = "AUTO";
                exp.valid = true;
                MINFO("[NINA-LLM-BRIDGE] Decision explained: " << decision_type
                      << " (" << result.inference_ms << "ms)");
            }
        } catch (...) {}
        return exp;
    }

    // ═══════════════════════════════════════════════════════════════════
    // 5. GOVERNANCE ENGINE BRIDGE
    // ═══════════════════════════════════════════════════════════════════

    /**
     * Justify a governance proposal with LLM reasoning.
     */
    struct LLMJustification {
        std::string justification;
        std::string benefit;
        std::string risk;
        std::string urgency;
        bool valid = false;
    };

    LLMJustification justify_proposal(
        const std::string& proposal_type,
        const std::string& current_val,
        const std::string& proposed_val,
        const std::string& network_condition,
        const std::vector<std::string>& affected_systems)
    {
        LLMJustification j;
        if (!is_available()) return j;

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.justify_governance_proposal(
                proposal_type, current_val, proposed_val,
                network_condition, join_strings(affected_systems, ", "));
            if (result.success) {
                j.justification = extract_field(result.analysis, "JUSTIFICATION");
                if (j.justification.empty()) j.justification = result.analysis;
                j.benefit = extract_field(result.analysis, "BENEFIT");
                j.risk = extract_field(result.analysis, "RISK");
                j.urgency = extract_field(result.analysis, "URGENCY");
                j.valid = true;
                MINFO("[NINA-LLM-BRIDGE] Proposal justified: " << proposal_type
                      << " (" << result.inference_ms << "ms)");
            }
        } catch (...) {}
        return j;
    }

    // ═══════════════════════════════════════════════════════════════════
    // 6. ADAPTIVE LEARNING BRIDGE
    // ═══════════════════════════════════════════════════════════════════

    /**
     * Generate insights from learned patterns.
     */
    struct LLMInsight {
        std::string trend;
        std::string threat_outlook;  // "IMPROVING", "STABLE", "DEGRADING", "CRITICAL"
        std::string insight;
        std::string action;
        bool valid = false;
    };

    LLMInsight generate_insights(
        const std::string& patterns_summary,
        const std::string& predictions_summary,
        uint32_t patterns_count, double avg_accuracy, uint32_t sessions)
    {
        LLMInsight ins;
        if (!is_available()) return ins;

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.generate_adaptive_insights(
                patterns_summary, predictions_summary,
                patterns_count, avg_accuracy, sessions);
            if (result.success) {
                ins.trend = extract_field(result.analysis, "TREND");
                ins.threat_outlook = extract_field(result.analysis, "THREAT_OUTLOOK");
                ins.insight = extract_field(result.analysis, "INSIGHT");
                if (ins.insight.empty()) ins.insight = result.analysis;
                ins.action = result.recommended_action;
                ins.valid = true;
                MINFO("[NINA-LLM-BRIDGE] Adaptive insight generated"
                      << " (" << result.inference_ms << "ms)");
            }
        } catch (...) {}
        return ins;
    }

    // ═══════════════════════════════════════════════════════════════════
    // 7. HUMAN COLLABORATION BRIDGE
    // ═══════════════════════════════════════════════════════════════════

    /**
     * Generate a clear, human-readable escalation description.
     */
    struct LLMEscalation {
        std::string situation_description;
        std::string recommendation;
        std::string urgency;
        std::string risk_if_ignored;
        bool valid = false;
    };

    LLMEscalation enhance_escalation(
        const std::string& situation_type,
        const std::string& urgency,
        const std::string& raw_data,
        const std::string& nina_assessment,
        const std::vector<std::string>& options)
    {
        LLMEscalation esc;
        if (!is_available()) return esc;

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.generate_human_escalation(
                situation_type, urgency, raw_data,
                nina_assessment, join_strings(options, ", "));
            if (result.success) {
                esc.situation_description = extract_field(result.analysis, "SITUATION");
                if (esc.situation_description.empty()) esc.situation_description = result.analysis;
                esc.recommendation = extract_field(result.analysis, "RECOMMENDATION");
                esc.urgency = extract_field(result.analysis, "URGENCY");
                esc.risk_if_ignored = extract_field(result.analysis, "RISK_IF_IGNORED");
                esc.valid = true;
                MINFO("[NINA-LLM-BRIDGE] Escalation enhanced: " << situation_type
                      << " (" << result.inference_ms << "ms)");
            }
        } catch (...) {}
        return esc;
    }

    /**
     * Generate a status report for operators using LLM.
     */
    std::string generate_status_report(
        const std::string& raw_metrics,
        const std::string& recent_decisions,
        const std::string& active_alerts)
    {
        if (!is_available()) return "";

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.generate_suggestion(
                "STATUS_REPORT",
                "Metrics: " + raw_metrics + "\nDecisions: " + recent_decisions,
                "Alerts: " + active_alerts);
            if (result.success) {
                return result.analysis;
            }
        } catch (...) {}
        return "";
    }

    // ═══════════════════════════════════════════════════════════════════
    // 8. SMART MEMPOOL BRIDGE
    // ═══════════════════════════════════════════════════════════════════

    /**
     * Analyze mempool health with LLM intelligence.
     */
    struct MempoolInsight {
        double spam_probability = 0.0;
        std::string analysis;
        std::string recommended_action;
        bool valid = false;
    };

    MempoolInsight analyze_mempool(
        uint32_t total_tx, uint64_t total_weight, double avg_fee,
        uint32_t bot_count, uint32_t spam_count, uint32_t normal_count,
        double health_score, const std::string& source_patterns)
    {
        MempoolInsight mi;
        if (!is_available()) return mi;

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.analyze_mempool_patterns(
                total_tx, total_weight, avg_fee,
                bot_count, spam_count, normal_count,
                health_score, source_patterns);
            if (result.success) {
                mi.spam_probability = result.threat_level;
                mi.analysis = result.analysis;
                mi.recommended_action = result.recommended_action;
                mi.valid = true;
                MINFO("[NINA-LLM-BRIDGE] Mempool analyzed: spam_prob=" << mi.spam_probability
                      << " (" << result.inference_ms << "ms)");
            }
        } catch (...) {}
        return mi;
    }

    /**
     * Enhance the mempool report with LLM analysis.
     */
    std::string enhance_mempool_report(const std::string& base_report,
                                        double health_score)
    {
        if (!is_available() || health_score > 0.8) return base_report;

        try {
            auto& llm = NinaLLMEngine::getInstance();
            auto result = llm.generate_suggestion(
                "MEMPOOL_HEALTH",
                base_report,
                "health_score=" + std::to_string(health_score));
            if (result.success) {
                return base_report + "\n\n[NINA LLM Analysis]\n" + result.analysis;
            }
        } catch (...) {}
        return base_report;
    }

private:
    NinaLLMBridge() = default;
    NinaLLMBridge(const NinaLLMBridge&) = delete;
    NinaLLMBridge& operator=(const NinaLLMBridge&) = delete;

    // Helper: extract "FIELD: value" from LLM output
    static std::string extract_field(const std::string& text, const std::string& field) {
        size_t pos = text.find(field + ":");
        if (pos == std::string::npos) return "";
        pos += field.size() + 1;
        while (pos < text.size() && text[pos] == ' ') pos++;
        size_t end = text.find('|', pos);
        if (end == std::string::npos) end = text.size();
        std::string val = text.substr(pos, end - pos);
        while (!val.empty() && (val.back() == ' ' || val.back() == '\n')) val.pop_back();
        return val;
    }

    // Helper: join vector of strings
    static std::string join_strings(const std::vector<std::string>& v, const std::string& sep) {
        std::string result;
        for (size_t i = 0; i < v.size(); i++) {
            if (i > 0) result += sep;
            result += v[i];
        }
        return result;
    }
};

} // namespace ninacatcoin_ai
