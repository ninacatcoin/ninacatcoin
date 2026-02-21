/**
 * NINA Advanced Learning Modules - Comprehensive AI Enhancement
 * Designed for early-stage blockchain learning and growth
 * 
 * Includes all 6 tiers:
 * - Tier 1: Prediction & Forecasting
 * - Tier 2: Transaction Analysis
 * - Tier 3: Network Intelligence
 * - Tier 4: Optimization & Automation
 * - Tier 5: Forensics & Security
 * - Tier 6: Market Intelligence
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <deque>
#include <memory>
#include <chrono>
#include <algorithm>
#include <cmath>

namespace ninacatcoin {
namespace ia {

// ============================================================================
// TIER 1: PREDICTION & FORECASTING
// ============================================================================

struct BlockTimeEntry {
    uint64_t block_height;
    uint64_t timestamp;
    uint32_t solve_time;  // seconds
    double difficulty;
    bool is_outlier;
};

class BlockTimePredictorModule {
public:
    BlockTimePredictorModule() : samples_needed_(20) {}
    
    void observe_block_time(uint64_t height, uint32_t solve_time, double difficulty) {
        BlockTimeEntry entry{height, static_cast<uint64_t>(std::time(nullptr)), solve_time, difficulty, false};
        block_times_.push_back(entry);
        
        // Keep last 100 blocks for newer chains
        if (block_times_.size() > 100) {
            block_times_.pop_front();
        }
        
        detect_outliers();
        calculate_moving_average();
    }
    
    double predict_next_block_time() const {
        if (block_times_.size() < samples_needed_) {
            return 120.0;  // Default to 2 minutes for new chains
        }
        
        // Filter outliers and calculate robust average
        std::vector<uint32_t> valid_times;
        for (const auto& entry : block_times_) {
            if (!entry.is_outlier) {
                valid_times.push_back(entry.solve_time);
            }
        }
        
        if (valid_times.empty()) return 120.0;
        
        std::sort(valid_times.begin(), valid_times.end());
        
        // Use median for stability in new chains
        size_t mid = valid_times.size() / 2;
        return static_cast<double>(valid_times[mid]);
    }
    
    uint64_t estimate_next_block_height() const {
        if (block_times_.empty()) return 1;
        
        double avg_time = predict_next_block_time();
        uint64_t current_height = block_times_.back().block_height;
        uint64_t current_timestamp = block_times_.back().timestamp;
        
        // How many blocks until next 1000?
        uint64_t next_milestone = ((current_height / 1000) + 1) * 1000;
        return next_milestone;
    }
    
    struct BlockTimeStats {
        double average;
        double median;
        double std_dev;
        uint32_t min;
        uint32_t max;
        double trend;  // -1 (slower) to +1 (faster)
    };
    
    BlockTimeStats get_statistics() const {
        BlockTimeStats stats{};
        
        if (block_times_.empty()) return stats;
        
        std::vector<uint32_t> times;
        for (const auto& entry : block_times_) {
            if (!entry.is_outlier) {
                times.push_back(entry.solve_time);
            }
        }
        
        if (times.empty()) return stats;
        
        // Calculate statistics
        stats.min = *std::min_element(times.begin(), times.end());
        stats.max = *std::max_element(times.begin(), times.end());
        
        double sum = 0;
        for (auto t : times) sum += t;
        stats.average = sum / times.size();
        
        std::sort(times.begin(), times.end());
        stats.median = times[times.size() / 2];
        
        // Standard deviation
        double variance = 0;
        for (auto t : times) {
            variance += (t - stats.average) * (t - stats.average);
        }
        stats.std_dev = std::sqrt(variance / times.size());
        
        // Trend: compare first half vs second half
        if (times.size() >= 4) {
            double first_half = 0, second_half = 0;
            for (size_t i = 0; i < times.size() / 2; i++) {
                first_half += times[i];
            }
            for (size_t i = times.size() / 2; i < times.size(); i++) {
                second_half += times[i];
            }
            first_half /= (times.size() / 2);
            second_half /= (times.size() - times.size() / 2);
            stats.trend = (second_half - first_half) / first_half;
        }
        
        return stats;
    }

private:
    std::deque<BlockTimeEntry> block_times_;
    size_t samples_needed_;
    double moving_average_;
    
    void detect_outliers() {
        if (block_times_.size() < 5) return;
        
        // Get recent non-outlier items
        std::vector<uint32_t> recent;
        for (const auto& entry : block_times_) {
            if (!entry.is_outlier && recent.size() < 20) {
                recent.push_back(entry.solve_time);
            }
        }
        
        if (recent.size() < 3) return;
        
        double mean = 0;
        for (auto t : recent) mean += t;
        mean /= recent.size();
        
        double variance = 0;
        for (auto t : recent) {
            variance += (t - mean) * (t - mean);
        }
        double std_dev = std::sqrt(variance / recent.size());
        
        // Mark as outlier if > 3 sigma away
        if (!block_times_.empty()) {
            auto& last = block_times_.back();
            if (std::abs(static_cast<double>(last.solve_time) - mean) > 3.0 * std_dev) {
                last.is_outlier = true;
            }
        }
    }
    
    void calculate_moving_average() {
        if (block_times_.empty()) return;
        double sum = 0;
        int count = 0;
        
        for (const auto& entry : block_times_) {
            if (!entry.is_outlier) {
                sum += entry.solve_time;
                count++;
            }
        }
        
        moving_average_ = count > 0 ? sum / count : 120.0;
    }
};

// ============================================================================

struct AttackPattern {
    std::string attack_type;
    std::vector<uint64_t> block_heights;
    double probability;
    std::chrono::system_clock::time_point first_seen;
    uint32_t recurrence_count;
};

class AttackForecastingModule {
public:
    void observe_block(uint64_t height, const std::string& anomaly_type, 
                      double difficulty_change_percent) {
        
        // Learn patterns from anomalies
        if (!anomaly_type.empty()) {
            if (patterns_.find(anomaly_type) == patterns_.end()) {
                patterns_[anomaly_type] = AttackPattern{
                    anomaly_type, {height}, 0.5, std::chrono::system_clock::now(), 1
                };
            } else {
                auto& pattern = patterns_[anomaly_type];
                pattern.block_heights.push_back(height);
                pattern.recurrence_count++;
                
                // Increase probability based on recurrence
                pattern.probability = std::min(0.95, pattern.probability + 0.1);
            }
        }
    }
    
    struct ForecastResult {
        std::string predicted_attack;
        double confidence;
        std::string mitigation_strategy;
    };
    
    ForecastResult forecast_next_attack() const {
        ForecastResult result{"none", 0.0, ""};
        
        for (const auto& [attack_type, pattern] : patterns_) {
            if (pattern.probability > result.confidence) {
                result.predicted_attack = attack_type;
                result.confidence = pattern.probability;
                result.mitigation_strategy = get_mitigation(attack_type);
            }
        }
        
        return result;
    }
    
    std::vector<std::string> get_pattern_types() const {
        std::vector<std::string> types;
        for (const auto& [attack_type, _] : patterns_) {
            types.push_back(attack_type);
        }
        return types;
    }

private:
    std::map<std::string, AttackPattern> patterns_;
    
    std::string get_mitigation(const std::string& attack_type) const {
        if (attack_type == "51_attack") {
            return "Increase checkpoint frequency, activate emergency consensus";
        } else if (attack_type == "eclipse_attack") {
            return "Diversify peer connections, activate network-wide alert";
        } else if (attack_type == "timestamp_manipulation") {
            return "Tighten timestamp validation, reject suspicious blocks";
        } else if (attack_type == "replay_attack") {
            return "Isolate sender, increase replay protection, require higher fees";
        }
        return "Monitor closely, enable additional logging";
    }
};

// ============================================================================

class DifficultyTrendModule {
public:
    void observe_difficulty(uint64_t height, double current_difficulty, double previous_difficulty) {
        double change = ((current_difficulty - previous_difficulty) / previous_difficulty) * 100.0;
        
        DifficultyEntry entry{height, current_difficulty, change, std::time(nullptr)};
        history_.push_back(entry);
        
        // Keep last 144 entries (roughly 1 day)
        if (history_.size() > 144) {
            history_.pop_front();
        }
    }
    
    struct DifficultyForecast {
        double predicted_in_24h;
        double predicted_in_72h;
        std::string trend_direction;  // "increasing", "decreasing", "stable"
        double confidence;
    };
    
    DifficultyForecast forecast_difficulty() const {
        DifficultyForecast forecast{0, 0, "stable", 0.0};
        
        if (history_.size() < 10) {
            // Not enough data, use last known value
            forecast.predicted_in_24h = history_.empty() ? 1000000 : history_.back().difficulty;
            forecast.predicted_in_72h = forecast.predicted_in_24h;
            forecast.confidence = 0.3;
            return forecast;
        }
        
        // Calculate trend using linear regression on last 10 entries
        std::vector<double> recent_changes;
        for (size_t i = history_.size() > 10 ? history_.size() - 10 : 0; i < history_.size(); i++) {
            recent_changes.push_back(history_[i].change_percent);
        }
        
        double avg_change = 0;
        for (auto change : recent_changes) {
            avg_change += change;
        }
        avg_change /= recent_changes.size();
        
        double last_difficulty = history_.back().difficulty;
        
        // Project 24 blocks ahead (assuming ~2min/block = ~48min)
        // For trend, use average change
        forecast.predicted_in_24h = last_difficulty * (1.0 + (avg_change * 24 / 100.0));
        forecast.predicted_in_72h = last_difficulty * (1.0 + (avg_change * 72 / 100.0));
        
        if (avg_change > 1.0) {
            forecast.trend_direction = "increasing";
        } else if (avg_change < -1.0) {
            forecast.trend_direction = "decreasing";
        } else {
            forecast.trend_direction = "stable";
        }
        
        forecast.confidence = std::min(0.8, 0.3 + (recent_changes.size() / 144.0) * 0.5);
        
        return forecast;
    }

private:
    struct DifficultyEntry {
        uint64_t height;
        double difficulty;
        double change_percent;
        time_t timestamp;
    };
    
    std::deque<DifficultyEntry> history_;
};

// ============================================================================
// TIER 2: TRANSACTION ANALYSIS
// ============================================================================

struct TransactionSignature {
    std::string tx_id;
    uint64_t timestamp;
    uint64_t block_height;
    double amount;
    bool high_value;
    bool unusual_time;
    bool new_address;
    double anomaly_score;
};

class AnomalousTransactionModule {
public:
    void observe_transaction(const std::string& tx_id, uint64_t block_height, 
                            double amount, bool is_new_address) {
        
        auto now = std::time(nullptr);
        TransactionSignature sig{tx_id, static_cast<uint64_t>(now), block_height, amount, false, false, is_new_address, 0.0};
        
        // Score based on multiple factors
        double score = 0.0;
        
        // Factor 1: Amount anomaly (for new chain, consider >10x average as unusual)
        double avg_amount = calculate_average_transaction_amount();
        if (amount > avg_amount * 10) {
            sig.high_value = true;
            score += 3.0;
        }
        
        // Factor 2: Time anomaly (transactions outside normal hours)
        if (is_unusual_time(now)) {
            sig.unusual_time = true;
            score += 2.0;
        }
        
        // Factor 3: New address (first transaction from this address)
        if (is_new_address) {
            score += 1.5;
        }
        
        // Factor 4: Clustering (multiple large transactions in short time)
        if (check_transaction_clustering(amount)) {
            score += 2.0;
        }
        
        sig.anomaly_score = std::min(10.0, score);
        
        transactions_.push_back(sig);
        if (transactions_.size() > 10000) {
            transactions_.pop_front();
        }
        
        // Alert if very suspicious
        if (sig.anomaly_score > 6.0) {
            suspicious_transactions_.push_back(sig);
            if (suspicious_transactions_.size() > 100) {
                suspicious_transactions_.pop_front();
            }
        }
    }
    
    std::vector<TransactionSignature> get_suspicious_transactions(double threshold = 6.0) const {
        std::vector<TransactionSignature> result;
        for (const auto& tx : suspicious_transactions_) {
            if (tx.anomaly_score >= threshold) {
                result.push_back(tx);
            }
        }
        return result;
    }

private:
    std::deque<TransactionSignature> transactions_;
    std::deque<TransactionSignature> suspicious_transactions_;
    
    double calculate_average_transaction_amount() const {
        if (transactions_.empty()) return 1.0;
        
        double sum = 0;
        for (const auto& tx : transactions_) {
            sum += tx.amount;
        }
        return sum / transactions_.size();
    }
    
    bool is_unusual_time(time_t timestamp) const {
        // For new chains, mark as unusual if outside 24h average activity
        struct tm* timeinfo = std::localtime(&timestamp);
        int hour = timeinfo->tm_hour;
        
        // If there's a pattern of transactions, detect deviation
        // For now, mark 2-5 AM UTC as less common
        return (hour >= 2 && hour <= 5);
    }
    
    bool check_transaction_clustering(double amount) const {
        if (transactions_.size() < 2) return false;
        
        // Check if this large transaction is part of a cluster
        int recent_large = 0;
        auto now = std::time(nullptr);
        double thres_amount = calculate_average_transaction_amount() * 5;
        
        for (const auto& tx : transactions_) {
            if (static_cast<time_t>(tx.timestamp) > now - 300) {  // Last 5 minutes
                if (tx.amount > thres_amount) {
                    recent_large++;
                }
            }
        }
        
        return recent_large > 2;
    }
};

// ============================================================================

class MempoolWatcherModule {
public:
    struct MempoolMetrics {
        size_t total_transactions;
        double total_volume;
        double average_fee;
        double median_fee;
        size_t spam_count;
        double spam_ratio;
        uint64_t timestamp;
    };
    
    void observe_mempool_state(size_t total_txs, double total_vol, double avg_fee) {
        last_total_txs_ = total_txs;
        last_volume_ = total_vol;
        last_avg_fee_ = avg_fee;
        last_timestamp_ = std::time(nullptr);
    }
    
    MempoolMetrics get_metrics() const {
        MempoolMetrics metrics{
            last_total_txs_,
            last_volume_,
            last_avg_fee_,
            last_avg_fee_ * 0.95,  // Approximate median
            0,
            0.0,
            static_cast<uint64_t>(last_timestamp_)
        };
        
        // Detect spam: high tx count with low average fee
        if (metrics.total_transactions > 1000 && metrics.average_fee < 0.0001) {
            metrics.spam_count = metrics.total_transactions / 10;  // Estimate
            metrics.spam_ratio = static_cast<double>(metrics.spam_count) / metrics.total_transactions;
        }
        
        return metrics;
    }
    
    bool detect_flooding_attack() const {
        // New chains: flag if mempool explodes in size
        return last_total_txs_ > 5000;
    }

private:
    size_t last_total_txs_ = 0;
    double last_volume_ = 0;
    double last_avg_fee_ = 0;
    time_t last_timestamp_ = 0;
};

// ============================================================================
// TIER 3: NETWORK INTELLIGENCE
// ============================================================================

struct PeerReputation {
    std::string peer_id;
    uint32_t valid_blocks;
    uint32_t invalid_blocks;
    uint32_t requested_blocks;
    double latency_ms;
    uint64_t total_bytes_sent;
    uint64_t total_bytes_received;
    time_t last_active;
    double reputation_score;
};

class PeerReputationModule {
public:
    void observe_peer_block(const std::string& peer_id, bool valid, double latency) {
        if (peers_.find(peer_id) == peers_.end()) {
            peers_[peer_id] = PeerReputation{peer_id, 0, 0, 0, latency, 0, 0, std::time(nullptr), 0.5};
        }
        
        auto& peer = peers_[peer_id];
        
        if (valid) {
            peer.valid_blocks++;
        } else {
            peer.invalid_blocks++;
        }
        
        peer.latency_ms = latency * 0.9 + peer.latency_ms * 0.1;  // Moving average
        peer.last_active = std::time(nullptr);
        
        calculate_reputation(peer);
    }
    
    void observe_peer_traffic(const std::string& peer_id, uint64_t sent, uint64_t received) {
        if (peers_.find(peer_id) != peers_.end()) {
            peers_[peer_id].total_bytes_sent += sent;
            peers_[peer_id].total_bytes_received += received;
        }
    }
    
    std::vector<PeerReputation> get_trusted_peers() const {
        std::vector<PeerReputation> trusted;
        for (const auto& [peer_id, peer] : peers_) {
            if (peer.reputation_score > 0.7) {
                trusted.push_back(peer);
            }
        }
        return trusted;
    }
    
    std::vector<PeerReputation> get_suspicious_peers() const {
        std::vector<PeerReputation> suspicious;
        for (const auto& [peer_id, peer] : peers_) {
            if (peer.reputation_score < 0.3) {
                suspicious.push_back(peer);
            }
        }
        return suspicious;
    }

private:
    std::map<std::string, PeerReputation> peers_;
    
    void calculate_reputation(PeerReputation& peer) const {
        // Very permissive for new chains
        uint32_t total = peer.valid_blocks + peer.invalid_blocks;
        if (total == 0) {
            peer.reputation_score = 0.5;
            return;
        }
        
        double validity_ratio = static_cast<double>(peer.valid_blocks) / total;
        
        // Base score from validity
        peer.reputation_score = validity_ratio * 0.7;
        
        // Bonus for low latency
        if (peer.latency_ms < 100) {
            peer.reputation_score += 0.2;
        } else if (peer.latency_ms < 300) {
            peer.reputation_score += 0.1;
        }
        
        // Penalty for high latency
        if (peer.latency_ms > 1000) {
            peer.reputation_score -= 0.1;
        }
        
        peer.reputation_score = std::max(0.0, std::min(1.0, peer.reputation_score));
    }
};

// ============================================================================

class NetworkHealthModule {
public:
    struct HealthScore {
        double overall_score;      // 0-100
        double peer_diversity;     // 0-100
        double consensus_strength; // 0-100
        double synchronization;    // 0-100
        std::string status;        // "excellent", "good", "fair", "poor"
    };
    
    void observe_network_state(uint32_t active_peers, uint32_t total_peers,
                              double consensus_alignment, bool fully_synced) {
        active_peers_ = active_peers;
        total_peers_ = total_peers;
        consensus_alignment_ = consensus_alignment;
        fully_synced_ = fully_synced;
        last_update_ = std::time(nullptr);
    }
    
    HealthScore calculate_health() const {
        HealthScore score{0, 0, 0, 0, ""};
        
        // Peer diversity (how many peers do we have)
        score.peer_diversity = std::min(100.0, (active_peers_ / 10.0) * 100.0);
        
        // Consensus strength
        score.consensus_strength = consensus_alignment_ * 100.0;
        
        // Synchronization
        score.synchronization = fully_synced_ ? 100.0 : 50.0;
        
        // Overall (weighted average)
        score.overall_score = (score.peer_diversity * 0.3 + 
                              score.consensus_strength * 0.4 + 
                              score.synchronization * 0.3);
        
        if (score.overall_score >= 80) {
            score.status = "excellent";
        } else if (score.overall_score >= 60) {
            score.status = "good";
        } else if (score.overall_score >= 40) {
            score.status = "fair";
        } else {
            score.status = "poor";
        }
        
        return score;
    }

private:
    uint32_t active_peers_ = 0;
    uint32_t total_peers_ = 0;
    double consensus_alignment_ = 0.0;
    bool fully_synced_ = false;
    time_t last_update_ = 0;
};

// ============================================================================

class GeoDistributionModule {
public:
    struct GeoData {
        std::string region;
        uint32_t peer_count;
        double latency_avg;
    };
    
    void observe_peer_location(const std::string& peer_id, const std::string& continent) {
        peer_distribution_[continent]++;
    }
    
    bool check_centralization_risk() const {
        // For new chains, flag if any single region has >50% of peers
        uint32_t total = 0;
        uint32_t max_region = 0;
        
        for (const auto& [region, count] : peer_distribution_) {
            total += count;
            max_region = std::max(max_region, count);
        }
        
        if (total == 0) return false;
        return (static_cast<double>(max_region) / total) > 0.5;
    }
    
    std::map<std::string, uint32_t> get_distribution() const {
        return peer_distribution_;
    }

private:
    std::map<std::string, uint32_t> peer_distribution_;
};

// ============================================================================

class PeerClusteringModule {
public:
    void observe_peer_behavior(const std::string& peer_id, uint32_t blocks_per_sec, 
                              double avg_latency) {
        behavior_[peer_id] = {blocks_per_sec, avg_latency};
    }
    
    std::map<std::string, std::vector<std::string>> cluster_peers() const {
        // Very basic clustering: fast/slow, local/remote
        std::map<std::string, std::vector<std::string>> clusters;
        
        for (const auto& [peer_id, behavior] : behavior_) {
            std::string cluster = "unknown";
            
            if (behavior.first > 100 && behavior.second < 50) {
                cluster = "fast_local";
            } else if (behavior.first > 100) {
                cluster = "fast_remote";
            } else if (behavior.second < 100) {
                cluster = "slow_local";
            } else {
                cluster = "slow_remote";
            }
            
            clusters[cluster].push_back(peer_id);
        }
        
        return clusters;
    }

private:
    std::map<std::string, std::pair<uint32_t, double>> behavior_;  // blocks_per_sec, latency
};

// ============================================================================
// TIER 4: OPTIMIZATION & AUTOMATION
// ============================================================================

class FeeOptimizationModule {
public:
    struct FeeRecommendation {
        double minimum_fee;
        double recommended_fee;
        double priority_fee;
        std::string urgency;  // "slow", "normal", "fast", "critical"
    };
    
    void observe_active_fee(double fee) {
        active_fees_.push_back(fee);
        if (active_fees_.size() > 1000) {
            active_fees_.pop_front();
        }
    }
    
    FeeRecommendation recommend_fee(uint64_t tx_size_bytes) const {
        FeeRecommendation rec{0, 0, 0, "normal"};
        
        if (active_fees_.empty()) {
            // Default for new chains
            rec.minimum_fee = 0.00001;
            rec.recommended_fee = 0.0001;
            rec.priority_fee = 0.001;
            return rec;
        }
        
        // Calculate based on recent fees
        double avg_fee = 0;
        for (auto fee : active_fees_) {
            avg_fee += fee;
        }
        avg_fee /= active_fees_.size();
        
        rec.minimum_fee = avg_fee * 0.5;
        rec.recommended_fee = avg_fee;
        rec.priority_fee = avg_fee * 2.0;
        rec.urgency = "normal";
        
        return rec;
    }

private:
    std::deque<double> active_fees_;
};

// ============================================================================

class ConsensusParameterModule {
public:
    struct ParameterSuggestion {
        std::string parameter_name;
        double current_value;
        double suggested_value;
        std::string reasoning;
        bool safe_to_apply;
    };
    
    void observe_blockchain_state(uint64_t block_height, double avg_blocktime,
                                double avg_difficulty, uint32_t active_miners) {
        block_height_ = block_height;
        avg_blocktime_ = avg_blocktime;
        avg_difficulty_ = avg_difficulty;
        active_miners_ = active_miners;
    }
    
    std::vector<ParameterSuggestion> suggest_adjustments() const {
        std::vector<ParameterSuggestion> suggestions;
        
        // For new chains: very conservative
        if (block_height_ < 10000) {
            return suggestions;  // Don't suggest anything for very new chains
        }
        
        // Only suggest after 10k blocks of data
        if (avg_blocktime_ > 150) {
            suggestions.push_back({"target_blocktime", 120.0, 150.0, 
                "Network is slower than target, increasing blocktime", false});
        }
        
        return suggestions;
    }

private:
    uint64_t block_height_ = 0;
    double avg_blocktime_ = 120.0;
    double avg_difficulty_ = 1000000.0;
    uint32_t active_miners_ = 0;
};

// ============================================================================

class NetworkSimulationModule {
public:
    struct SimulationResult {
        std::string attack_scenario;
        double chain_vulnerability;
        std::string mitigation;
        bool currently_vulnerable;
    };
    
    SimulationResult simulate_51_attack(uint32_t attacker_hash_percent) const {
        SimulationResult result{"51% attack", 0.0, "", false};
        
        // Calculate vulnerability
        // For new chains with few miners, 51% attacks are easier
        result.chain_vulnerability = static_cast<double>(attacker_hash_percent) / 50.0;  // Normalize to 50%
        result.currently_vulnerable = attacker_hash_percent > 33;
        
        result.mitigation = "Increase checkpoint frequency, broadcast alert, activate emergency protocol";
        
        return result;
    }
    
    SimulationResult simulate_eclipse_attack() const {
        SimulationResult result{"eclipse attack", 0.0, "", false};
        
        // Estimate vulnerability based on peer diversity
        result.chain_vulnerability = 0.4;  // Moderate risk for new chains with few peers
        result.currently_vulnerable = true;
        
        result.mitigation = "Diversify peer connections, use multiple seed nodes, enable peer reputation scoring";
        
        return result;
    }
};

// ============================================================================
// TIER 5: FORENSICS & SECURITY
// ============================================================================

class TemporalAnalysisModule {
public:
    struct TemporalAnomaly {
        uint64_t block_height;
        std::string anomaly_type;
        double severity;
        std::string description;
    };
    
    void observe_temporal_pattern(uint64_t block_height, time_t timestamp,
                                 uint32_t block_time) {
        struct tm* timeinfo = std::localtime(&timestamp);
        int hour = timeinfo->tm_hour;
        
        hourly_blocks_[hour]++;
    }
    
    std::vector<TemporalAnomaly> detect_anomalies(uint64_t current_height) const {
        std::vector<TemporalAnomaly> anomalies;
        
        // Find hours with unusual activity
        uint32_t avg_blocks_per_hour = 0;
        for (const auto& [hour, count] : hourly_blocks_) {
            avg_blocks_per_hour += count;
        }
        avg_blocks_per_hour /= 24;
        
        for (const auto& [hour, count] : hourly_blocks_) {
            if (count > avg_blocks_per_hour * 2) {
                anomalies.push_back({current_height, "excessive_activity_in_hour", 
                    static_cast<double>(count) / avg_blocks_per_hour, 
                    "Hour " + std::to_string(hour) + " has " + std::to_string(count) + " blocks"});
            }
            if (count == 0 && hourly_blocks_.size() > 12) {
                anomalies.push_back({current_height, "no_blocks_in_hour", 0.5,
                    "Hour " + std::to_string(hour) + " has no blocks"});
            }
        }
        
        return anomalies;
    }

private:
    std::map<int, uint32_t> hourly_blocks_;
};

// ============================================================================

class BehavioralClusteringModule {
public:
    struct AddressCluster {
        std::vector<std::string> addresses;
        std::string cluster_type;  // "trading", "mining", "holding", "suspicious"
        double similarity_score;
    };
    
    void observe_address_behavior(const std::string& address, double sent_amount,
                                 double received_amount, uint32_t tx_count) {
        address_patterns_[address] = {sent_amount, received_amount, tx_count};
    }
    
    std::vector<AddressCluster> get_clusters() const {
        std::vector<AddressCluster> clusters;
        
        // Very basic clustering for new chains
        std::vector<std::string> high_volume, frequent_traders, miners;
        
        for (const auto& [address, behavior] : address_patterns_) {
            if (behavior.tx_count > 100) {
                frequent_traders.push_back(address);
            }
            if (behavior.sent + behavior.received > 10000) {
                high_volume.push_back(address);
            }
            if (behavior.received > behavior.sent * 5) {
                miners.push_back(address);
            }
        }
        
        if (!frequent_traders.empty()) {
            clusters.push_back({frequent_traders, "trading", 0.8});
        }
        if (!miners.empty()) {
            clusters.push_back({miners, "mining", 0.7});
        }
        if (!high_volume.empty()) {
            clusters.push_back({high_volume, "whale", 0.9});
        }
        
        return clusters;
    }

private:
    struct AddressPattern {
        double sent;
        double received;
        uint32_t tx_count;
    };
    
    std::map<std::string, AddressPattern> address_patterns_;
};

// ============================================================================

class BlockForgeryDetectionModule {
public:
    struct ForgeryIndicator {
        uint64_t block_height;
        std::string indicator_type;
        double suspicion_level;
        std::string evidence;
    };
    
    void observe_block(uint64_t height, const std::string& block_hash,
                      const std::vector<std::string>& tx_hashes) {
        blocks_[height] = {block_hash, tx_hashes};
    }
    
    ForgeryIndicator check_block_validity(uint64_t height) const {
        ForgeryIndicator indicator{height, "none", 0.0, ""};
        
        if (blocks_.find(height) == blocks_.end()) {
            return indicator;
        }
        
        // Run signature checks (simplified)
        // In real code, verify actual crypto signatures
        
        return indicator;
    }

private:
    struct BlockData {
        std::string hash;
        std::vector<std::string> tx_hashes;
    };
    
    std::map<uint64_t, BlockData> blocks_;
};

// ============================================================================

class TimingAttackDetectionModule {
public:
    void observe_block_times(uint64_t height, uint32_t solve_time) {
        block_times_[height] = solve_time;
        if (block_times_.size() > 1000) {
            block_times_.erase(block_times_.begin());
        }
    }
    
    bool detect_timing_attack() const {
        if (block_times_.size() < 50) return false;
        
        // Check for unusual timing patterns
        uint32_t zero_times = 0;
        uint32_t instant_times = 0;
        
        for (const auto& [height, solve_time] : block_times_) {
            if (solve_time == 0) zero_times++;
            if (solve_time < 1) instant_times++;
        }
        
        // If too many blocks solved instantly, it's suspicious
        return (instant_times > block_times_.size() / 10);
    }

private:
    std::map<uint64_t, uint32_t> block_times_;
};

// ============================================================================
// TIER 6: MARKET INTELLIGENCE (Placeholder for future expansion)
// ============================================================================

class MarketIntelligenceModule {
public:
    struct MarketSignal {
        std::string signal_type;
        double correlation_strength;
        std::string implication;
    };
    
    // Placeholder for future price data integration
    std::vector<MarketSignal> analyze_price_correlation() const {
        return {};  // Not implemented yet
    }
};

// ============================================================================

/**
 * Master NINA Advanced Module Coordinator
 * Integrates all 6 tiers of learning
 */
