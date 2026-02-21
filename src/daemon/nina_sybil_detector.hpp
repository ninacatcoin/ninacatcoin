/**
 * NINA Sybil Attack Detector
 * 
 * Detects when multiple peers are acting as coordinated clones (Sybil attack)
 * by analyzing behavioral correlations, timing patterns, and network topology
 * 
 * Copyright (c) 2026, The ninacatcoin Project
 */

#pragma once

#include "misc_log_ex.h"
#include <map>
#include <vector>
#include <string>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <deque>
#include <sstream>
#include <iomanip>

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina_sybil"

namespace daemonize {

/**
 * @brief Sybil Detector Module - Identifies coordinated peer attacks
 * 
 * A Sybil attack occurs when an attacker controls multiple peers that act
 * in a coordinated manner to:
 * - Isolate and eclipse our node (control all our connections)
 * - Censor transactions
 * - Gain majority voting power
 * - Perform timing attacks
 * 
 * Detection strategy:
 * 1. Monitor per-peer behavioral metrics (latency, response times, block propagation)
 * 2. Analyze correlation between peers (are they behaving like clones?)
 * 3. Calculate statistical confidence that peers are correlated
 * 4. Cluster peers by similarity
 * 5. Flag clusters as suspicious if correlation is too high
 */
class SybilDetectorModule {
public:
    struct PeerMetrics {
        std::string peer_id;
        std::deque<uint32_t> block_announce_times;  // ms to announce block
        std::deque<uint32_t> transaction_announce_times;  // ms to announce tx
        std::deque<double> response_latencies;  // network latency samples
        std::deque<uint64_t> sync_times;  // time to sync to tip
        std::deque<uint32_t> block_heights_seen;  // blocks announced
        uint32_t total_announcements = 0;
        double last_seen_latency = 0;
        time_t last_active = 0;
        uint32_t observations = 0;
    };
    
    struct SybilScore {
        std::string peer_id;
        double correlation_confidence;  // 0-100, probability this peer is Sybil
        std::vector<std::string> correlated_peers;  // peers with similar behavior
        std::string threat_level;  // "safe", "suspicious", "dangerous"
        std::string reasoning;
    };
    
    struct ClusterResult {
        std::vector<std::vector<std::string>> clusters;  // groups of highly correlated peers
        std::vector<double> cluster_correlation_scores;  // confidence for each cluster
        std::vector<std::string> flagged_peers;  // peers to distrust
        std::string cluster_analysis;
    };
    
    // ===========================================================================
    
    SybilDetectorModule() : max_history_size_(100) {
        MINFO("[NINA-SYBIL] Sybil Detector initialized");
    }
    
    /**
     * @brief Observe a peer announcing a block
     */
    void observe_block_announcement(
        const std::string& peer_id,
        uint32_t block_height,
        uint32_t announcement_time_ms,
        double peer_latency
    ) {
        auto& metrics = peer_metrics_[peer_id];
        metrics.peer_id = peer_id;
        
        // Add announcement timing
        metrics.block_announce_times.push_back(announcement_time_ms);
        if (metrics.block_announce_times.size() > max_history_size_) {
            metrics.block_announce_times.pop_front();
        }
        
        // Track latency
        metrics.response_latencies.push_back(peer_latency);
        if (metrics.response_latencies.size() > max_history_size_) {
            metrics.response_latencies.pop_front();
        }
        
        // Track block height seen
        metrics.block_heights_seen.push_back(block_height);
        if (metrics.block_heights_seen.size() > max_history_size_) {
            metrics.block_heights_seen.pop_front();
        }
        
        metrics.last_seen_latency = peer_latency;
        metrics.last_active = std::time(nullptr);
        metrics.total_announcements++;
        metrics.observations++;
    }
    
