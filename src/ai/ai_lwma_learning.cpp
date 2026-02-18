#include "ai_lwma_learning.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace ninacatcoin_ai {

// =====================================================
// Global state for LWMA learning
// =====================================================

static const int MAX_HISTORY = 100;
static std::deque<AILWMALearning::LWMASnapshot> g_difficulty_history;
static std::map<std::string, AILWMALearning::DifficultyPattern> g_patterns;
static uint64_t g_blocks_monitored = 0;
static bool g_initialized = false;

// =====================================================
// Initialization
// =====================================================

void AILWMALearning::initialize() {
    if (g_initialized) return;
    
    g_difficulty_history.clear();
    g_patterns.clear();
    g_blocks_monitored = 0;
    
    // Initialize known patterns
    g_patterns["stable"] = {
        "stable",           // pattern_name
        0,                  // occurrence_count
        120.0,              // average_block_time (target)
        100.0,              // stability_score
        false               // is_concerning
    };
    
    g_patterns["recovering"] = {
        "recovering",       // pattern_name
        0,                  // occurrence_count
        150.0,              // average_block_time (slower)
        70.0,               // stability_score
        false               // is_concerning (expected after hashrate loss)
    };
    
    g_patterns["attacking"] = {
        "attacking",        // pattern_name
        0,                  // occurrence_count
        60.0,               // average_block_time (too fast)
        20.0,               // stability_score
        true                // is_concerning (anomalous)
    };
    
    MGINFO("[LWMA-Learning] IA initialized and ready to observe LWMA-1 behavior");
    g_initialized = true;
}

// =====================================================
// Core Learning
// =====================================================

void AILWMALearning::observe_lwma_calculation(
    uint64_t height,
    uint64_t calculated_difficulty,
    uint64_t previous_difficulty,
    uint64_t block_solve_time)
{
    if (!g_initialized) initialize();
    
    // Calculate percentage change
    double change_percent = 0.0;
    if (previous_difficulty > 0) {
        change_percent = ((double)(calculated_difficulty - previous_difficulty) / previous_difficulty) * 100.0;
    }
    
    // Detect anomalies
    std::string anomaly = "";
    bool is_anomalous = false;
    
    if (change_percent > 50.0) {
        anomaly = "rapid_increase";
        is_anomalous = true;
    } else if (change_percent < -50.0) {
        anomaly = "rapid_decrease";
        is_anomalous = true;
    } else if (block_solve_time > 720) {  // 6x target time
        anomaly = "timestamp_attack";
        is_anomalous = true;
    }
    
    // Create snapshot
    LWMASnapshot snapshot = {
        height,
        calculated_difficulty,
        previous_difficulty,
        block_solve_time,
        change_percent,
        is_anomalous,
        anomaly,
        time(nullptr)
    };
    
    g_difficulty_history.push_back(snapshot);
    g_blocks_monitored++;
    
    // Keep history limited
    if (g_difficulty_history.size() > MAX_HISTORY) {
        g_difficulty_history.pop_front();
    }
    
    if (is_anomalous) {
        MWARNING("[LWMA-Learning] Anomaly detected at height " << height 
                 << ": " << anomaly 
                 << " (difficulty change: " << std::fixed << std::setprecision(2) 
                 << change_percent << "%)");
    }
}

// =====================================================
// Anomaly Detection
// =====================================================

std::string AILWMALearning::detect_difficulty_anomaly(
    uint64_t current_diff,
    uint64_t previous_diff,
    uint64_t block_time)
{
    std::stringstream report;
    
    // Check for manipulation
    if (block_time > 720) {  // 6x target (120s)
        report << "TIMESTAMP ATTACK detected! Block time: " << block_time 
               << "s (normal: ~120s)";
        return report.str();
    }
    
    if (current_diff == 0 || previous_diff == 0) {
        return "INVALID difficulty value";
    }
    
    double ratio = (double)current_diff / previous_diff;
    
    if (ratio > 2.0) {
        report << "RAPID DIFFICULTY INCREASE: " 
               << std::fixed << std::setprecision(2) 
               << (ratio - 1.0) * 100.0 << "% spike";
        return report.str();
    }
    
    if (ratio < 0.5) {
        report << "RAPID DIFFICULTY DROP: " 
               << std::fixed << std::setprecision(2) 
               << (1.0 - ratio) * 100.0 << "% crash";
        return report.str();
    }
    
    // All normal
    return "";
}

