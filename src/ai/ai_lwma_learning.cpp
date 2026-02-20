#include "ai_lwma_learning.hpp"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <numeric>
#include <mutex>
#include <functional>

// Prevent Windows min/max macro conflicts
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace ninacatcoin_ai {

// =====================================================
// Global state for LWMA learning — v2.0 REAL STATISTICS
// =====================================================

static const size_t MAX_HISTORY = 200;       // Keep more history for better stats
static const size_t MIN_SAMPLES = 5;         // Minimum samples before making predictions
static const double EWMA_ALPHA = 0.15;       // EWMA smoothing factor (recent data weights more)
static const double TARGET_BLOCK_TIME = 120.0;
static const double Z_SCORE_THRESHOLD = 2.5; // Standard deviations for anomaly

static std::deque<AILWMALearning::LWMASnapshot> g_difficulty_history;
static std::map<std::string, AILWMALearning::DifficultyPattern> g_patterns;
static uint64_t g_blocks_monitored = 0;
static bool g_initialized = false;
static std::mutex g_lwma_mutex;

// Running statistics (updated incrementally)
static double g_ewma_difficulty = 0.0;
static double g_ewma_block_time = TARGET_BLOCK_TIME;
static double g_ewma_change_pct = 0.0;

// =====================================================
// Internal helper: compute mean of a range
// =====================================================
static double compute_mean(const std::deque<AILWMALearning::LWMASnapshot>& history,
                           size_t window, 
                           std::function<double(const AILWMALearning::LWMASnapshot&)> extractor)
{
    if (history.empty()) return 0.0;
    size_t n = std::min(window, history.size());
    double sum = 0.0;
    for (size_t i = history.size() - n; i < history.size(); ++i) {
        sum += extractor(history[i]);
    }
    return sum / n;
}

// =====================================================
// Internal helper: compute standard deviation 
// =====================================================
static double compute_stddev(const std::deque<AILWMALearning::LWMASnapshot>& history,
                             size_t window,
                             std::function<double(const AILWMALearning::LWMASnapshot&)> extractor)
{
    if (history.size() < 2) return 0.0;
    size_t n = std::min(window, history.size());
    double mean = compute_mean(history, window, extractor);
    double sum_sq = 0.0;
    for (size_t i = history.size() - n; i < history.size(); ++i) {
        double diff = extractor(history[i]) - mean;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / (n - 1));  // Bessel's correction
}

// =====================================================
// Internal helper: z-score of latest value
// =====================================================
static double compute_zscore(const std::deque<AILWMALearning::LWMASnapshot>& history,
                             size_t window,
                             std::function<double(const AILWMALearning::LWMASnapshot&)> extractor)
{
    if (history.size() < MIN_SAMPLES) return 0.0;
    double mean = compute_mean(history, window, extractor);
    double stddev = compute_stddev(history, window, extractor);
    if (stddev < 1e-9) return 0.0;  // Avoid division by zero
    double latest = extractor(history.back());
    return (latest - mean) / stddev;
}

// =====================================================
// Initialization
// =====================================================

void AILWMALearning::initialize() {
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    
    if (g_initialized) return;
    
    g_difficulty_history.clear();
    g_patterns.clear();
    g_blocks_monitored = 0;
    g_ewma_difficulty = 0.0;
    g_ewma_block_time = TARGET_BLOCK_TIME;
    g_ewma_change_pct = 0.0;
    
    // Initialize known pattern templates
    g_patterns["stable"] = {
        "stable", 0, TARGET_BLOCK_TIME, 100.0, false
    };
    g_patterns["recovering"] = {
        "recovering", 0, 0.0, 0.0, false
    };
    g_patterns["attacking"] = {
        "attacking", 0, 0.0, 0.0, true
    };
    g_patterns["oscillating"] = {
        "oscillating", 0, 0.0, 0.0, true
    };
    g_patterns["growing"] = {
        "growing", 0, 0.0, 0.0, false
    };
    
    MGINFO("[LWMA-Learning] v2.0 initialized — real statistical analysis active");
    g_initialized = true;
}

// =====================================================
// Core Learning — REAL observation with EWMA
// =====================================================

void AILWMALearning::observe_lwma_calculation(
    uint64_t height,
    uint64_t calculated_difficulty,
    uint64_t previous_difficulty,
    uint64_t block_solve_time)
{
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    
    if (!g_initialized) {
        // Initialize inline (can't call initialize() as it also takes the lock)
        g_difficulty_history.clear();
        g_patterns.clear();
        g_blocks_monitored = 0;
        g_ewma_difficulty = 0.0;
        g_ewma_block_time = TARGET_BLOCK_TIME;
        g_ewma_change_pct = 0.0;
        g_patterns["stable"] = {"stable", 0, TARGET_BLOCK_TIME, 100.0, false};
        g_patterns["recovering"] = {"recovering", 0, 0.0, 0.0, false};
        g_patterns["attacking"] = {"attacking", 0, 0.0, 0.0, true};
        g_patterns["oscillating"] = {"oscillating", 0, 0.0, 0.0, true};
        g_patterns["growing"] = {"growing", 0, 0.0, 0.0, false};
        g_initialized = true;
    }
    
    // Calculate percentage change
    double change_percent = 0.0;
    if (previous_difficulty > 0) {
        change_percent = ((double)calculated_difficulty - (double)previous_difficulty) 
                        / (double)previous_difficulty * 100.0;
    }
    
    // Update EWMA running statistics
    if (g_blocks_monitored == 0) {
        g_ewma_difficulty = (double)calculated_difficulty;
        g_ewma_block_time = (double)block_solve_time;
        g_ewma_change_pct = change_percent;
    } else {
        g_ewma_difficulty = EWMA_ALPHA * (double)calculated_difficulty + (1.0 - EWMA_ALPHA) * g_ewma_difficulty;
        g_ewma_block_time = EWMA_ALPHA * (double)block_solve_time + (1.0 - EWMA_ALPHA) * g_ewma_block_time;
        g_ewma_change_pct = EWMA_ALPHA * change_percent + (1.0 - EWMA_ALPHA) * g_ewma_change_pct;
    }
    
    // Detect anomalies using z-score on recent history
    std::string anomaly = "";
    bool is_anomalous = false;
    
    if (g_difficulty_history.size() >= MIN_SAMPLES) {
        // Z-score of difficulty change
        auto change_extractor = [](const LWMASnapshot& s) { return s.difficulty_change_percent; };
        double z_change = compute_zscore(g_difficulty_history, 60, change_extractor);
        
        // Z-score of block time
        auto time_extractor = [](const LWMASnapshot& s) { return (double)s.block_solve_time; };
        double z_time = compute_zscore(g_difficulty_history, 60, time_extractor);
        
        if (std::abs(z_change) > Z_SCORE_THRESHOLD) {
            anomaly = (z_change > 0) ? "statistical_spike" : "statistical_drop";
            is_anomalous = true;
        } else if (std::abs(z_time) > Z_SCORE_THRESHOLD) {
            anomaly = (z_time > 0) ? "slow_block_outlier" : "fast_block_outlier";
            is_anomalous = true;
        }
    }
    
    // Also check hard limits (these are always suspicious regardless of stats)
    if (block_solve_time > 600 && anomaly.empty()) {  // 5x target
        anomaly = "extreme_slow_block";
        is_anomalous = true;
    }
    if (std::abs(change_percent) > 40.0 && anomaly.empty()) {
        anomaly = (change_percent > 0) ? "extreme_increase" : "extreme_decrease";
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
    
    if (g_difficulty_history.size() > MAX_HISTORY) {
        g_difficulty_history.pop_front();
    }
    
    if (is_anomalous) {
        MWARNING("[LWMA-Learning] Anomaly at height " << height 
                 << ": " << anomaly 
                 << " (change: " << std::fixed << std::setprecision(2) 
                 << change_percent << "%, block_time: " << block_solve_time << "s)");
    }
}

// =====================================================
// Anomaly Detection — z-score based
// =====================================================

std::string AILWMALearning::detect_difficulty_anomaly(
    uint64_t current_diff,
    uint64_t previous_diff,
    uint64_t block_time)
{
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    std::stringstream report;
    
    if (current_diff == 0 || previous_diff == 0) {
        return "INVALID: zero difficulty value";
    }
    
    double ratio = (double)current_diff / (double)previous_diff;
    double change_pct = (ratio - 1.0) * 100.0;
    
    // Statistical anomaly: use z-score against historical changes
    if (g_difficulty_history.size() >= MIN_SAMPLES) {
        auto extractor = [](const LWMASnapshot& s) { return s.difficulty_change_percent; };
        double mean = compute_mean(g_difficulty_history, 60, extractor);
        double stddev = compute_stddev(g_difficulty_history, 60, extractor);
        
        if (stddev > 1e-9) {
            double z = (change_pct - mean) / stddev;
            if (std::abs(z) > Z_SCORE_THRESHOLD) {
                report << "STATISTICAL ANOMALY: difficulty change " 
                       << std::fixed << std::setprecision(2) << change_pct 
                       << "% (z-score: " << std::setprecision(2) << z 
                       << ", mean: " << mean << "%, stddev: " << stddev << "%)";
                return report.str();
            }
        }
    }
    
    // Hard limit checks (always check even without enough history)
    if (block_time > 600) {
        report << "EXTREME SLOW BLOCK: " << block_time 
               << "s (target: " << TARGET_BLOCK_TIME << "s, " 
               << std::fixed << std::setprecision(1) 
               << (block_time / TARGET_BLOCK_TIME) << "x slower)";
        return report.str();
    }
    
    if (ratio > 1.5) {
        report << "RAPID INCREASE: +" << std::fixed << std::setprecision(1) << change_pct << "%";
        return report.str();
    }
    if (ratio < 0.67) {
        report << "RAPID DECREASE: " << std::fixed << std::setprecision(1) << change_pct << "%";
        return report.str();
    }
    
    return "";  // Normal
}

// =====================================================
// Pattern Recognition — real statistical classification
// =====================================================

std::string AILWMALearning::identify_current_pattern() {
    // Note: caller should hold lock if needed, but this is called internally
    // from generate_network_health_report which already holds the lock
    
    if (g_difficulty_history.size() < MIN_SAMPLES) {
        return "insufficient_data";
    }
    
    size_t window = std::min(size_t(30), g_difficulty_history.size());
    
    // Compute statistics over recent window
    auto time_extractor = [](const LWMASnapshot& s) { return (double)s.block_solve_time; };
    auto diff_extractor = [](const LWMASnapshot& s) { return (double)s.calculated_difficulty; };
    auto change_extractor = [](const LWMASnapshot& s) { return s.difficulty_change_percent; };
    
    double avg_time = compute_mean(g_difficulty_history, window, time_extractor);
    double stddev_time = compute_stddev(g_difficulty_history, window, time_extractor);
    double avg_change = compute_mean(g_difficulty_history, window, change_extractor);
    double stddev_change = compute_stddev(g_difficulty_history, window, change_extractor);
    
    // Coefficient of variation for block times (measure of instability)
    double cv_time = (avg_time > 1.0) ? (stddev_time / avg_time) : 0.0;
    
    // Count anomalies in window
    size_t anomalies = 0;
    for (size_t i = g_difficulty_history.size() - window; i < g_difficulty_history.size(); ++i) {
        if (g_difficulty_history[i].is_anomalous) anomalies++;
    }
    double anomaly_rate = (double)anomalies / window;
    
    // Difficulty trend (positive = growing, negative = declining)
    double trend = avg_change;
    
    // Classify using real metrics
    std::string pattern;
    
    if (anomaly_rate > 0.15) {
        // >15% of recent blocks are anomalous → under attack
        pattern = "attacking";
        g_patterns["attacking"].occurrence_count++;
        g_patterns["attacking"].average_block_time = avg_time;
        g_patterns["attacking"].stability_score = std::max(0.0, 100.0 - anomaly_rate * 200.0);
    } else if (cv_time > 0.6) {
        // High coefficient of variation → oscillating wildly
        pattern = "oscillating";
        g_patterns["oscillating"].occurrence_count++;
        g_patterns["oscillating"].average_block_time = avg_time;
        g_patterns["oscillating"].stability_score = std::max(0.0, 100.0 - cv_time * 80.0);
    } else if (avg_time > TARGET_BLOCK_TIME * 1.3) {
        // Blocks significantly slower than target → recovering from hashrate loss
        pattern = "recovering";
        g_patterns["recovering"].occurrence_count++;
        g_patterns["recovering"].average_block_time = avg_time;
        g_patterns["recovering"].stability_score = 60.0;
    } else if (trend > 1.0 && cv_time < 0.3) {
        // Steady difficulty growth with low variance → network growing
        pattern = "growing";
        g_patterns["growing"].occurrence_count++;
        g_patterns["growing"].average_block_time = avg_time;
        g_patterns["growing"].stability_score = 85.0;
    } else if (cv_time < 0.25 && std::abs(avg_time - TARGET_BLOCK_TIME) < 20.0) {
        // Low variance, close to target → stable
        pattern = "stable";
        g_patterns["stable"].occurrence_count++;
        g_patterns["stable"].average_block_time = avg_time;
        g_patterns["stable"].stability_score = std::min(100.0, 90.0 + (1.0 - cv_time) * 10.0);
    } else {
        // Default: slightly unstable but not concerning
        pattern = "stable";
        g_patterns["stable"].occurrence_count++;
        g_patterns["stable"].average_block_time = avg_time;
        g_patterns["stable"].stability_score = std::max(50.0, 80.0 - cv_time * 40.0);
    }
    
    return pattern;
}

// =====================================================
// Prediction — EWMA with trend correction
// =====================================================

std::string AILWMALearning::predict_next_difficulty(int recent_blocks) {
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    
    if (g_difficulty_history.size() < MIN_SAMPLES) {
        return "0 (insufficient data)";
    }
    
    // Method 1: EWMA prediction (smooth trend following)
    // The EWMA already implicitly predicts "next value ≈ current EWMA"
    double ewma_prediction = g_ewma_difficulty;
    
    // Method 2: Add trend correction
    // Compute recent trend (slope) using last N blocks
    size_t n = std::min((size_t)recent_blocks, g_difficulty_history.size());
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
    for (size_t i = 0; i < n; ++i) {
        size_t idx = g_difficulty_history.size() - n + i;
        double x = (double)i;
        double y = (double)g_difficulty_history[idx].calculated_difficulty;
        sum_x += x;
        sum_y += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }
    
    // Linear regression slope
    double denominator = n * sum_xx - sum_x * sum_x;
    double slope = 0.0;
    if (std::abs(denominator) > 1e-9) {
        slope = (n * sum_xy - sum_x * sum_y) / denominator;
    }
    
    // Final prediction: EWMA + 1 step of trend
    double predicted = ewma_prediction + slope;
    if (predicted < 1.0) predicted = 1.0;
    
    // Confidence: based on data quantity and prediction stability
    double data_conf = std::min(1.0, (double)g_difficulty_history.size() / 60.0);
    
    // Coefficient of variation of recent difficulties
    auto diff_extractor = [](const LWMASnapshot& s) { return (double)s.calculated_difficulty; };
    double mean_diff = compute_mean(g_difficulty_history, n, diff_extractor);
    double stddev_diff = compute_stddev(g_difficulty_history, n, diff_extractor);
    double cv = (mean_diff > 1.0) ? (stddev_diff / mean_diff) : 0.0;
    double stability_conf = std::max(0.3, 1.0 - cv * 2.0);
    
    uint8_t confidence = (uint8_t)std::min(99.0, std::max(10.0, 
        data_conf * stability_conf * 100.0));
    
    std::stringstream report;
    report << (uint64_t)predicted << " (" << (int)confidence << "% confidence"
           << ", trend: " << std::fixed << std::setprecision(1) << slope << "/block)";
    return report.str();
}

// =====================================================
// Network Analysis
// =====================================================

std::string AILWMALearning::estimate_network_hashrate() {
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    
    if (g_difficulty_history.empty()) {
        return "unknown (no data)";
    }
    
    // Use EWMA values for smoother estimation
    // For CryptoNight: hashrate ≈ difficulty / target_block_time
    // The daemon already provides solve_time data
    double effective_block_time = g_ewma_block_time;
    if (effective_block_time < 1.0) effective_block_time = TARGET_BLOCK_TIME;
    
    double estimated_rate = g_ewma_difficulty / effective_block_time;
    
    std::stringstream report;
    if (estimated_rate > 1e12) {
        report << std::fixed << std::setprecision(2) << (estimated_rate / 1e12) << " TH/s";
    } else if (estimated_rate > 1e9) {
        report << std::fixed << std::setprecision(2) << (estimated_rate / 1e9) << " GH/s";
    } else if (estimated_rate > 1e6) {
        report << std::fixed << std::setprecision(2) << (estimated_rate / 1e6) << " MH/s";
    } else if (estimated_rate > 1e3) {
        report << std::fixed << std::setprecision(2) << (estimated_rate / 1e3) << " KH/s";
    } else {
        report << std::fixed << std::setprecision(2) << estimated_rate << " H/s";
    }
    
    return report.str();
}

std::string AILWMALearning::detect_timestamp_attack(const std::vector<uint64_t>& recent_times) {
    if (recent_times.size() < 3) {
        return "";
    }
    
    // Compute mean and stddev of the provided times
    double sum = 0.0;
    for (auto t : recent_times) sum += (double)t;
    double mean = sum / recent_times.size();
    
    double sum_sq = 0.0;
    for (auto t : recent_times) {
        double diff = (double)t - mean;
        sum_sq += diff * diff;
    }
    double stddev = std::sqrt(sum_sq / (recent_times.size() - 1));
    
    // Check each time for statistical outliers
    std::vector<size_t> suspect_indices;
    for (size_t i = 0; i < recent_times.size(); ++i) {
        if (stddev > 1e-9) {
            double z = ((double)recent_times[i] - mean) / stddev;
            if (z > 3.0) {
                suspect_indices.push_back(i);
            }
        } else if (recent_times[i] > TARGET_BLOCK_TIME * 5) {
            suspect_indices.push_back(i);
        }
    }
    
    // Also check for hard limit violations
    uint64_t max_time = *std::max_element(recent_times.begin(), recent_times.end());
    
    if (!suspect_indices.empty() || max_time > 600) {
        std::stringstream report;
        report << "TIMESTAMP MANIPULATION WARNING\n"
               << "  Mean block time: " << std::fixed << std::setprecision(1) << mean << "s\n"
               << "  Std deviation: " << std::setprecision(1) << stddev << "s\n"
               << "  Max observed: " << max_time << "s\n"
               << "  Statistical outliers: " << suspect_indices.size() << " blocks\n"
               << "  Risk: Inflated solve times allow LWMA-1 to underestimate difficulty";
        return report.str();
    }
    
    return "";
}

// =====================================================
// Stability confidence — internal helpers (needed by report)
// =====================================================

static uint8_t get_stability_confidence_internal_impl(
    const std::deque<AILWMALearning::LWMASnapshot>& history)
{
    if (history.size() < MIN_SAMPLES) return 0;
    
    size_t window = std::min(size_t(60), history.size());
    
    // Factor 1: Anomaly rate (0-40 points deducted)
    uint64_t anomalies = 0;
    for (size_t i = history.size() - window; i < history.size(); ++i) {
        if (history[i].is_anomalous) anomalies++;
    }
    double anomaly_penalty = std::min(40.0, (double)anomalies / window * 100.0);
    
    // Factor 2: Block time coefficient of variation (0-30 points deducted)
    auto time_extractor = [](const AILWMALearning::LWMASnapshot& s) { return (double)s.block_solve_time; };
    double mean_time = compute_mean(history, window, time_extractor);
    double stddev_time = compute_stddev(history, window, time_extractor);
    double cv_time = (mean_time > 1.0) ? (stddev_time / mean_time) : 0.0;
    double cv_penalty = std::min(30.0, cv_time * 60.0);
    
    // Factor 3: Deviation from target block time (0-30 points deducted)
    double target_deviation = std::abs(mean_time - TARGET_BLOCK_TIME) / TARGET_BLOCK_TIME;
    double target_penalty = std::min(30.0, target_deviation * 60.0);
    
    int score = (int)(100.0 - anomaly_penalty - cv_penalty - target_penalty);
    return (uint8_t)std::max(0, std::min(100, score));
}

// Non-locking version for use within already-locked contexts
static uint8_t get_stability_confidence_internal() {
    return get_stability_confidence_internal_impl(g_difficulty_history);
}

// =====================================================
// Reporting — with real computed statistics
// =====================================================

std::string AILWMALearning::generate_network_health_report() {
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    std::stringstream report;
    
    if (!g_initialized || g_difficulty_history.empty()) {
        report << "═══════════════════════════════════════════\n"
               << "LWMA-Learning v2.0 Network Health Report\n"
               << "═══════════════════════════════════════════\n"
               << "Status: COLLECTING DATA (need " << MIN_SAMPLES << "+ blocks)";
        return report.str();
    }
    
    const auto& latest = g_difficulty_history.back();
    
    // Compute real statistics
    auto time_extractor = [](const LWMASnapshot& s) { return (double)s.block_solve_time; };
    auto diff_extractor = [](const LWMASnapshot& s) { return (double)s.calculated_difficulty; };
    
    size_t window = std::min(size_t(60), g_difficulty_history.size());
    double avg_time = compute_mean(g_difficulty_history, window, time_extractor);
    double stddev_time = compute_stddev(g_difficulty_history, window, time_extractor);
    double avg_diff = compute_mean(g_difficulty_history, window, diff_extractor);
    double stddev_diff = compute_stddev(g_difficulty_history, window, diff_extractor);
    
    // Count anomalies
    uint64_t anomaly_count = 0;
    for (const auto& snap : g_difficulty_history) {
        if (snap.is_anomalous) anomaly_count++;
    }
    double anomaly_rate = (double)anomaly_count / g_difficulty_history.size() * 100.0;
    
    std::string pattern = identify_current_pattern();
    
    report << "╔═══════════════════════════════════════════════╗\n"
           << "║   LWMA-Learning v2.0 Network Health Report    ║\n"
           << "╠═══════════════════════════════════════════════╣\n"
           << "║ Current State:\n"
           << "║   Pattern:        " << pattern << "\n"
           << "║   Height:         " << latest.block_height << "\n"
           << "║   Difficulty:     " << latest.calculated_difficulty << "\n"
           << "║   EWMA Difficulty: " << std::fixed << std::setprecision(0) << g_ewma_difficulty << "\n"
           << "║\n"
           << "║ Block Time Statistics (last " << window << " blocks):\n"
           << "║   Mean:    " << std::setprecision(1) << avg_time << "s (target: " << TARGET_BLOCK_TIME << "s)\n"
           << "║   StdDev:  " << stddev_time << "s\n"
           << "║   EWMA:    " << g_ewma_block_time << "s\n"
           << "║\n"
           << "║ Difficulty Statistics:\n"
           << "║   Mean:    " << std::setprecision(0) << avg_diff << "\n"
           << "║   StdDev:  " << stddev_diff << "\n"
           << "║   Trend:   " << std::setprecision(2) << g_ewma_change_pct << "%/block\n"
           << "║\n"
           << "║ Health Metrics:\n"
           << "║   Stability:       " << (int)get_stability_confidence_internal() << "%\n"
           << "║   Anomaly rate:    " << std::setprecision(1) << anomaly_rate << "%\n"
           << "║   Blocks monitored: " << g_blocks_monitored << "\n"
           << "║   History window:  " << g_difficulty_history.size() << " blocks\n"
           << "╚═══════════════════════════════════════════════╝";
    
    return report.str();
}

uint8_t AILWMALearning::get_stability_confidence() {
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    return get_stability_confidence_internal_impl(g_difficulty_history);
}

bool AILWMALearning::is_network_recovering() {
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    if (g_difficulty_history.size() < MIN_SAMPLES) return false;
    
    auto time_extractor = [](const LWMASnapshot& s) { return (double)s.block_solve_time; };
    double avg_time = compute_mean(g_difficulty_history, 30, time_extractor);
    
    // Recovering if average block time is significantly above target
    return avg_time > TARGET_BLOCK_TIME * 1.3;
}

std::map<std::string, AILWMALearning::DifficultyPattern> AILWMALearning::get_learned_patterns() {
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    return g_patterns;
}

void AILWMALearning::reset_learning() {
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    g_difficulty_history.clear();
    g_patterns.clear();
    g_blocks_monitored = 0;
    g_ewma_difficulty = 0.0;
    g_ewma_block_time = TARGET_BLOCK_TIME;
    g_ewma_change_pct = 0.0;
    g_initialized = false;
    MGINFO("[LWMA-Learning] Learning state reset");
    // Re-init will happen on next observe call
}

std::string AILWMALearning::get_statistics() {
    std::lock_guard<std::mutex> lock(g_lwma_mutex);
    std::stringstream stats;
    
    stats << "LWMA-Learning v2.0 Statistics:\n"
          << "  Total blocks observed: " << g_blocks_monitored << "\n"
          << "  History size: " << g_difficulty_history.size() << "/" << MAX_HISTORY << "\n"
          << "  EWMA difficulty: " << std::fixed << std::setprecision(0) << g_ewma_difficulty << "\n"
          << "  EWMA block time: " << std::setprecision(1) << g_ewma_block_time << "s\n"
          << "  EWMA trend: " << std::setprecision(2) << g_ewma_change_pct << "%/block\n"
          << "  Patterns detected:\n";
    
    for (const auto& [name, pattern] : g_patterns) {
        if (pattern.occurrence_count > 0) {
            stats << "    " << name << ": " << pattern.occurrence_count 
                  << " occurrences (stability: " << std::setprecision(0) 
                  << pattern.stability_score << ")\n";
        }
    }
    
    return stats.str();
}

} // namespace ninacatcoin_ai