    /**
     * @brief Observe a peer announcing a transaction
     */
    void observe_transaction_announcement(
        const std::string& peer_id,
        uint32_t announcement_time_ms
    ) {
        auto& metrics = peer_metrics_[peer_id];
        metrics.peer_id = peer_id;
        
        metrics.transaction_announce_times.push_back(announcement_time_ms);
        if (metrics.transaction_announce_times.size() > max_history_size_) {
            metrics.transaction_announce_times.pop_front();
        }
        
        metrics.last_active = std::time(nullptr);
        metrics.total_announcements++;
    }
    
    /**
     * @brief Calculate Sybil score for a specific peer
     * Returns how likely this peer is part of a Sybil attack
     */
    SybilScore calculate_peer_sybil_score(const std::string& peer_id) {
        SybilScore score;
        score.peer_id = peer_id;
        score.correlation_confidence = 0.0;
        score.threat_level = "safe";
        
        if (peer_metrics_.find(peer_id) == peer_metrics_.end()) {
            score.reasoning = "Not enough data";
            return score;
        }
        
        const auto& target_metrics = peer_metrics_[peer_id];
        
        // Need minimum observations
        if (target_metrics.observations < 5) {
            score.reasoning = "Insufficient observations (" + std::to_string(target_metrics.observations) + ")";
            return score;
        }
        
        // Find peers with similar behavior
        std::vector<std::pair<std::string, double>> peer_correlations;
        
        for (const auto& [other_id, other_metrics] : peer_metrics_) {
            if (other_id == peer_id) continue;
            if (other_metrics.observations < 5) continue;
            
            // Calculate correlation
            double correlation = calculate_behavioral_correlation(target_metrics, other_metrics);
            if (correlation > 0.7) {  // Significant correlation threshold
                score.correlated_peers.push_back(other_id);
                peer_correlations.push_back({other_id, correlation});
            }
        }
        
        // Score calculation
        if (score.correlated_peers.size() >= 2) {
            // Multiple peers behaving identically = high Sybil probability
            double avg_correlation = 0.0;
            for (const auto& [_, corr] : peer_correlations) {
                avg_correlation += corr;
            }
            avg_correlation /= peer_correlations.size();
            
            // Increased confidence with more correlated peers
            score.correlation_confidence = std::min(95.0, 60.0 + (score.correlated_peers.size() * 10.0));
            score.correlation_confidence *= avg_correlation;  // Adjust by actual correlation strength
            
            score.threat_level = "dangerous";
            score.reasoning = "High behavioral correlation with " + std::to_string(score.correlated_peers.size()) + 
                            " other peers (avg correlation: " + format_double(avg_correlation, 2) + ")";
            
        } else if (score.correlated_peers.size() == 1) {
            // One peer behaving same = moderate suspicion
            score.correlation_confidence = 45.0;
            score.threat_level = "suspicious";
            score.reasoning = "Moderate behavioral correlation with 1 peer";
            
        } else {
            // Unique behavior = not Sybil
            score.correlation_confidence = 5.0;
            score.threat_level = "safe";
            score.reasoning = "Unique behavior pattern, no significant correlations detected";
        }
        
        return score;
    }
    
