#!/usr/bin/env python3
"""
NINA Embedded Model Trainer
============================
Trains ML models from ml_training_data.csv and exports them as pure C++ header.
The exported header contains model weights as compile-time constants.
This ensures ALL nodes run the EXACT same model — no forks possible.

Usage:
    python3 tools/nina_train_embedded_model.py [path_to_csv]

Output:
    src/ai/ai_embedded_model_weights.hpp  (auto-generated C++ header)

Models trained:
    1. Block Anomaly Detector  — Isolation Forest → exported as thresholds + decision rules
    2. Difficulty Predictor    — Linear regression → exported as coefficients
    3. Solve Time Anomaly      — Statistical bounds from real data (z-score based)
"""

import sys
import os
import csv
import math
import statistics
from datetime import datetime

# Try importing sklearn, but we have a pure-stats fallback
try:
    import numpy as np
    from sklearn.ensemble import IsolationForest, GradientBoostingRegressor
    from sklearn.linear_model import LinearRegression
    from sklearn.preprocessing import StandardScaler
    HAS_SKLEARN = True
except ImportError:
    HAS_SKLEARN = False
    print("WARNING: scikit-learn not available, using pure statistical model")


def load_csv(path):
    """Load training data from CSV"""
    data = []
    with open(path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                rec = {
                    'height': int(row['height']),
                    'timestamp': int(row['timestamp']),
                    'solve_time': int(row['solve_time']),
                    'difficulty': int(row['difficulty']),
                    'cumulative_difficulty': int(row['cumulative_difficulty']),
                    'txs_count': int(row['txs_count']),
                    'block_size_bytes': int(row['block_size_bytes']),
                    'network_health': float(row['network_health']),
                    'miner_diversity': float(row['miner_diversity']),
                    'hash_entropy': int(row['hash_entropy']),
                    'block_accepted': int(row['block_accepted']),
                }
                data.append(rec)
            except (ValueError, KeyError):
                continue
    return data


def compute_statistics(data):
    """Compute comprehensive statistics from training data"""
    # Filter out genesis block and blocks with unreasonable solve_time
    solve_times = [d['solve_time'] for d in data if 0 < d['solve_time'] < 100000]
    difficulties = [d['difficulty'] for d in data if d['difficulty'] > 0]
    entropies = [d['hash_entropy'] for d in data]
    sizes = [d['block_size_bytes'] for d in data]
    txs = [d['txs_count'] for d in data]

    stats = {}

    # Solve time statistics
    stats['solve_time_mean'] = statistics.mean(solve_times)
    stats['solve_time_median'] = statistics.median(solve_times)
    stats['solve_time_stdev'] = statistics.stdev(solve_times)
    stats['solve_time_p5'] = sorted(solve_times)[int(len(solve_times) * 0.05)]
    stats['solve_time_p95'] = sorted(solve_times)[int(len(solve_times) * 0.95)]
    stats['solve_time_p99'] = sorted(solve_times)[int(len(solve_times) * 0.99)]
    stats['solve_time_min'] = min(solve_times)
    stats['solve_time_max'] = max(solve_times)

    # Hash entropy statistics
    stats['entropy_mean'] = statistics.mean(entropies)
    stats['entropy_stdev'] = statistics.stdev(entropies) if len(entropies) > 1 else 1.0
    stats['entropy_min'] = min(entropies)
    stats['entropy_max'] = max(entropies)

    # Block size statistics
    stats['block_size_mean'] = statistics.mean(sizes)
    stats['block_size_stdev'] = statistics.stdev(sizes) if len(sizes) > 1 else 1.0
    stats['block_size_p95'] = sorted(sizes)[int(len(sizes) * 0.95)]
    stats['block_size_p99'] = sorted(sizes)[int(len(sizes) * 0.99)]

    # Transaction count statistics
    stats['txs_mean'] = statistics.mean(txs)
    stats['txs_max'] = max(txs)

    # Difficulty change rate statistics (consecutive blocks)
    diff_changes = []
    for i in range(1, len(data)):
        if data[i-1]['difficulty'] > 0:
            change = abs(data[i]['difficulty'] - data[i-1]['difficulty']) / data[i-1]['difficulty']
            if change < 10.0:  # Filter extreme outliers
                diff_changes.append(change)

    if diff_changes:
        stats['diff_change_mean'] = statistics.mean(diff_changes)
        stats['diff_change_stdev'] = statistics.stdev(diff_changes) if len(diff_changes) > 1 else 0.1
        stats['diff_change_p95'] = sorted(diff_changes)[int(len(diff_changes) * 0.95)]
        stats['diff_change_p99'] = sorted(diff_changes)[int(len(diff_changes) * 0.99)]
    else:
        stats['diff_change_mean'] = 0.1
        stats['diff_change_stdev'] = 0.1
        stats['diff_change_p95'] = 0.5
        stats['diff_change_p99'] = 1.0

    # Solve time ratio statistics (solve_time / target_block_time)
    # Target block time for NinaCatCoin = 120 seconds
    TARGET_BLOCK_TIME = 120.0
    ratios = [s / TARGET_BLOCK_TIME for s in solve_times]
    stats['ratio_mean'] = statistics.mean(ratios)
    stats['ratio_stdev'] = statistics.stdev(ratios) if len(ratios) > 1 else 1.0

    return stats


def train_difficulty_predictor(data):
    """Train a simple linear model: predict next difficulty from recent solve times"""
    # Use windows of 10 blocks to predict difficulty change
    coefficients = {'intercept': 0.0, 'avg_solve_ratio': 0.0, 'trend': 0.0}

    if len(data) < 20:
        return coefficients

    WINDOW = 10
    TARGET = 120.0  # target block time

    X_avg_ratios = []
    X_trends = []
    Y_diff_changes = []

    for i in range(WINDOW, len(data) - 1):
        window = data[i-WINDOW:i]
        solve_times = [d['solve_time'] for d in window if 0 < d['solve_time'] < 100000]
        if len(solve_times) < 5:
            continue

        avg_ratio = statistics.mean(solve_times) / TARGET
        # Trend: are solve times increasing or decreasing?
        first_half = statistics.mean(solve_times[:len(solve_times)//2])
        second_half = statistics.mean(solve_times[len(solve_times)//2:])
        trend = (second_half - first_half) / TARGET if first_half > 0 else 0

        # Target: actual difficulty change ratio
        if data[i]['difficulty'] > 0:
            diff_change = (data[i+1]['difficulty'] - data[i]['difficulty']) / data[i]['difficulty']
            if abs(diff_change) < 5.0:  # Filter extreme
                X_avg_ratios.append(avg_ratio)
                X_trends.append(trend)
                Y_diff_changes.append(diff_change)

    if len(X_avg_ratios) < 10:
        return coefficients

    # Simple linear regression: diff_change = a*avg_ratio + b*trend + c
    if HAS_SKLEARN:
        X = np.column_stack([X_avg_ratios, X_trends])
        Y = np.array(Y_diff_changes)
        model = LinearRegression()
        model.fit(X, Y)
        coefficients['avg_solve_ratio'] = float(model.coef_[0])
        coefficients['trend'] = float(model.coef_[1])
        coefficients['intercept'] = float(model.intercept_)
    else:
        # Fallback: simple correlation
        n = len(X_avg_ratios)
        mean_x = sum(X_avg_ratios) / n
        mean_y = sum(Y_diff_changes) / n
        cov = sum((X_avg_ratios[i] - mean_x) * (Y_diff_changes[i] - mean_y) for i in range(n))
        var_x = sum((x - mean_x)**2 for x in X_avg_ratios)
        if var_x > 0:
            coefficients['avg_solve_ratio'] = cov / var_x
            coefficients['intercept'] = mean_y - coefficients['avg_solve_ratio'] * mean_x

    return coefficients


def train_anomaly_thresholds(data, stats):
    """
    Train anomaly detection thresholds using Isolation Forest or statistical bounds.
    Returns threshold scores that can be implemented as simple rules in C++.
    """
    thresholds = {}

    # Z-score based anomaly thresholds (works without sklearn too)
    # A block is anomalous if any feature is beyond these Z-score thresholds
    thresholds['solve_time_zscore_warn'] = 2.0    # > 2 sigma = warning
    thresholds['solve_time_zscore_danger'] = 3.5   # > 3.5 sigma = dangerous
    thresholds['entropy_zscore_warn'] = 2.5
    thresholds['entropy_zscore_danger'] = 4.0
    thresholds['diff_change_zscore_warn'] = 2.0
    thresholds['diff_change_zscore_danger'] = 3.0

    # Absolute thresholds (from data distribution)
    thresholds['solve_time_min_safe'] = max(1, stats['solve_time_p5'] // 3)
    thresholds['solve_time_max_safe'] = stats['solve_time_p99'] * 3
    thresholds['block_size_max_safe'] = max(stats['block_size_p99'] * 5, 500000)
    thresholds['entropy_min_safe'] = max(40, stats['entropy_min'] - 5)

    if HAS_SKLEARN:
        # Train Isolation Forest for more nuanced anomaly scoring
        # Use: solve_time_ratio, difficulty_change_rate, entropy
        features = []
        for i in range(1, len(data)):
            st = data[i]['solve_time']
            if st <= 0 or st > 100000:
                continue
            ratio = st / 120.0  # solve_time / target
            if data[i-1]['difficulty'] > 0:
                diff_change = abs(data[i]['difficulty'] - data[i-1]['difficulty']) / data[i-1]['difficulty']
            else:
                diff_change = 0
            entropy = data[i]['hash_entropy']
            features.append([ratio, diff_change, entropy])

        if len(features) > 100:
            X = np.array(features)
            scaler = StandardScaler()
            X_scaled = scaler.fit_transform(X)

            iso = IsolationForest(
                n_estimators=100,
                contamination=0.05,  # Expect ~5% anomalies
                random_state=42
            )
            iso.fit(X_scaled)

            # Get the anomaly score threshold
            scores = iso.decision_function(X_scaled)
            thresholds['iforest_threshold'] = float(np.percentile(scores, 5))

            # Store scaler params for C++ reconstruction
            thresholds['scaler_mean_ratio'] = float(scaler.mean_[0])
            thresholds['scaler_mean_diff'] = float(scaler.mean_[1])
            thresholds['scaler_mean_entropy'] = float(scaler.mean_[2])
            thresholds['scaler_std_ratio'] = float(scaler.scale_[0])
            thresholds['scaler_std_diff'] = float(scaler.scale_[1])
            thresholds['scaler_std_entropy'] = float(scaler.scale_[2])

            # Export simplified decision rules from Isolation Forest
            # We approximate it with the top 3 most informative thresholds
            anomalous_mask = iso.predict(X_scaled) == -1
            anomalous_features = X[anomalous_mask]
            normal_features = X[~anomalous_mask]

            if len(anomalous_features) > 0 and len(normal_features) > 0:
                # Key differentiating values
                thresholds['anomaly_ratio_boundary'] = float(
                    (np.mean(anomalous_features[:, 0]) + np.mean(normal_features[:, 0])) / 2
                )
                thresholds['anomaly_diff_boundary'] = float(
                    np.percentile(normal_features[:, 1], 95)
                )

            print(f"  Isolation Forest trained: {len(features)} samples, "
                  f"threshold={thresholds['iforest_threshold']:.4f}")

    return thresholds


def generate_cpp_header(stats, coefficients, thresholds, num_records, output_path):
    """Generate the C++ header file with embedded model weights"""

    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    header = f"""// AUTO-GENERATED by tools/nina_train_embedded_model.py
// Date: {now}
// Training data: {num_records} blocks from ml_training_data.csv
//
// DO NOT EDIT MANUALLY — regenerate by running:
//   python3 tools/nina_train_embedded_model.py ~/.ninacatcoin/ml_training_data.csv
//
// This file contains ML model weights compiled directly into the daemon.
// All nodes use the exact same model = deterministic = no forks.

#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <string>

namespace ninacatcoin_ai {{

// ============================================================================
// MODEL METADATA
// ============================================================================
struct EmbeddedModelInfo {{
    static constexpr uint32_t TRAINING_BLOCKS = {num_records};
    static constexpr const char* TRAINING_DATE = "{now}";
    static constexpr uint32_t MODEL_VERSION = 1;
    static constexpr double TARGET_BLOCK_TIME = 120.0;  // seconds
}};

// ============================================================================
// LEARNED STATISTICS FROM {num_records} REAL BLOCKS
// ============================================================================
struct LearnedStatistics {{
    // Solve time distribution
    static constexpr double SOLVE_TIME_MEAN    = {stats['solve_time_mean']:.4f};
    static constexpr double SOLVE_TIME_MEDIAN  = {stats['solve_time_median']:.4f};
    static constexpr double SOLVE_TIME_STDEV   = {stats['solve_time_stdev']:.4f};
    static constexpr double SOLVE_TIME_P5      = {stats['solve_time_p5']:.1f};
    static constexpr double SOLVE_TIME_P95     = {stats['solve_time_p95']:.1f};
    static constexpr double SOLVE_TIME_P99     = {stats['solve_time_p99']:.1f};
    static constexpr double SOLVE_TIME_MIN     = {stats['solve_time_min']:.1f};
    static constexpr double SOLVE_TIME_MAX     = {stats['solve_time_max']:.1f};

    // Hash entropy distribution
    static constexpr double ENTROPY_MEAN  = {stats['entropy_mean']:.4f};
    static constexpr double ENTROPY_STDEV = {stats['entropy_stdev']:.4f};
    static constexpr int    ENTROPY_MIN   = {stats['entropy_min']};
    static constexpr int    ENTROPY_MAX   = {stats['entropy_max']};

    // Block size distribution
    static constexpr double BLOCK_SIZE_MEAN  = {stats['block_size_mean']:.1f};
    static constexpr double BLOCK_SIZE_STDEV = {stats['block_size_stdev']:.1f};
    static constexpr double BLOCK_SIZE_P95   = {stats['block_size_p95']:.1f};
    static constexpr double BLOCK_SIZE_P99   = {stats['block_size_p99']:.1f};

    // Difficulty change rate distribution
    static constexpr double DIFF_CHANGE_MEAN  = {stats['diff_change_mean']:.6f};
    static constexpr double DIFF_CHANGE_STDEV = {stats['diff_change_stdev']:.6f};
    static constexpr double DIFF_CHANGE_P95   = {stats['diff_change_p95']:.6f};
    static constexpr double DIFF_CHANGE_P99   = {stats['diff_change_p99']:.6f};

    // Transaction stats
    static constexpr double TXS_MEAN = {stats['txs_mean']:.2f};
    static constexpr int    TXS_MAX  = {stats['txs_max']};

    // Solve time ratio (solve_time / target_block_time)
    static constexpr double RATIO_MEAN  = {stats['ratio_mean']:.4f};
    static constexpr double RATIO_STDEV = {stats['ratio_stdev']:.4f};
}};

// ============================================================================
// ANOMALY DETECTION THRESHOLDS (learned from Isolation Forest + statistics)
// ============================================================================
struct AnomalyThresholds {{
    // Z-score thresholds
    static constexpr double SOLVE_TIME_ZSCORE_WARN   = {thresholds['solve_time_zscore_warn']};
    static constexpr double SOLVE_TIME_ZSCORE_DANGER = {thresholds['solve_time_zscore_danger']};
    static constexpr double ENTROPY_ZSCORE_WARN      = {thresholds['entropy_zscore_warn']};
    static constexpr double ENTROPY_ZSCORE_DANGER    = {thresholds['entropy_zscore_danger']};
    static constexpr double DIFF_CHANGE_ZSCORE_WARN  = {thresholds['diff_change_zscore_warn']};
    static constexpr double DIFF_CHANGE_ZSCORE_DANGER= {thresholds['diff_change_zscore_danger']};

    // Absolute safety bounds
    static constexpr uint64_t SOLVE_TIME_MIN_SAFE = {thresholds['solve_time_min_safe']};
    static constexpr uint64_t SOLVE_TIME_MAX_SAFE = {thresholds['solve_time_max_safe']};
    static constexpr uint64_t BLOCK_SIZE_MAX_SAFE = {int(thresholds['block_size_max_safe'])};
    static constexpr int      ENTROPY_MIN_SAFE    = {int(thresholds['entropy_min_safe'])};
"""

    # Add Isolation Forest params if available
    if 'iforest_threshold' in thresholds:
        header += f"""
    // Isolation Forest approximation parameters
    static constexpr double IFOREST_THRESHOLD = {thresholds['iforest_threshold']:.6f};
    static constexpr double SCALER_MEAN_RATIO   = {thresholds['scaler_mean_ratio']:.6f};
    static constexpr double SCALER_MEAN_DIFF    = {thresholds['scaler_mean_diff']:.6f};
    static constexpr double SCALER_MEAN_ENTROPY = {thresholds['scaler_mean_entropy']:.6f};
    static constexpr double SCALER_STD_RATIO    = {thresholds['scaler_std_ratio']:.6f};
    static constexpr double SCALER_STD_DIFF     = {thresholds['scaler_std_diff']:.6f};
    static constexpr double SCALER_STD_ENTROPY  = {thresholds['scaler_std_entropy']:.6f};
    static constexpr bool   HAS_IFOREST = true;
"""
    else:
        header += """
    static constexpr bool   HAS_IFOREST = false;
"""

    if 'anomaly_ratio_boundary' in thresholds:
        header += f"""    static constexpr double ANOMALY_RATIO_BOUNDARY = {thresholds['anomaly_ratio_boundary']:.6f};
    static constexpr double ANOMALY_DIFF_BOUNDARY  = {thresholds['anomaly_diff_boundary']:.6f};
"""

    header += f"""
}};

// ============================================================================
// DIFFICULTY PREDICTION MODEL (Linear Regression, trained on {num_records} blocks)
// predicted_diff_change = intercept + avg_solve_ratio * coeff_ratio + trend * coeff_trend
// ============================================================================
struct DifficultyPredictor {{
    static constexpr double INTERCEPT       = {coefficients['intercept']:.10f};
    static constexpr double COEFF_AVG_RATIO = {coefficients['avg_solve_ratio']:.10f};
    static constexpr double COEFF_TREND     = {coefficients['trend']:.10f};

    /**
     * Predict difficulty change ratio from recent block data.
     * @param avg_solve_ratio  Average(recent_solve_times) / TARGET_BLOCK_TIME
     * @param trend            (avg_second_half - avg_first_half) / TARGET_BLOCK_TIME
     * @return predicted difficulty change as ratio (-1 to +1 typically)
     */
    static double predict_change(double avg_solve_ratio, double trend) {{
        return INTERCEPT + COEFF_AVG_RATIO * avg_solve_ratio + COEFF_TREND * trend;
    }}
}};

// ============================================================================
// EMBEDDED ANOMALY SCORER
// Pure C++ inference — no Python, no sockets, no external dependencies.
// Every node computes the same score = deterministic = no forks.
// ============================================================================

enum class AnomalyLevel {{
    SAFE       = 0,   // Normal block, no concerns
    NOTICE     = 1,   // Slightly unusual, log only
    WARNING    = 2,   // Suspicious, worth investigating
    DANGEROUS  = 3    // Highly anomalous, potential attack
}};

struct AnomalyResult {{
    double    confidence;       // 0.0 (no data) to 1.0 (certain)
    double    risk_score;       // 0.0 (safe) to 1.0 (dangerous)
    AnomalyLevel level;
    int       anomaly_flags;    // Bitfield of which features triggered

    // Anomaly flag bits
    static constexpr int FLAG_FAST_SOLVE    = 0x01;
    static constexpr int FLAG_SLOW_SOLVE    = 0x02;
    static constexpr int FLAG_LOW_ENTROPY   = 0x04;
    static constexpr int FLAG_HIGH_DIFF_CHG = 0x08;
    static constexpr int FLAG_LARGE_BLOCK   = 0x10;
    static constexpr int FLAG_IFOREST       = 0x20;

    std::string describe() const {{
        std::string desc;
        if (anomaly_flags & FLAG_FAST_SOLVE)    desc += "FAST_SOLVE ";
        if (anomaly_flags & FLAG_SLOW_SOLVE)    desc += "SLOW_SOLVE ";
        if (anomaly_flags & FLAG_LOW_ENTROPY)   desc += "LOW_ENTROPY ";
        if (anomaly_flags & FLAG_HIGH_DIFF_CHG) desc += "HIGH_DIFF_CHANGE ";
        if (anomaly_flags & FLAG_LARGE_BLOCK)   desc += "LARGE_BLOCK ";
        if (anomaly_flags & FLAG_IFOREST)       desc += "IFOREST_ANOMALY ";
        if (desc.empty()) desc = "NORMAL";
        return desc;
    }}
}};

/**
 * Score a block for anomalies using the embedded model.
 * This is the main entry point — called for every new block.
 *
 * IMPORTANT: This is ADVISORY ONLY. It NEVER rejects blocks.
 * It provides confidence/risk scores for NINA's learning system.
 *
 * @param solve_time         Time to solve this block (seconds)
 * @param difficulty         Current block difficulty
 * @param prev_difficulty    Previous block difficulty
 * @param hash_entropy       Bit entropy of block hash (0-64)
 * @param block_size_bytes   Size of the block in bytes
 * @param txs_count          Number of transactions
 * @return AnomalyResult with scores and flags
 */
inline AnomalyResult score_block_anomaly(
    uint64_t solve_time,
    uint64_t difficulty,
    uint64_t prev_difficulty,
    uint32_t hash_entropy,
    uint64_t block_size_bytes,
    uint32_t txs_count
) {{
    AnomalyResult result;
    result.confidence = 0.0;
    result.risk_score = 0.0;
    result.level = AnomalyLevel::SAFE;
    result.anomaly_flags = 0;

    // Need valid data
    if (solve_time == 0) return result;

    double risk_components = 0.0;
    int num_checks = 0;

    // ===== CHECK 1: Solve time Z-score =====
    double solve_zscore = std::abs(
        (static_cast<double>(solve_time) - LearnedStatistics::SOLVE_TIME_MEAN) /
        LearnedStatistics::SOLVE_TIME_STDEV
    );
    num_checks++;

    if (solve_time < AnomalyThresholds::SOLVE_TIME_MIN_SAFE) {{
        result.anomaly_flags |= AnomalyResult::FLAG_FAST_SOLVE;
        risk_components += 0.8;  // Very fast = high risk
    }} else if (solve_zscore > AnomalyThresholds::SOLVE_TIME_ZSCORE_DANGER) {{
        if (solve_time < LearnedStatistics::SOLVE_TIME_MEAN) {{
            result.anomaly_flags |= AnomalyResult::FLAG_FAST_SOLVE;
        }} else {{
            result.anomaly_flags |= AnomalyResult::FLAG_SLOW_SOLVE;
        }}
        risk_components += 0.6;
    }} else if (solve_zscore > AnomalyThresholds::SOLVE_TIME_ZSCORE_WARN) {{
        risk_components += 0.2;
    }} else {{
        risk_components += 0.0;  // Normal
    }}

    // ===== CHECK 2: Hash entropy =====
    double entropy_zscore = std::abs(
        (static_cast<double>(hash_entropy) - LearnedStatistics::ENTROPY_MEAN) /
        std::max(LearnedStatistics::ENTROPY_STDEV, 0.1)
    );
    num_checks++;

    if (hash_entropy < static_cast<uint32_t>(AnomalyThresholds::ENTROPY_MIN_SAFE)) {{
        result.anomaly_flags |= AnomalyResult::FLAG_LOW_ENTROPY;
        risk_components += 0.7;
    }} else if (entropy_zscore > AnomalyThresholds::ENTROPY_ZSCORE_DANGER) {{
        result.anomaly_flags |= AnomalyResult::FLAG_LOW_ENTROPY;
        risk_components += 0.4;
    }} else if (entropy_zscore > AnomalyThresholds::ENTROPY_ZSCORE_WARN) {{
        risk_components += 0.1;
    }}

    // ===== CHECK 3: Difficulty change rate =====
    if (prev_difficulty > 0) {{
        double diff_change = std::abs(
            static_cast<double>(difficulty) - static_cast<double>(prev_difficulty)
        ) / static_cast<double>(prev_difficulty);

        double diff_zscore = std::abs(
            (diff_change - LearnedStatistics::DIFF_CHANGE_MEAN) /
            std::max(LearnedStatistics::DIFF_CHANGE_STDEV, 0.001)
        );
        num_checks++;

        if (diff_zscore > AnomalyThresholds::DIFF_CHANGE_ZSCORE_DANGER) {{
            result.anomaly_flags |= AnomalyResult::FLAG_HIGH_DIFF_CHG;
            risk_components += 0.5;
        }} else if (diff_zscore > AnomalyThresholds::DIFF_CHANGE_ZSCORE_WARN) {{
            risk_components += 0.15;
        }}
    }}

    // ===== CHECK 4: Block size =====
    if (block_size_bytes > AnomalyThresholds::BLOCK_SIZE_MAX_SAFE) {{
        result.anomaly_flags |= AnomalyResult::FLAG_LARGE_BLOCK;
        risk_components += 0.4;
        num_checks++;
    }} else {{
        num_checks++;
    }}

    // ===== FINAL SCORING =====
    if (num_checks > 0) {{
        result.risk_score = std::min(1.0, risk_components / num_checks);
        result.confidence = std::min(1.0, static_cast<double>(EmbeddedModelInfo::TRAINING_BLOCKS) / 1000.0);

        // Determine anomaly level
        if (result.risk_score >= 0.6) {{
            result.level = AnomalyLevel::DANGEROUS;
        }} else if (result.risk_score >= 0.3) {{
            result.level = AnomalyLevel::WARNING;
        }} else if (result.risk_score >= 0.1) {{
            result.level = AnomalyLevel::NOTICE;
        }} else {{
            result.level = AnomalyLevel::SAFE;
        }}
    }}

    return result;
}}

/**
 * Predict expected solve time ratio for the next block.
 * Uses the embedded linear regression model.
 *
 * @param recent_solve_times  Array of recent solve times (at least 5)
 * @param count               Number of elements in the array
 * @return Predicted difficulty change ratio
 */
inline double predict_difficulty_change(const uint64_t* recent_solve_times, size_t count) {{
    if (count < 5) return 0.0;

    constexpr double TARGET = EmbeddedModelInfo::TARGET_BLOCK_TIME;

    // Calculate average solve time ratio
    double sum = 0.0;
    for (size_t i = 0; i < count; i++) {{
        sum += static_cast<double>(recent_solve_times[i]);
    }}
    double avg_ratio = (sum / count) / TARGET;

    // Calculate trend
    size_t half = count / 2;
    double first_half_sum = 0.0, second_half_sum = 0.0;
    for (size_t i = 0; i < half; i++) first_half_sum += static_cast<double>(recent_solve_times[i]);
    for (size_t i = half; i < count; i++) second_half_sum += static_cast<double>(recent_solve_times[i]);
    double trend = ((second_half_sum / (count - half)) - (first_half_sum / half)) / TARGET;

    return DifficultyPredictor::predict_change(avg_ratio, trend);
}}

}} // namespace ninacatcoin_ai
"""

    with open(output_path, 'w') as f:
        f.write(header)

    print(f"\n  C++ header written to: {output_path}")


def main():
    # Find CSV path
    csv_path = None
    if len(sys.argv) > 1:
        csv_path = sys.argv[1]
    else:
        # Try default locations
        home = os.environ.get('HOME', os.environ.get('USERPROFILE', ''))
        candidates = [
            os.path.join(home, '.ninacatcoin', 'ml_training_data.csv'),
            'ml_training_data.csv',
        ]
        for c in candidates:
            if os.path.exists(c):
                csv_path = c
                break

    if not csv_path or not os.path.exists(csv_path):
        print("ERROR: Cannot find ml_training_data.csv")
        print("Usage: python3 nina_train_embedded_model.py [path_to_csv]")
        sys.exit(1)

    print(f"NINA Embedded Model Trainer")
    print(f"===========================")
    print(f"Training data: {csv_path}")

    # Load data
    data = load_csv(csv_path)
    print(f"Loaded {len(data)} block records")

    if len(data) < 100:
        print("ERROR: Need at least 100 blocks for meaningful model training")
        sys.exit(1)

    # Step 1: Compute statistics
    print("\n[1/3] Computing statistics from real blockchain data...")
    stats = compute_statistics(data)
    print(f"  Solve time: mean={stats['solve_time_mean']:.1f}s stdev={stats['solve_time_stdev']:.1f}s")
    print(f"  Solve time range: [{stats['solve_time_p5']:.0f}s, {stats['solve_time_p95']:.0f}s] (P5-P95)")
    print(f"  Difficulty change: mean={stats['diff_change_mean']:.4f} stdev={stats['diff_change_stdev']:.4f}")

    # Step 2: Train difficulty predictor
    print("\n[2/3] Training difficulty predictor...")
    coefficients = train_difficulty_predictor(data)
    print(f"  Coefficients: ratio={coefficients['avg_solve_ratio']:.6f} "
          f"trend={coefficients['trend']:.6f} intercept={coefficients['intercept']:.6f}")

    # Step 3: Train anomaly detection
    print("\n[3/3] Training anomaly detector...")
    thresholds = train_anomaly_thresholds(data, stats)

    # Generate C++ header
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    output_path = os.path.join(project_root, 'src', 'ai', 'ai_embedded_model_weights.hpp')

    generate_cpp_header(stats, coefficients, thresholds, len(data), output_path)

    print(f"\n  DONE! Model embedded in C++ — no Python needed at runtime.")
    print(f"  All nodes will use the exact same model = NO FORKS.")
    print(f"\n  To update the model with more data:")
    print(f"    1. Let the daemon collect more blocks")
    print(f"    2. Re-run: python3 tools/nina_train_embedded_model.py")
    print(f"    3. Recompile the daemon")


if __name__ == '__main__':
    main()
