/**
 * NINA Adaptive Learning Engine - TIER 5
 * Learns from patterns WITHOUT modifying core rules
 * Improves accuracy over time while staying aligned
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <cstdint>

namespace ninacatcoin_ai {

/**
 * Learning Pattern - tracks behavior patterns NINA observes
 */
struct LearningPattern {
    std::string pattern_type;           // "ATTACK_SIGNATURE", "NORMAL_BEHAVIOR", etc.
    std::vector<double> feature_vector; // Numerical representation
    int occurrence_count;
    double confidence;                  // How confident in this pattern
    std::time_t first_observed;
    std::time_t last_updated;
    std::string threat_level;           // "SAFE", "SUSPICIOUS", "DANGEROUS"
};

/**
 * Model Prediction - what NINA predicts will happen
 */
struct ModelPrediction {
    std::string prediction_type;
    int target_height;
    double predicted_value;
    double confidence;
    std::string actual_outcome;         // Filled in later
    double prediction_accuracy;         // How accurate was this?
};

class NInaAdaptiveLearning {
public:
    /**
     * Learn attack patterns WITHOUT implementing countermeasures
     * Improves detection accuracy
     */
    static void learn_attack_pattern(
        const std::string& attack_type,
        std::vector<double> features,
        int block_height
    );

    /**
     * Learn normal network behavior baseline
     */
    static void learn_baseline_behavior(
        const std::string& metric_name,
        double value,
        int block_height
    );

    /**
     * Predict next difficulty WITHOUT modifying consensus
     */
    static ModelPrediction predict_difficulty(
        int current_height,
        std::vector<int> recent_block_times
    );

    /**
     * Predict network health trend
     */
    static ModelPrediction predict_network_health(
        int current_height,
        std::vector<double> peer_metrics
    );

    /**
     * Recognize attack patterns from learned knowledge
     */
    static std::vector<LearningPattern> recognize_patterns(
        const std::vector<double>& current_features
    );

    /**
     * Update model accuracy based on outcomes
     * Improves future predictions
     */
    static void update_model_accuracy(
        const std::string& model_name,
        double actual_value,
        double predicted_value
    );

    /**
     * Get learning statistics
     */
    struct LearningStats {
        int patterns_learned;
        double average_prediction_accuracy;
        int learning_sessions;
        std::time_t last_learning_update;
    };

    static LearningStats get_learning_stats();

    /**
     * Get insights from learned patterns
     */
    static std::vector<std::string> get_insights();
};

} // namespace ninacatcoin_ai
