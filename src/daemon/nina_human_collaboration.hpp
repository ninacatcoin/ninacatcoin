/**
 * NINA Human-AI Collaboration Engine - TIER 6
 * Clear interfaces for human decision-making
 * Escalates critical decisions to humans
 */

#pragma once

#include <string>
#include <vector>
#include <functional>
#include <ctime>

namespace ninacatcoin_ai {

/**
 * Escalation Request - when NINA needs human input
 */
struct EscalationRequest {
    std::string escalation_id;
    std::string situation_description;
    std::vector<std::string> options;     // What could be done
    std::string nina_recommendation;      // What NINA thinks is best
    std::string urgency_level;            // "LOW", "MEDIUM", "HIGH", "CRITICAL"
    std::time_t created_at;
    bool requires_human_approval;
    std::string human_decision;           // Added by human later
    std::time_t decision_timestamp;
};

/**
 * Human Input Handler - receives and processes human decisions
 */
struct HumanInputHandler {
    std::function<bool(const EscalationRequest&)> callback;
};

class NINAHumanCollaboration {
public:
    /**
     * Escalate decision to human operators
     * Blocks until human responds
     */
    static bool escalate_decision(
        const std::string& situation,
        std::vector<std::string> available_options,
        const std::string& nina_recommendation,
        const std::string& urgency
    );

    /**
     * Register a human decision callback
     */
    static void register_human_handler(const HumanInputHandler& handler);

    /**
     * Check if a decision requires human approval
     */
    static bool requires_human_approval(const std::string& decision_type);

    /**
     * Propose improvement to human operators
     * Non-blocking, just informational
     */
    static void suggest_improvement(
        const std::string& improvement_description,
        double expected_benefit,
        const std::string& justification
    );

    /**
     * Request human feedback on NINA's reasoning
     */
    static void request_feedback(
        const std::string& decision_id,
        const std::string& explanation,
        std::vector<std::string> confidence_factors
    );

    /**
     * Report to human dashboard
     */
    static std::string generate_status_report();

    /**
     * Get escalation history
     */
    static std::vector<EscalationRequest> get_escalation_history(int limit = 50);

    /**
     * Log human intervention
     */
    static void log_human_decision(
        const std::string& escalation_id,
        const std::string& human_decision,
        const std::string& human_reasoning
    );

    /**
     * Verify escalation response received
     */
    static bool wait_for_human_response(
        const std::string& escalation_id,
        int timeout_seconds = 300
    );
};

} // namespace ninacatcoin_ai