    /**
     * @brief Detect clusters of potentially coordinated peers
     * Returns groupings of peers with high behavioral similarity
     */
    ClusterResult detect_sybil_clusters() {
        ClusterResult result;
        result.cluster_analysis = "";
        
        if (peer_metrics_.size() < 3) {
            result.cluster_analysis = "Insufficient peers for reliable clustering (" + 
                                     std::to_string(peer_metrics_.size()) + ")";
            return result;
        }
        
        // Build correlation matrix
        std::vector<std::string> peer_ids;
        std::map<std::string, std::map<std::string, double>> correlation_matrix;
        
        for (const auto& [peer_id, _] : peer_metrics_) {
            peer_ids.push_back(peer_id);
        }
        
        // Calculate all pairwise correlations
        for (size_t i = 0; i < peer_ids.size(); i++) {
            for (size_t j = i + 1; j < peer_ids.size(); j++) {
                const auto& metrics_i = peer_metrics_[peer_ids[i]];
                const auto& metrics_j = peer_metrics_[peer_ids[j]];
                
                if (metrics_i.observations < 5 || metrics_j.observations < 5) {
                    continue;
                }
                
                double corr = calculate_behavioral_correlation(metrics_i, metrics_j);
                correlation_matrix[peer_ids[i]][peer_ids[j]] = corr;
                correlation_matrix[peer_ids[j]][peer_ids[i]] = corr;
            }
        }
        
        // Simple clustering: find groups where all members correlate > 0.75
        std::map<std::string, bool> clustered;
        
        for (const auto& [peer_id, _] : peer_metrics_) {
            if (clustered[peer_id]) continue;
            
            std::vector<std::string> cluster;
            cluster.push_back(peer_id);
            clustered[peer_id] = true;
            
            // Find all peers that correlate strongly with this one
            for (const auto& [other_id, corr_map] : correlation_matrix) {
                if (clustered[other_id]) continue;
                if (corr_map.find(peer_id) == corr_map.end()) continue;
                
                double corr = corr_map.at(peer_id);
                if (corr > 0.72) {  // High correlation threshold for clustering
                    cluster.push_back(other_id);
                    clustered[other_id] = true;
                }
            }
            
            // Only report clusters of 2+ that suggest Sybil behavior
            if (cluster.size() >= 2) {
                result.clusters.push_back(cluster);
                
                // Calculate cluster correlation score
                double avg_correlation = 0.0;
                int correlation_count = 0;
                for (size_t i = 0; i < cluster.size(); i++) {
                    for (size_t j = i + 1; j < cluster.size(); j++) {
                        auto it = correlation_matrix[cluster[i]].find(cluster[j]);
                        if (it != correlation_matrix[cluster[i]].end()) {
                            avg_correlation += it->second;
                            correlation_count++;
                        }
                    }
                }
                
                if (correlation_count > 0) {
                    avg_correlation /= correlation_count;
                    result.cluster_correlation_scores.push_back(avg_correlation);
                    
                    // Flag all peers in suspicious cluster
                    if (avg_correlation > 0.70) {
                        for (const auto& peer_id : cluster) {
                            result.flagged_peers.push_back(peer_id);
                        }
                    }
                }
            }
        }
        
        // Generate analysis
        if (result.clusters.empty()) {
            result.cluster_analysis = "No suspicious clusters detected";
        } else {
            result.cluster_analysis = "Detected " + std::to_string(result.clusters.size()) + 
                                     " potential Sybil cluster(s) with " + 
                                     std::to_string(result.flagged_peers.size()) + " flagged peer(s)";
        }
        
        return result;
    }
    
    /**
     * @brief Get all Sybil scores for current peer set
     */
    std::vector<SybilScore> get_all_sybil_scores() {
        std::vector<SybilScore> scores;
        
        for (const auto& [peer_id, _] : peer_metrics_) {
            scores.push_back(calculate_peer_sybil_score(peer_id));
        }
        
        // Sort by threat level (most dangerous first)
        std::sort(scores.begin(), scores.end(), 
                 [](const SybilScore& a, const SybilScore& b) {
                     return a.correlation_confidence > b.correlation_confidence;
                 });
        
        return scores;
    }
    
    /**
     * @brief Get status summary
     */
    std::string get_sybil_status() const {
        // Count dangerous/suspicious peers
        int dangerous = 0, suspicious = 0;
        
        for (const auto& [_, metrics] : peer_metrics_) {
            auto score = const_cast<SybilDetectorModule*>(this)->calculate_peer_sybil_score(metrics.peer_id);
            if (score.threat_level == "dangerous") dangerous++;
            else if (score.threat_level == "suspicious") suspicious++;
        }
        
        std::string status = "[SYBIL] Monitoring " + std::to_string(peer_metrics_.size()) + 
                           " peers | Dangerous: " + std::to_string(dangerous) + 
                           " | Suspicious: " + std::to_string(suspicious);
        return status;
    }
    
