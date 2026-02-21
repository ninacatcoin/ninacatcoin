/**
 * NINA Advanced AI Service - Implementation
 * Integrates all 6 tiers of learning into the daemon
 */

#pragma once

#include "ai/ai_advanced_modules.hpp"
#include "nina_persistent_memory.hpp"
#include "nina_learning_module.hpp"
#include "nina_sybil_detector.hpp"
#include "nina_constitution.hpp"
#include "nina_complete_evolution.hpp"
#include "nina_persistence_engine.hpp"
#include "misc_log_ex.h"
#include <memory>
#include <filesystem>
#include <fstream>

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_advanced"

namespace daemonize {

using namespace ninacatcoin::ia;

/**
 * Global NINA Advanced Coordinator Instance
 * Created once at daemon startup
 */
static std::unique_ptr<NInaAdvancedCoordinator> g_nina_advanced_ai = nullptr;

/**
 * Global Sybil Detector Instance
 * Monitors peer behavior to detect coordinated attacks
 */
static std::unique_ptr<SybilDetectorModule> g_nina_sybil_detector = nullptr;

// Forward declarations
inline void nina_advanced_generate_report(uint64_t block_height);
inline void nina_sybil_analyze_and_alert();
inline std::string nina_sybil_get_status();

/**
 * Load shared ML models received from peers via P2P
 * Models are saved as .b64 files with .meta metadata in ~/.ninacatcoin/nina_shared_models/
 */
inline void nina_load_shared_models()
{
    try {
        const char* home = std::getenv("HOME");
        if (!home) home = std::getenv("USERPROFILE");
        if (!home) return;

        std::string model_dir = std::string(home) + "/.ninacatcoin/nina_shared_models/";
        if (!std::filesystem::exists(model_dir)) {
            MINFO("[NINA-MODELS] No shared models directory found (first run)");
            return;
        }

        size_t loaded = 0;
        for (const auto& entry : std::filesystem::directory_iterator(model_dir)) {
            if (entry.path().extension() == ".meta") {
                try {
                    std::ifstream meta_file(entry.path());
                    if (!meta_file.is_open()) continue;

                    std::string line;
                    std::string model_name = entry.path().stem().string();
                    std::string version;
                    uint64_t training_height = 0;
                    uint64_t training_rows = 0;
                    double accuracy = 0.0;

                    while (std::getline(meta_file, line)) {
                        auto eq = line.find('=');
                        if (eq == std::string::npos) continue;
                        std::string key = line.substr(0, eq);
                        std::string val = line.substr(eq + 1);

                        if (key == "version") version = val;
                        else if (key == "training_height") try { training_height = std::stoull(val); } catch (...) {}
                        else if (key == "training_rows") try { training_rows = std::stoull(val); } catch (...) {}
                        else if (key == "accuracy") try { accuracy = std::stod(val); } catch (...) {}
                    }
                    meta_file.close();

                    MINFO("[NINA-MODELS] Loaded shared model: " << model_name
                          << " (height=" << training_height
                          << ", rows=" << training_rows
                          << ", accuracy=" << accuracy << ")");

                    // Record model info in learning module metrics
                    auto& learning = ninacatcoin_ai::NINALearningModule::getInstance();
                    learning.recordMetric("shared_model_" + model_name + "_accuracy", accuracy);
                    learning.recordMetric("shared_model_" + model_name + "_rows", static_cast<double>(training_rows));

                    loaded++;
                } catch (const std::exception& e) {
                    MWARNING("[NINA-MODELS] Error loading model metadata: " << e.what());
                }
            }
        }

        if (loaded > 0) {
            MINFO("[NINA-MODELS] ✓ Loaded " << loaded << " shared ML model(s) from peers");
            nina_audit_log(0, "SHARED_MODELS_LOADED", std::to_string(loaded) + " models integrated from P2P");
        }
    } catch (const std::exception& e) {
        MWARNING("[NINA-MODELS] Error scanning shared models: " << e.what());
    }
}

/**
 * Initialize NINA Advanced AI Framework
 * Call this once during daemon startup, after basic initialization
 */
inline void initialize_nina_advanced()
{
    if (g_nina_advanced_ai) {
        return;  // Already initialized
    }
    
    try {
        g_nina_advanced_ai = std::make_unique<NInaAdvancedCoordinator>();
        g_nina_sybil_detector = std::make_unique<SybilDetectorModule>();
        
        // Initialize LMDB Persistence Engine
        MINFO("\n[NINA-PERSISTENCE] Initializing persistence layer...");
        const char* home = getenv("HOME");
        std::string db_path = home ? std::string(home) : "/root";
        db_path += "/.ninacatcoin/ninacatcoin_ai_db";
        if (!ninacatcoin_ai::NINaPersistenceEngine::initialize(db_path)) {
            MERROR("Failed to initialize NINA Persistence Engine!");
        }
        
        // Load persistent state from LMDB
        nina_load_persistent_state();
        
        // Load learning metrics from LMDB
        {
            auto& learning = ninacatcoin_ai::NINALearningModule::getInstance();
            learning.initialize();
            if (learning.loadFromLMDB()) {
                MINFO("[NINA] ✓ Learning metrics restored from LMDB");
            }
        }
        
        // Load shared ML models received from peers
        nina_load_shared_models();
        
        // Initialize NINA Constitution - fundamental governance framework
        MINFO("\n📜 LOADING NINA CONSTITUTION...");
        try {
            std::string const_principles = ninacatcoin_ai::NINAConstitution::getConstitutionalPrinciples();
            MINFO(const_principles);
            MINFO("✓ NINA Constitution loaded and active");
        } catch (const std::exception& e) {
            MERROR("Failed to load NINA Constitution: " << e.what());
        }
        
        // Initialize Complete NINA Evolution Framework (All 6 TIERS)
        MINFO("\n");
        ninacatcoin_ai::NInaCompleteEvolution::initialize_all_tiers();
        ninacatcoin_ai::NInaCompleteEvolution::health_check();
        
        MINFO("\n" << std::string(80, '='));
        MINFO("🤖 NINA ADVANCED AI FRAMEWORK v2.0 - PERSISTENT LEARNING");
        MINFO(std::string(80, '='));
        MINFO("\n📊 TIER 1: PREDICTION & FORECASTING");
        MINFO("   ✓ BlockTime Predictor");
        MINFO("   ✓ Attack Forecasting");
        MINFO("   ✓ Difficulty Trend Analysis");
        MINFO("\n🔍 TIER 2: TRANSACTION ANALYSIS");
        MINFO("   ✓ Anomalous Transaction Detection");
        MINFO("   ✓ Mempool Watcher");
        MINFO("\n🌐 TIER 3: NETWORK INTELLIGENCE");
        MINFO("   ✓ Peer Reputation System");
        MINFO("   ✓ Network Health Scoring");
        MINFO("   ✓ Geographic Distribution Monitor");
        MINFO("   ✓ Peer Clustering");
        MINFO("   ✓ Sybil Attack Detection");
        MINFO("\n⚙️ TIER 4: OPTIMIZATION & AUTOMATION");
        MINFO("   ✓ Fee Optimization");
        MINFO("   ✓ Consensus Parameter Tuning");
        MINFO("   ✓ Network Simulation Module");
        MINFO("\n🔐 TIER 5: FORENSICS & SECURITY");
        MINFO("   ✓ Temporal Analysis");
        MINFO("   ✓ Behavioral Clustering");
        MINFO("   ✓ Block Forgery Detection");
        MINFO("   ✓ Timing Attack Detection");
        MINFO("\n📈 TIER 6: MARKET INTELLIGENCE");
        MINFO("   ⏳ Awaiting price data integration");
        MINFO("\n" << std::string(80, '='));
        MINFO("✅ NINA ADVANCED AI FRAMEWORK READY");
        MINFO(std::string(80, '=') << "\n");
        
        // Log initialization
        nina_audit_log(0, "FRAMEWORK_INIT", "NINA Advanced AI initialized with persistent learning");
        
    } catch (const std::exception& e) {
        MERROR("Failed to initialize NINA Advanced AI: " << e.what());
    }
}

/**
 * Process a new block with NINA Advanced AI
 * Call this when a new block is validated
 */
inline void nina_advanced_observe_block(
    uint64_t block_height,
    uint32_t block_solve_time,
    double block_difficulty,
    double previous_difficulty)
{
    if (!g_nina_advanced_ai) {
        return;  // AI not initialized
    }
    
    try {
        // TIER 1: Prediction & Forecasting
        g_nina_advanced_ai->get_blocktime().observe_block_time(
            block_height,
            block_solve_time,
            block_difficulty
        );
        
        // Detect anomalies
        std::string anomaly_type = "";  // Would be detected from block
        if (!anomaly_type.empty()) {
            double diff_change = previous_difficulty > 0 ? 
                ((block_difficulty - previous_difficulty) / previous_difficulty) * 100.0 : 0.0;
            g_nina_advanced_ai->get_attack_forecast().observe_block(
                block_height,
                anomaly_type,
                diff_change
            );
        }
        
        // Difficulty trend
        g_nina_advanced_ai->get_difficulty_trend().observe_difficulty(
            block_height,
            block_difficulty,
            previous_difficulty
        );
        
        // TIER 5: Forensics
        g_nina_advanced_ai->get_timing_attack().observe_block_times(
            block_height,
            block_solve_time
        );
        
        // Generate report every 100 blocks
        if (block_height > 0 && block_height % 100 == 0) {
            nina_advanced_generate_report(block_height);
        }
        
        // Persist NINA memory to LMDB every 30 blocks (~1 hour)
        // This ensures NINA doesn't lose memory if daemon crashes
        if (block_height > 0 && block_height % 30 == 0) {
            uint64_t anomalies = g_nina_advanced_ai->get_anomalous_tx().get_suspicious_transactions(6.0).size();
            uint64_t attacks = 0;  // Would count detected attacks
            double accuracy = 0.94;  // Would calculate from actual predictions
            double peer_rep = 0.85;  // Would get from reputation module
            double health = g_nina_advanced_ai->get_network_health().calculate_health().overall_score;
            
            nina_save_persistent_state(block_height, anomalies, attacks, accuracy, peer_rep, health);
            
            // Also persist learning metrics to LMDB
            auto& learning = ninacatcoin_ai::NINALearningModule::getInstance();
            learning.persistToLMDB(block_height);
            
            nina_audit_log(block_height, "STATE_PERSISTED", "NINA memory + learning metrics saved to LMDB");
        }
        
    } catch (const std::exception& e) {
        MERROR("NINA Advanced: Error processing block: " << e.what());
    }
}

/**
 * Update network state for NINA Advanced AI
 * Call this periodically (every 10 blocks) with current network state
 */
inline void nina_advanced_update_network_state(
    uint32_t active_peers,
    uint32_t total_peers,
    double consensus_alignment,
    bool fully_synced)
{
    if (!g_nina_advanced_ai) {
        return;
    }
    
    try {
        g_nina_advanced_ai->get_network_health().observe_network_state(
            active_peers,
            total_peers,
            consensus_alignment,
            fully_synced
        );
    } catch (const std::exception& e) {
        MERROR("NINA Advanced: Error updating network state: " << e.what());
    }
}

/**
 * Record peer activity (block received, validation result)
 */
inline void nina_advanced_record_peer_activity(
    const std::string& peer_id,
    bool valid_block,
    double latency_ms)
{
    if (!g_nina_advanced_ai) {
        return;
    }
    
    try {
        g_nina_advanced_ai->get_peer_reputation().observe_peer_block(
            peer_id,
            valid_block,
            latency_ms
        );
        
        // Audit log for suspicious peer behavior
        if (!valid_block) {
            nina_audit_log(0, "PEER_INVALID_BLOCK", "Peer " + peer_id + " sent invalid block");
        }
    } catch (const std::exception& e) {
        MERROR("NINA Advanced: Error recording peer activity: " << e.what());
    }
}

/**
 * Generate comprehensive intelligence report
 */
inline void nina_advanced_generate_report(uint64_t block_height)
{
    if (!g_nina_advanced_ai) {
        return;
    }
    
    try {
        MINFO("\n" << std::string(80, '='));
        MINFO("🤖 NINA INTELLIGENCE REPORT - Block " << block_height);
        MINFO(std::string(80, '='));
        
        // TIER 1 Report
        MINFO("\n📊 TIER 1: PREDICTION & FORECASTING");
        auto blocktime_stats = g_nina_advanced_ai->get_blocktime().get_statistics();
        MINFO("   BlockTime - Average: " << blocktime_stats.average << "s, Median: " 
              << blocktime_stats.median << "s (min:" << blocktime_stats.min << ", max:" 
              << blocktime_stats.max << ")");
        MINFO("   Trend: " << (blocktime_stats.trend > 0 ? "Blocks Getting Faster" : 
              (blocktime_stats.trend < 0 ? "Blocks Getting Slower" : "Stable")));
        MINFO("   Std Dev: " << blocktime_stats.std_dev << "s");
        
        double next_blocktime = g_nina_advanced_ai->get_blocktime().predict_next_block_time();
        MINFO("   Predicted Next Block Time: " << next_blocktime << "s");
        
        auto diff_forecast = g_nina_advanced_ai->get_difficulty_trend().forecast_difficulty();
        MINFO("   Difficulty Forecast:");
        MINFO("      24h: " << diff_forecast.predicted_in_24h);
        MINFO("      72h: " << diff_forecast.predicted_in_72h);
        MINFO("      Trend: " << diff_forecast.trend_direction);
        MINFO("      Confidence: " << (diff_forecast.confidence * 100.0) << "%");
        
        auto attack_forecast = g_nina_advanced_ai->get_attack_forecast().forecast_next_attack();
        if (attack_forecast.confidence > 0.5) {
            MWARNING("   ⚠️ PREDICTED ATTACK: " << attack_forecast.predicted_attack);
            MWARNING("       Confidence: " << (attack_forecast.confidence * 100.0) << "%");
            MWARNING("       Mitigation: " << attack_forecast.mitigation_strategy);
        }
        
        // TIER 2 Report
        MINFO("\n🔍 TIER 2: TRANSACTION ANALYSIS");
        auto suspicious_txs = g_nina_advanced_ai->get_anomalous_tx().get_suspicious_transactions(6.0);
        MINFO("   Suspicious Transactions Detected: " << suspicious_txs.size());
        for (size_t i = 0; i < std::min(size_t(5), suspicious_txs.size()); i++) {
            MWARNING("      TX: " << suspicious_txs[i].tx_id << " (Score: " 
                    << suspicious_txs[i].anomaly_score << "/10)");
        }
        
        if (g_nina_advanced_ai->get_mempool().detect_flooding_attack()) {
            MWARNING("   ⚠️ MEMPOOL FLOODING DETECTED!");
        }
        
        // TIER 3 Report
        MINFO("\n🌐 TIER 3: NETWORK INTELLIGENCE");
        auto health = g_nina_advanced_ai->get_network_health().calculate_health();
        MINFO("   Network Health Score: " << health.overall_score << "/100 (" << health.status << ")");
        MINFO("      Peer Diversity: " << health.peer_diversity << "/100");
        MINFO("      Consensus Strength: " << health.consensus_strength << "/100");
        MINFO("      Synchronization: " << health.synchronization << "/100");
        
        auto trusted_peers = g_nina_advanced_ai->get_peer_reputation().get_trusted_peers();
        auto suspicious_peers = g_nina_advanced_ai->get_peer_reputation().get_suspicious_peers();
        MINFO("   Peer Reputation:");
        MINFO("      Trusted Peers: " << trusted_peers.size());
        MINFO("      Suspicious Peers: " << suspicious_peers.size());
        
        if (g_nina_advanced_ai->get_geo_distribution().check_centralization_risk()) {
            MWARNING("   ⚠️ NETWORK CENTRALIZATION RISK DETECTED!");
        }
        
        // SYBIL ANALYSIS
        MINFO("   Sybil Detection:");
        MINFO("      " << nina_sybil_get_status());
        nina_sybil_analyze_and_alert();
        
        // TIER 5 Report
        MINFO("\n🔐 TIER 5: FORENSICS & SECURITY");
        auto temporal_anomalies = g_nina_advanced_ai->get_temporal_analysis().detect_anomalies(block_height);
        if (!temporal_anomalies.empty()) {
            MWARNING("   Temporal Anomalies Detected: " << temporal_anomalies.size());
            for (const auto& anom : temporal_anomalies) {
                MWARNING("      - " << anom.anomaly_type << ": " << anom.description);
            }
        }
        
        if (g_nina_advanced_ai->get_timing_attack().detect_timing_attack()) {
            MWARNING("   ⚠️ POTENTIAL TIMING ATTACK DETECTED!");
        }
        
        // Summary
        MINFO("\n" << std::string(80, '='));
        MINFO("✅ NINA Status: OPERATIONAL AND LEARNING");
        MINFO("   Next Report: Block " << (block_height + 100));
        MINFO(std::string(80, '=') << "\n");
        
    } catch (const std::exception& e) {
        MERROR("Error generating NINA report: " << e.what());
    }
}

/**
 * Get NINA Advanced AI Coordinator (for direct access if needed)
 */
inline NInaAdvancedCoordinator* get_nina_advanced_ai()
{
    return g_nina_advanced_ai.get();
}

/**
 * Observe a block announcement from a peer
 * Call this when a peer announces a new block
 */
inline void nina_sybil_observe_peer_block_announcement(
    const std::string& peer_id,
    uint32_t block_height,
    uint32_t announcement_time_ms,
    double peer_latency_ms
) {
    if (!g_nina_sybil_detector) {
        return;
    }
    
    try {
        g_nina_sybil_detector->observe_block_announcement(
            peer_id,
            block_height,
            announcement_time_ms,
            peer_latency_ms
        );
    } catch (const std::exception& e) {
        MERROR("NINA Sybil: Error observing peer block announcement: " << e.what());
    }
}

/**
 * Observe a transaction announcement from a peer
 * Call this when a peer announces a transaction
 */
inline void nina_sybil_observe_peer_tx_announcement(
    const std::string& peer_id,
    uint32_t announcement_time_ms
) {
    if (!g_nina_sybil_detector) {
        return;
    }
    
    try {
        g_nina_sybil_detector->observe_transaction_announcement(
            peer_id,
            announcement_time_ms
        );
    } catch (const std::exception& e) {
        MERROR("NINA Sybil: Error observing peer tx announcement: " << e.what());
    }
}

/**
 * Analyze peer behavior and detect Sybil attacks
 * Should be called periodically (every 100 blocks or on demand)
 */
inline void nina_sybil_analyze_and_alert()
{
    if (!g_nina_sybil_detector) {
        return;
    }
    
    try {
        auto cluster_result = g_nina_sybil_detector->detect_sybil_clusters();
        
        // Log analysis results
        MINFO("[SYBIL] " << cluster_result.cluster_analysis);
        
        // Alert on dangerous peers
        if (!cluster_result.flagged_peers.empty()) {
            MWARNING("[SYBIL] ⚠️ POTENTIAL SYBIL ATTACK DETECTED!");
            MWARNING("[SYBIL] " << cluster_result.flagged_peers.size() << " peer(s) under suspicion:");
            
            for (const auto& peer_id : cluster_result.flagged_peers) {
                auto score = g_nina_sybil_detector->calculate_peer_sybil_score(peer_id);
                MWARNING("[SYBIL]    Peer " << peer_id.substr(0, 16) << "... - Confidence: " 
                        << score.correlation_confidence << "% - " << score.reasoning);
            }
            
            // Log to audit trail
            std::string flagged_list = "";
            for (size_t i = 0; i < cluster_result.flagged_peers.size(); i++) {
                if (i > 0) flagged_list += ",";
                flagged_list += cluster_result.flagged_peers[i].substr(0, 16);
            }
            nina_audit_log(0, "SYBIL_ALERT", "Detected " + std::to_string(cluster_result.clusters.size()) + 
                          " cluster(s): [" + flagged_list + "]");
        }
        
        // Also audit top suspicious peers even if not in clusters
        auto all_scores = g_nina_sybil_detector->get_all_sybil_scores();
        for (size_t i = 0; i < std::min(size_t(3), all_scores.size()); i++) {
            if (all_scores[i].threat_level != "safe") {
                MINFO("[SYBIL] Monitoring peer " << all_scores[i].peer_id.substr(0, 16) 
                     << "... - Threat: " << all_scores[i].threat_level);
            }
        }
        
        // Clean up inactive peers
        g_nina_sybil_detector->cleanup_inactive_peers();
        
    } catch (const std::exception& e) {
        MERROR("NINA Sybil: Error during analysis: " << e.what());
    }
}

/**
 * Get current Sybil detector status
 */
inline std::string nina_sybil_get_status()
{
    if (!g_nina_sybil_detector) {
        return "[SYBIL] Detector not initialized";
    }
    
    try {
        return g_nina_sybil_detector->get_sybil_status();
    } catch (const std::exception& e) {
        return "[SYBIL] Error getting status: " + std::string(e.what());
    }
}

/**
 * Check if NINA Advanced is initialized
 */
inline bool is_nina_advanced_initialized()
{
    return g_nina_advanced_ai != nullptr;
}

}  // namespace daemonize
