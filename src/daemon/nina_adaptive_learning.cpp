/**
 * NINA Adaptive Learning Engine Implementation - REAL VERSION
 * 
 * Maintains a circular buffer of block observations and computes
 * REAL statistics from actual blockchain data. No hardcoded values.
 * State persists in memory during daemon lifetime and predictions
 * are compared against actual outcomes for accuracy tracking.
 */

#include "nina_adaptive_learning.hpp"
#include "misc_log_ex.h"
#include <cmath>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <deque>
#include <mutex>

namespace ninacatcoin_ai {

// ============================================================================
// Internal state — real data, not hardcoded
// ============================================================================
static std::mutex g_learning_mutex;
static std::deque<std::vector<double>> g_attack_features;  // observed attack patterns
static std::deque<std::pair<std::string, double>> g_baselines; // (metric_name, value)
static std::deque<ModelPrediction> g_prediction_history; // track predictions for accuracy
static uint64_t g_total_predictions = 0;
static double g_cumulative_error = 0.0;
static uint64_t g_accuracy_samples = 0;
static const size_t MAX_HISTORY = 500;

// ============================================================================
// Attack Pattern Learning — stores feature vectors for anomaly baseline
// ============================================================================
void NInaAdaptiveLearning::learn_attack_pattern(
    const std::string& attack_type,
    std::vector<double> features,
    int block_height)
{
    std::lock_guard<std::mutex> lock(g_learning_mutex);
    
    // Store the actual feature vector for future comparison
    g_attack_features.push_back(features);
    if (g_attack_features.size() > MAX_HISTORY)
        g_attack_features.pop_front();
    
    // Calculate real statistics on the features
    double mean = 0.0, max_val = -1e18, min_val = 1e18;
    for (double f : features) {
        mean += f;
        max_val = std::max(max_val, f);
        min_val = std::min(min_val, f);
    }
    mean /= static_cast<double>(features.size());
    
    double variance = 0.0;
    for (double f : features) {
        variance += (f - mean) * (f - mean);
    }
    variance /= static_cast<double>(features.size());
    
    MINFO("[LEARNING] Attack pattern '" << attack_type << "' at height " << block_height
          << " | features=" << features.size() 
          << " mean=" << std::fixed << std::setprecision(4) << mean
          << " std=" << std::sqrt(variance)
          << " range=[" << min_val << "," << max_val << "]"
          << " | total_patterns=" << g_attack_features.size());
}

// ============================================================================
// Baseline Learning — tracks real metric evolultion over time
// ============================================================================
void NInaAdaptiveLearning::learn_baseline_behavior(
    const std::string& metric_name,
    double value,
    int block_height)
{
    std::lock_guard<std::mutex> lock(g_learning_mutex);
    
    g_baselines.push_back({metric_name, value});
    if (g_baselines.size() > MAX_HISTORY * 2)
        g_baselines.pop_front();
    
    // Compute running average for this metric from stored baselines
    double running_sum = 0.0;
    int count = 0;
    for (const auto& [name, val] : g_baselines) {
        if (name == metric_name) {
            running_sum += val;
            count++;
        }
    }
    double running_avg = (count > 0) ? running_sum / count : value;
    double deviation = (running_avg > 0) ? std::abs(value - running_avg) / running_avg * 100.0 : 0.0;
    
    MINFO("[BASELINE] " << metric_name << "=" << std::fixed << std::setprecision(2) << value
          << " | running_avg=" << running_avg << " deviation=" << deviation << "%"
          << " | height=" << block_height << " samples=" << count);
}

// ============================================================================
// Difficulty Prediction — uses exponential weighted moving average (EWMA)
// ============================================================================
ModelPrediction NInaAdaptiveLearning::predict_difficulty(
    int current_height,
    std::vector<int> recent_block_times)
{
    std::lock_guard<std::mutex> lock(g_learning_mutex);
    
    ModelPrediction pred;
    pred.prediction_type = "DIFFICULTY";
    pred.target_height = current_height + 1;
    pred.confidence = 0.0;
    pred.predicted_value = 0.0;
    
    if (recent_block_times.empty()) {
        pred.predicted_value = 100.0;
        pred.confidence = 0.1;
        return pred;
    }
    
    const double target_time = 120.0; // ninacatcoin target
    size_t n = recent_block_times.size();
    
    // 1) Exponential weighted moving average — recent blocks matter more
    double ewma = 0.0;
    double weight_sum = 0.0;
    double alpha = 2.0 / (std::min(n, (size_t)20) + 1); // decay factor
    
    for (size_t i = 0; i < n; ++i) {
        double w = std::pow(1.0 - alpha, static_cast<double>(n - 1 - i));
        ewma += recent_block_times[i] * w;
        weight_sum += w;
    }
    double weighted_avg_time = ewma / weight_sum;
    
    // 2) Calculate trend (are blocks getting faster or slower?)
    double trend = 0.0;
    if (n >= 10) {
        size_t half = n / 2;
        double first_half = 0.0, second_half = 0.0;
        for (size_t i = 0; i < half; ++i) first_half += recent_block_times[i];
        for (size_t i = half; i < n; ++i) second_half += recent_block_times[i];
        first_half /= half;
        second_half /= (n - half);
        trend = (first_half > 0) ? (second_half - first_half) / first_half : 0.0;
    }
    
    // 3) Predict: adjustment factor based on how far off target we are + trend
    double time_ratio = target_time / std::max(weighted_avg_time, 1.0);
    double trend_correction = 1.0 - trend * 0.3; // dampen trend effect
    pred.predicted_value = time_ratio * trend_correction * 100.0;
    
    // 4) Confidence based on data quality (more data + less variance = higher confidence)
    double variance = 0.0;
    double mean = 0.0;
    for (int t : recent_block_times) mean += t;
    mean /= n;
    for (int t : recent_block_times) variance += (t - mean) * (t - mean);
    variance /= n;
    double cv = (mean > 0) ? std::sqrt(variance) / mean : 1.0; // coefficient of variation
    
    // Confidence formula: starts at 0.5, increases with data, decreases with variance
    double data_factor = std::min(1.0, static_cast<double>(n) / 60.0); // full confidence at 60 blocks
    double stability_factor = std::max(0.0, 1.0 - cv); // low cv = high stability
    pred.confidence = 0.3 + 0.4 * data_factor + 0.3 * stability_factor;
    pred.confidence = std::min(0.99, std::max(0.1, pred.confidence));
    
    // Store prediction for accuracy tracking
    g_prediction_history.push_back(pred);
    if (g_prediction_history.size() > MAX_HISTORY)
        g_prediction_history.pop_front();
    g_total_predictions++;
    
    MINFO("[PREDICT] Difficulty for h=" << pred.target_height
          << " | ewma_time=" << std::fixed << std::setprecision(1) << weighted_avg_time << "s"
          << " trend=" << std::setprecision(3) << trend
          << " | multiplier=" << std::setprecision(4) << pred.predicted_value / 100.0
          << " confidence=" << std::setprecision(2) << pred.confidence * 100.0 << "%"
          << " (from " << n << " blocks, cv=" << std::setprecision(3) << cv << ")");
    
    return pred;
}

// ============================================================================
// Network Health — real aggregation of peer metrics
// ============================================================================
ModelPrediction NInaAdaptiveLearning::predict_network_health(
    int current_height,
    std::vector<double> peer_metrics)
{
    ModelPrediction pred;
    pred.prediction_type = "NETWORK_HEALTH";
    pred.target_height = current_height + 1;
    
    if (peer_metrics.empty()) {
        pred.predicted_value = 0.5;
        pred.confidence = 0.1;
        return pred;
    }
    
    // Compute robust statistics (median + MAD instead of mean)
    std::vector<double> sorted_metrics = peer_metrics;
    std::sort(sorted_metrics.begin(), sorted_metrics.end());
    
    size_t n = sorted_metrics.size();
    double median = sorted_metrics[n / 2];
    
    // Median absolute deviation (MAD) — robust variance measure
    std::vector<double> abs_devs;
    for (double m : sorted_metrics)
        abs_devs.push_back(std::abs(m - median));
    std::sort(abs_devs.begin(), abs_devs.end());
    double mad = abs_devs[abs_devs.size() / 2];
    
    pred.predicted_value = median;
    // Confidence: more peers + less deviation = better
    double peer_factor = std::min(1.0, static_cast<double>(n) / 10.0);
    double consistency = (median > 0) ? std::max(0.0, 1.0 - mad / median) : 0.5;
    pred.confidence = 0.3 + 0.35 * peer_factor + 0.35 * consistency;
    pred.confidence = std::min(0.99, std::max(0.1, pred.confidence));
    
    MINFO("[HEALTH] Network prediction: " << std::fixed << std::setprecision(2)
          << pred.predicted_value * 100.0 << "% (median)"
          << " | peers=" << n << " MAD=" << std::setprecision(4) << mad
          << " confidence=" << std::setprecision(1) << pred.confidence * 100.0 << "%");
    
    return pred;
}

// ============================================================================
// Pattern Recognition — compares current features against known attack baselines
// ============================================================================
std::vector<LearningPattern> NInaAdaptiveLearning::recognize_patterns(
    const std::vector<double>& current_features)
{
    std::lock_guard<std::mutex> lock(g_learning_mutex);
    
    std::vector<LearningPattern> patterns;
    
    if (current_features.empty()) return patterns;
    
    // Calculate mean and std of current observation
    double curr_mean = 0.0;
    for (double f : current_features) curr_mean += f;
    curr_mean /= current_features.size();
    
    double curr_var = 0.0;
    for (double f : current_features) curr_var += (f - curr_mean) * (f - curr_mean);
    curr_var /= current_features.size();
    double curr_std = std::sqrt(curr_var);
    
    // Compare against stored attack feature distributions using Mahalanobis-like distance
    if (!g_attack_features.empty()) {
        // Compute centroid and spread of known attack patterns
        size_t dim = current_features.size();
        std::vector<double> attack_means(dim, 0.0);
        int comparable = 0;
        
        for (const auto& af : g_attack_features) {
            if (af.size() >= dim) {
                for (size_t i = 0; i < dim; ++i)
                    attack_means[i] += af[i];
                comparable++;
            }
        }
        
        if (comparable > 0) {
            for (size_t i = 0; i < dim; ++i) attack_means[i] /= comparable;
            
            // Euclidean distance between current and attack centroid
            double dist = 0.0;
            for (size_t i = 0; i < dim; ++i) {
                double d = current_features[i] - attack_means[i];
                dist += d * d;
            }
            dist = std::sqrt(dist);
            
            // Convert distance to similarity/threat score
            double similarity = std::exp(-dist * 0.1); // 0-1, higher = more similar to attacks
            
            LearningPattern lp;
            if (similarity > 0.7) {
                lp.pattern_type = "ATTACK_MATCH";
                lp.confidence = similarity;
                lp.threat_level = "DANGEROUS";
            } else if (similarity > 0.4) {
                lp.pattern_type = "SUSPICIOUS_BEHAVIOR";
                lp.confidence = similarity;
                lp.threat_level = "SUSPICIOUS";
            } else {
                lp.pattern_type = "NORMAL_BEHAVIOR";
                lp.confidence = 1.0 - similarity;
                lp.threat_level = "SAFE";
            }
            lp.occurrence_count = comparable;
            patterns.push_back(lp);
        }
    }
    
    // If no attack patterns to compare, just assess normality based on features
    if (patterns.empty()) {
        LearningPattern lp;
        lp.pattern_type = "NORMAL_BEHAVIOR";
        lp.confidence = 0.5; // uncertain, no baseline yet
        lp.threat_level = "SAFE";
        lp.occurrence_count = 0;
        patterns.push_back(lp);
    }
    
    MINFO("[RECOGNITION] " << patterns.size() << " patterns identified"
          << " | attack_db_size=" << g_attack_features.size()
          << " | top_pattern=" << patterns[0].pattern_type
          << " (" << patterns[0].threat_level << ", conf=" 
          << std::fixed << std::setprecision(2) << patterns[0].confidence << ")");
    
    return patterns;
}

// ============================================================================
// Accuracy Tracking — REAL feedback loop
// ============================================================================
void NInaAdaptiveLearning::update_model_accuracy(
    const std::string& model_name,
    double actual_value,
    double predicted_value)
{
    std::lock_guard<std::mutex> lock(g_learning_mutex);
    
    double error = (actual_value != 0.0) 
        ? std::abs(actual_value - predicted_value) / std::abs(actual_value) 
        : std::abs(predicted_value);
    double accuracy = std::max(0.0, 1.0 - error);
    
    g_cumulative_error += error;
    g_accuracy_samples++;
    
    double running_accuracy = (g_accuracy_samples > 0)
        ? 1.0 - (g_cumulative_error / g_accuracy_samples) : 0.0;
    
    MINFO("[ACCURACY] " << model_name 
          << " | actual=" << std::fixed << std::setprecision(2) << actual_value
          << " predicted=" << predicted_value
          << " | this_accuracy=" << std::setprecision(1) << accuracy * 100.0 << "%"
          << " | running_avg=" << running_accuracy * 100.0 << "%"
          << " (over " << g_accuracy_samples << " samples)");
}

// ============================================================================
// Stats — returns REAL computed values, not hardcoded
// ============================================================================
NInaAdaptiveLearning::LearningStats NInaAdaptiveLearning::get_learning_stats()
{
    std::lock_guard<std::mutex> lock(g_learning_mutex);
    
    LearningStats stats;
    stats.patterns_learned = static_cast<int>(g_attack_features.size());
    stats.average_prediction_accuracy = (g_accuracy_samples > 0)
        ? std::max(0.0, 1.0 - (g_cumulative_error / g_accuracy_samples)) : 0.0;
    stats.learning_sessions = static_cast<int>(g_total_predictions);
    
    return stats;
}

// ============================================================================
// Insights — generated from REAL data analysis
// ============================================================================
std::vector<std::string> NInaAdaptiveLearning::get_insights()
{
    std::lock_guard<std::mutex> lock(g_learning_mutex);
    
    std::vector<std::string> insights;
    
    // Insight from prediction accuracy
    if (g_accuracy_samples > 10) {
        double avg_acc = 1.0 - (g_cumulative_error / g_accuracy_samples);
        std::ostringstream oss;
        oss << "Model accuracy over " << g_accuracy_samples << " predictions: "
            << std::fixed << std::setprecision(1) << avg_acc * 100.0 << "%";
        insights.push_back(oss.str());
    } else {
        insights.push_back("Insufficient data for accuracy assessment (need >10 predictions)");
    }
    
    // Insight from attack pattern database
    {
        std::ostringstream oss;
        oss << "Attack pattern database: " << g_attack_features.size() << " patterns stored";
        insights.push_back(oss.str());
    }
    
    // Insight from baseline trends
    if (g_baselines.size() >= 20) {
        // Check if recent baselines differ from older ones (drift detection)
        size_t n = g_baselines.size();
        size_t half = n / 2;
        double first_half_avg = 0.0, second_half_avg = 0.0;
        int fh_count = 0, sh_count = 0;
        for (size_t i = 0; i < half; ++i) {
            first_half_avg += g_baselines[i].second;
            fh_count++;
        }
        for (size_t i = half; i < n; ++i) {
            second_half_avg += g_baselines[i].second;
            sh_count++;
        }
        first_half_avg /= std::max(fh_count, 1);
        second_half_avg /= std::max(sh_count, 1);
        
        double drift = (first_half_avg > 0) 
            ? (second_half_avg - first_half_avg) / first_half_avg * 100.0 : 0.0;
        
        std::ostringstream oss;
        if (std::abs(drift) > 10.0)
            oss << "⚠ Baseline drift detected: " << std::fixed << std::setprecision(1) << drift << "% change in recent metrics";
        else
            oss << "Baseline metrics stable (drift: " << std::fixed << std::setprecision(1) << drift << "%)";
        insights.push_back(oss.str());
    }
    
    // Insight from prediction history
    if (!g_prediction_history.empty()) {
        double avg_conf = 0.0;
        for (const auto& p : g_prediction_history) avg_conf += p.confidence;
        avg_conf /= g_prediction_history.size();
        
        std::ostringstream oss;
        oss << "Average prediction confidence: " << std::fixed << std::setprecision(1) 
            << avg_conf * 100.0 << "% (over " << g_prediction_history.size() << " predictions)";
        insights.push_back(oss.str());
    }
    
    MINFO("[INSIGHTS] Generated " << insights.size() << " insights from real data");
    
    return insights;
}

} // namespace ninacatcoin_ai