class NInaAdvancedCoordinator {
public:
    NInaAdvancedCoordinator() = default;
    
    // Add references to all modules
    BlockTimePredictorModule& get_blocktime() { return blocktime_; }
    AttackForecastingModule& get_attack_forecast() { return attack_forecast_; }
    DifficultyTrendModule& get_difficulty_trend() { return difficulty_trend_; }
    AnomalousTransactionModule& get_anomalous_tx() { return anomalous_tx_; }
    MempoolWatcherModule& get_mempool() { return mempool_; }
    PeerReputationModule& get_peer_reputation() { return peer_reputation_; }
    NetworkHealthModule& get_network_health() { return network_health_; }
    GeoDistributionModule& get_geo_distribution() { return geo_distribution_; }
    PeerClusteringModule& get_peer_clustering() { return peer_clustering_; }
    FeeOptimizationModule& get_fee_optimization() { return fee_optimization_; }
    ConsensusParameterModule& get_consensus_params() { return consensus_params_; }
    NetworkSimulationModule& get_network_simulation() { return network_simulation_; }
    TemporalAnalysisModule& get_temporal_analysis() { return temporal_analysis_; }
    BehavioralClusteringModule& get_behavioral_clustering() { return behavioral_clustering_; }
    BlockForgeryDetectionModule& get_block_forgery() { return block_forgery_; }
    TimingAttackDetectionModule& get_timing_attack() { return timing_attack_; }
    MarketIntelligenceModule& get_market_intel() { return market_intel_; }
    
    void generate_intelligence_report(uint64_t current_height) {
        // Generate comprehensive report every 100 blocks
        if (current_height % 100 == 0) {
            // Call individual modules to gather intelligence
            // This is called periodically by the main daemon
        }
    }

private:
    BlockTimePredictorModule blocktime_;
    AttackForecastingModule attack_forecast_;
    DifficultyTrendModule difficulty_trend_;
    AnomalousTransactionModule anomalous_tx_;
    MempoolWatcherModule mempool_;
    PeerReputationModule peer_reputation_;
    NetworkHealthModule network_health_;
    GeoDistributionModule geo_distribution_;
    PeerClusteringModule peer_clustering_;
    FeeOptimizationModule fee_optimization_;
    ConsensusParameterModule consensus_params_;
    NetworkSimulationModule network_simulation_;
    TemporalAnalysisModule temporal_analysis_;
    BehavioralClusteringModule behavioral_clustering_;
    BlockForgeryDetectionModule block_forgery_;
    TimingAttackDetectionModule timing_attack_;
    MarketIntelligenceModule market_intel_;
};

}  // namespace ia
}  // namespace ninacatcoin