    /**
     * @brief Clean up old peer data (inactive for >24h)
     */
    void cleanup_inactive_peers() {
        time_t now = std::time(nullptr);
        const time_t INACTIVE_THRESHOLD = 86400;  // 24 hours
        
        auto it = peer_metrics_.begin();
        while (it != peer_metrics_.end()) {
            if ((now - it->second.last_active) > INACTIVE_THRESHOLD) {
                MINFO("[SYBIL] Removing inactive peer: " << it->first);
                it = peer_metrics_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    /**
     * @brief Get number of monitored peers
     */
    size_t get_peer_count() const {
        return peer_metrics_.size();
    }

private:
    std::map<std::string, PeerMetrics> peer_metrics_;
    const size_t max_history_size_;
    
    /**
     * @brief Calculate how similar two peers' behavior is (0-1 scale)
     */
    double calculate_behavioral_correlation(
        const PeerMetrics& peer_a,
        const PeerMetrics& peer_b
    ) const {
        if (peer_a.block_announce_times.empty() || peer_b.block_announce_times.empty()) {
            return 0.0;
        }
        
        double correlation = 0.0;
        int measurements = 0;
        
        // 1. Latency similarity (how close are their network latencies?)
        if (!peer_a.response_latencies.empty() && !peer_b.response_latencies.empty()) {
            double avg_a = calculate_average(peer_a.response_latencies);
            double avg_b = calculate_average(peer_b.response_latencies);
            
            // If latencies are very similar, high correlation
            double latency_diff = std::abs(avg_a - avg_b);
            double latency_correlation = std::max(0.0, 1.0 - (latency_diff / 300.0));
            correlation += latency_correlation * 0.35;
            measurements++;
        }
        
        // 2. Block announcement timing similarity
        if (peer_a.block_announce_times.size() >= 3 && peer_b.block_announce_times.size() >= 3) {
            double variance_sum = 0.0;
            size_t min_size = std::min(peer_a.block_announce_times.size(), 
                                       peer_b.block_announce_times.size());
            
            for (size_t i = 0; i < std::min((size_t)10, min_size); i++) {
                double diff = std::abs(static_cast<double>(peer_a.block_announce_times[i]) - 
                                      static_cast<double>(peer_b.block_announce_times[i]));
                variance_sum += diff;
            }
            
            double avg_variance = variance_sum / std::min((size_t)10, min_size);
            // If announcement times are very similar (within 50ms), likely Sybil
            double timing_correlation = std::max(0.0, 1.0 - (avg_variance / 200.0));
            correlation += timing_correlation * 0.40;
            measurements++;
        }
        
        // 3. Transaction announcement patterns
        if (!peer_a.transaction_announce_times.empty() && !peer_b.transaction_announce_times.empty()) {
            // Check if they announce transactions at similar times
            double tx_correlation = 0.5;  // Base correlation for both announcing txs
            correlation += tx_correlation * 0.25;
            measurements++;
        }
        
        if (measurements == 0) {
            return 0.0;
        }
        
        return std::min(1.0, correlation / measurements);
    }
    
    /**
     * @brief Calculate average of a deque
     */
    template<typename T>
    double calculate_average(const std::deque<T>& values) const {
        if (values.empty()) return 0.0;
        double sum = 0.0;
        for (const auto& v : values) {
            sum += static_cast<double>(v);
        }
        return sum / values.size();
    }
    
    /**
     * @brief Format double to N decimal places
     */
    std::string format_double(double value, int decimal_places) const {
        std::ostringstream out;
        out << std::fixed << std::setprecision(decimal_places) << value;
        return out.str();
    }
};

} // namespace daemonize