// =====================================================
// Pattern Recognition
// =====================================================

std::string AILWMALearning::identify_current_pattern() {
    if (g_difficulty_history.size() < 5) {
        return "insufficient_data";
    }
    
    // Calculate average block time from last 10 blocks
    int sample_size = std::min(size_t(10), g_difficulty_history.size());
    uint64_t total_time = 0;
    
    for (size_t i = g_difficulty_history.size() - sample_size; i < g_difficulty_history.size(); ++i) {
        total_time += g_difficulty_history[i].block_solve_time;
    }
    
    double avg_time = (double)total_time / sample_size;
    
    // Classify pattern
    if (avg_time < 100.0) {
        g_patterns["attacking"].occurrence_count++;
        return "attacking";
    } else if (avg_time > 150.0) {
        g_patterns["recovering"].occurrence_count++;
        return "recovering";
    } else {
        g_patterns["stable"].occurrence_count++;
        return "stable";
    }
}

// =====================================================
// Prediction
// =====================================================

std::string AILWMALearning::predict_next_difficulty(int recent_blocks) {
    if (g_difficulty_history.size() < 3) {
        return "0 (insufficient data)";
    }
    
    int sample_size = std::min((int)g_difficulty_history.size(), recent_blocks);
    double total_diff = 0.0;
    double weights = 0.0;
    
    // Weighted average: more recent blocks have higher weight
    for (int i = 0; i < sample_size; ++i) {
        int idx = g_difficulty_history.size() - sample_size + i;
        double weight = (double)(i + 1) / sample_size;  // Linear weights
        total_diff += g_difficulty_history[idx].calculated_difficulty * weight;
        weights += weight;
    }
    
    uint64_t predicted = (uint64_t)(total_diff / weights);
    
    // Estimate confidence based on stability
    uint8_t confidence = 70;  // Base confidence
    if (g_difficulty_history.size() >= 60) {
        confidence = 90;  // High confidence with full window
    }
    
    std::stringstream report;
    report << predicted << " (" << (int)confidence << "% confidence)";
    return report.str();
}

// =====================================================
// Network Analysis
// =====================================================

std::string AILWMALearning::estimate_network_hashrate() {
    if (g_difficulty_history.empty()) {
        return "unknown (no data)";
    }
    
    const auto& latest = g_difficulty_history.back();
    
    // Simple estimation: hashrate = difficulty * 2^32 / block_time
    // For 120s target: hashrate ≈ difficulty * 35.8
    double estimated_rate = latest.calculated_difficulty * 35.8;
    
    std::stringstream report;
    
    if (estimated_rate > 1e12) {
        report << std::fixed << std::setprecision(2) 
               << (estimated_rate / 1e12) << " TH/s";
    } else if (estimated_rate > 1e9) {
        report << std::fixed << std::setprecision(2) 
               << (estimated_rate / 1e9) << " GH/s";
    } else {
        report << std::fixed << std::setprecision(2) 
               << (estimated_rate / 1e6) << " MH/s";
    }
    
    return report.str();
}

std::string AILWMALearning::detect_timestamp_attack(const std::vector<uint64_t>& recent_times) {
    if (recent_times.size() < 3) {
        return "";
    }
    
    // Check for suspiciously large values
    uint64_t max_time = *std::max_element(recent_times.begin(), recent_times.end());
    
    if (max_time > 720) {  // 6x target time
        std::stringstream report;
        report << "⚠️ TIMESTAMP MANIPULATION DETECTED!\n"
               << "  Abnormal block time: " << max_time << " seconds\n"
               << "  Normal target: 120 seconds\n"
               << "  This allows LWMA-1 to underestimate difficulty";
        return report.str();
    }
    
    return "";
}

