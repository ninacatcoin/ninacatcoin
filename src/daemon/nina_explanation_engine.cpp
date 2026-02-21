/**
 * NINA Explanation Engine Implementation - TIER 4
 * Every decision is justified and traceable
 */

#include "nina_explanation_engine.hpp"
#include "misc_log_ex.h"
#include <sstream>
#include <iomanip>

namespace ninacatcoin_ai {

DecisionExplanation NInaExplanationEngine::explain_block_validation(
    int block_height,
    bool is_valid,
    std::vector<std::string> validation_rules,
    double anomaly_score)
{
    DecisionExplanation exp;
    exp.decision_id = "BLOCK_VALIDATE_" + std::to_string(block_height);
    exp.action_type = "BLOCK_VALIDATION";
    exp.timestamp = std::time(nullptr);

    if (is_valid) {
        exp.reasoning = "Block passed all validation rules and is safe for relay";
    } else {
        exp.reasoning = "Block rejected due to validation failure or anomaly detection";
    }

    for (const auto& rule : validation_rules) {
        exp.evidence.push_back("✓ Checked: " + rule);
    }

    exp.evidence.push_back("Anomaly Score: " + std::to_string(anomaly_score));
    exp.constraints.push_back("CONSENSUS_BINDING: Cannot alter consensus");
    exp.constraints.push_back("NO_CENSORSHIP: Cannot arbitrarily reject");
    exp.confidence_score = 1.0 - (anomaly_score * 0.1);
    exp.escalation_level = anomaly_score > 0.8 ? "HUMAN_REQUIRED" : "AUTO";

    MINFO("[EXPLANATION] Block " << block_height << " validation: " 
          << (is_valid ? "VALID" : "INVALID") 
          << " (confidence: " << std::fixed << std::setprecision(2) 
          << exp.confidence_score << ")");

    return exp;
}

DecisionExplanation NInaExplanationEngine::explain_checkpoint_validation(
    int checkpoint_height,
    bool sources_verified,
    int seeds_confirmed,
    bool potential_attack)
{
    DecisionExplanation exp;
    exp.decision_id = "CHECKPOINT_" + std::to_string(checkpoint_height);
    exp.action_type = "CHECKPOINT_VALIDATION";
    exp.timestamp = std::time(nullptr);

    std::ostringstream reasoning;
    reasoning << "Checkpoint height " << checkpoint_height << " validation: ";

    if (sources_verified && seeds_confirmed >= 3 && !potential_attack) {
        reasoning << "VERIFIED SAFE - " << seeds_confirmed << "/4 seed nodes confirmed";
        exp.confidence_score = 0.99;
        exp.escalation_level = "AUTO";
    } else if (potential_attack) {
        reasoning << "POTENTIAL ATTACK DETECTED - checkpoint sources compromised";
        exp.confidence_score = 0.95;
        exp.escalation_level = "HUMAN_REQUIRED";
    } else {
        reasoning << "UNVERIFIED - waiting for seed confirmation";
        exp.confidence_score = 0.5;
        exp.escalation_level = "WARN";
    }

    exp.reasoning = reasoning.str();
    exp.evidence.push_back("Sources Verified: " + std::string(sources_verified ? "YES" : "NO"));
    exp.evidence.push_back("Seeds Confirmed: " + std::to_string(seeds_confirmed) + "/4");
    exp.evidence.push_back("Attack Detected: " + std::string(potential_attack ? "YES" : "NO"));

    exp.constraints.push_back("NETWORK_TRANSPARENCY: Must verify through multiple sources");
    exp.constraints.push_back("NO_CENSORSHIP: Cannot reject without evidence");

    return exp;
}

DecisionExplanation NInaExplanationEngine::explain_transaction_filtering(
    const std::string& tx_hash,
    bool should_relay,
    std::vector<std::string> anomaly_flags,
    double suspicion_score)
{
    DecisionExplanation exp;
    exp.decision_id = "TX_FILTER_" + tx_hash.substr(0, 8);
    exp.action_type = "TRANSACTION_FILTERING";
    exp.timestamp = std::time(nullptr);

    if (should_relay) {
        exp.reasoning = "Transaction is valid and should be relayed to network";
    } else {
        exp.reasoning = "Transaction contains anomalies and is held for review";
    }

    for (const auto& flag : anomaly_flags) {
        exp.evidence.push_back("⚠ Anomaly detected: " + flag);
    }

    exp.evidence.push_back("Suspicion Score: " + std::to_string(suspicion_score));
    exp.constraints.push_back("NO_CENSORSHIP: Cannot silently drop valid transactions");
    exp.constraints.push_back("USER_AUTONOMY: User retains transaction rights");

    exp.confidence_score = 1.0 - (suspicion_score * 0.2);
    exp.escalation_level = suspicion_score > 0.7 ? "WARN" : "AUTO";

    return exp;
}

DecisionExplanation NInaExplanationEngine::explain_peer_reputation(
    const std::string& peer_id,
    double reputation_score,
    std::vector<std::string> behaviors_observed,
    bool is_trustworthy)
{
    DecisionExplanation exp;
    exp.decision_id = "PEER_REP_" + peer_id.substr(0, 8);
    exp.action_type = "PEER_EVALUATION";
    exp.timestamp = std::time(nullptr);

    std::ostringstream reasoning;
    reasoning << "Peer " << peer_id.substr(0, 8) << " reputation: ";
    reasoning << std::fixed << std::setprecision(2) << (reputation_score * 100) << "%";

    if (is_trustworthy) {
        reasoning << " - TRUSTED";
    } else {
        reasoning << " - MONITORING";
    }

    exp.reasoning = reasoning.str();

    for (const auto& behavior : behaviors_observed) {
        exp.evidence.push_back("Observed: " + behavior);
    }

    exp.constraints.push_back("NETWORK_TRANSPARENCY: Peer reputation is visible");
    exp.constraints.push_back("USER_AUTONOMY: Node operators can choose peers");

    exp.confidence_score = reputation_score;
    exp.escalation_level = reputation_score < 0.2 ? "WARN" : "AUTO";

    return exp;
}

std::string NInaExplanationEngine::format_explanation(const DecisionExplanation& exp)
{
    std::ostringstream oss;

    oss << "\n╔════════════════════════════════════════════════════════════╗\n";
    oss << "║ NINA DECISION EXPLANATION - " << exp.action_type << "\n";
    oss << "╚════════════════════════════════════════════════════════════╝\n\n";

    oss << "📋 Decision ID: " << exp.decision_id << "\n";
    oss << "⏱️  Timestamp: " << std::ctime(&exp.timestamp);
    oss << "📊 Confidence: " << std::fixed << std::setprecision(1) 
        << (exp.confidence_score * 100) << "%\n";
    oss << "🚨 Escalation: " << exp.escalation_level << "\n\n";

    oss << "🧠 REASONING:\n";
    oss << "   " << exp.reasoning << "\n\n";

    oss << "📊 EVIDENCE:\n";
    for (const auto& ev : exp.evidence) {
        oss << "   • " << ev << "\n";
    }

    oss << "\n🔒 CONSTITUTIONAL CONSTRAINTS APPLIED:\n";
    for (const auto& constraint : exp.constraints) {
        oss << "   ✓ " << constraint << "\n";
    }

    oss << "\n╚════════════════════════════════════════════════════════════╝\n";

    return oss.str();
}

void NInaExplanationEngine::log_decision(const DecisionExplanation& exp)
{
    MINFO("[DECISION] " << exp.decision_id);
    MINFO("[DECISION] Action: " << exp.action_type);
    MINFO("[DECISION] Reasoning: " << exp.reasoning);
    MINFO("[DECISION] Confidence: " << std::fixed << std::setprecision(2) 
          << exp.confidence_score);
    MINFO("[DECISION] Escalation: " << exp.escalation_level);
}

std::vector<DecisionExplanation> NInaExplanationEngine::get_decision_history(
    const std::string& action_type,
    int limit)
{
    // This would fetch from persistent storage
    MINFO("[AUDIT] Decision history requested for: " << action_type);
    return std::vector<DecisionExplanation>();
}

} // namespace ninacatcoin_ai