// =====================================================
// Reporting
// =====================================================

std::string AILWMALearning::generate_network_health_report() {
    std::stringstream report;
    
    if (!g_initialized || g_difficulty_history.empty()) {
        report << "═══════════════════════════════════════════\n"
               << "LWMA-Learning Network Health Report\n"
               << "═══════════════════════════════════════════\n"
               << "Status: NO DATA (IA still initializing)";
        return report.str();
    }
    
    const auto& latest = g_difficulty_history.back();
    std::string pattern = identify_current_pattern();
    
    report << "╔═══════════════════════════════════════════╗\n"
           << "║   LWMA-Learning Network Health Report     ║\n"
           << "╠═══════════════════════════════════════════╣\n"
           << "║ Current State:\n"
           << "║   Pattern: " << pattern << "\n"
           << "║   Height: " << latest.block_height << "\n"
           << "║   Difficulty: " << latest.calculated_difficulty << "\n"
           << "║   Avg Block Time (last 10): ";
    
    if (g_difficulty_history.size() >= 10) {
        uint64_t total = 0;
        for (size_t i = g_difficulty_history.size() - 10; i < g_difficulty_history.size(); ++i) {
            total += g_difficulty_history[i].block_solve_time;
        }
        report << std::fixed << std::setprecision(1) << (double)total / 10.0 << "s\n";
    } else {
        report << "calculating\n";
    }
    
    report << "║   Est. Hashrate: " << estimate_network_hashrate() << "\n"
           << "║   Stability: " << (int)get_stability_confidence() << "%\n"
           << "║ Blocks Monitored: " << g_blocks_monitored << "\n"
           << "║ Anomalies Detected: ";
    
    uint64_t anomaly_count = 0;
    for (const auto& snap : g_difficulty_history) {
        if (snap.is_anomalous) anomaly_count++;
    }
    report << anomaly_count << "\n"
           << "╚═══════════════════════════════════════════╝";
    
    return report.str();
}

uint8_t AILWMALearning::get_stability_confidence() {
    if (g_difficulty_history.size() < 10) {
        return 0;
    }
    
    // Count anomalies in last 60 blocks
    int sample_size = std::min(size_t(60), g_difficulty_history.size());
    uint64_t anomalies = 0;
    
    for (size_t i = g_difficulty_history.size() - sample_size; i < g_difficulty_history.size(); ++i) {
        if (g_difficulty_history[i].is_anomalous) {
            anomalies++;
        }
    }
    
    // Score: 100 - (anomalies * 5)
    int score = 100 - (anomalies * 5);
    return std::max(0, std::min(100, score));
}

bool AILWMALearning::is_network_recovering() {
    std::string pattern = identify_current_pattern();
    return pattern == "recovering";
}

std::map<std::string, AILWMALearning::DifficultyPattern> AILWMALearning::get_learned_patterns() {
    return g_patterns;
}

void AILWMALearning::reset_learning() {
    g_difficulty_history.clear();
    g_patterns.clear();
    g_blocks_monitored = 0;
    initialize();
    MGINFO("[LWMA-Learning] Learning state reset");
}

std::string AILWMALearning::get_statistics() {
    std::stringstream stats;
    
    stats << "LWMA-Learning Statistics:\n"
          << "  Total blocks observed: " << g_blocks_monitored << "\n"
          << "  Current history size: " << g_difficulty_history.size() << "\n"
          << "  Patterns learned: " << g_patterns.size() << "\n";
    
    for (const auto& [name, pattern] : g_patterns) {
        stats << "    - " << name << ": " << pattern.occurrence_count << " occurrences\n";
    }
    
    return stats.str();
}

} // namespace ninacatcoin_ai
