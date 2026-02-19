// Copyright (c) 2026, The ninacatcoin Project
// NINA Network Health Monitor Implementation

#include "nina_network_health_monitor.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace ninacatcoin_ai {

static NINANetworkHealthMonitor* g_health_monitor = nullptr;

NINANetworkHealthMonitor& NINANetworkHealthMonitor::getInstance() {
    if (!g_health_monitor) {
        g_health_monitor = new NINANetworkHealthMonitor();
    }
    return *g_health_monitor;
}

NINANetworkHealthMonitor::NINANetworkHealthMonitor() {
}

NINANetworkHealthMonitor::~NINANetworkHealthMonitor() {
}

bool NINANetworkHealthMonitor::initialize() {
    std::cout << "[NINA Health Monitor] Initializing network health monitoring..." << std::endl;
    std::cout << "[NINA Health Monitor] Will analyze: latency, orphans, reorgs, throughput" << std::endl;
    metrics.trust_score = 0.75;  // Start optimistic
    return true;
}

void NINANetworkHealthMonitor::recordLatency(const std::string& peer_ip, double latency_ms) {
    latency_history.push_back(latency_ms);
    if (latency_history.size() > 100) {
        latency_history.erase(latency_history.begin());
    }
    
    // Calculate average
    double sum = 0.0;
    for (double lat : latency_history) {
        sum += lat;
    }
    metrics.average_latency = sum / latency_history.size();
}

void NINANetworkHealthMonitor::recordBlockPropagation(uint64_t block_height, double propagation_time_ms) {
    metrics.propagation_time = propagation_time_ms;
    
    if (propagation_time_ms > 5000.0) {
        std::cout << "[NINA Health Monitor] WARNING: Slow block propagation: " << propagation_time_ms << "ms" << std::endl;
    }
}

void NINANetworkHealthMonitor::recordOrphanedBlock(uint64_t block_height) {
    metrics.orphaned_blocks_24h++;
    
    if (metrics.orphaned_blocks_24h > orphan_threshold) {
        std::cout << "[NINA Health Monitor] ALERT: Orphaned block threshold exceeded: " 
                  << metrics.orphaned_blocks_24h << std::endl;
    }
}

void NINANetworkHealthMonitor::recordBlockReorg(uint64_t block_height, uint64_t reorg_depth) {
    metrics.block_reorgs_24h++;
    reorg_history.push_back(metrics.block_reorgs_24h);
    if (reorg_history.size() > 100) {
        reorg_history.erase(reorg_history.begin());
    }
    
    if (metrics.block_reorgs_24h > reorg_threshold) {
        std::cout << "[NINA Health Monitor] ALERT: Block reorganization detected (depth=" << reorg_depth 
                  << ") - Total in 24h: " << metrics.block_reorgs_24h << std::endl;
    }
}

void NINANetworkHealthMonitor::recordTransactionThroughput(double tx_per_second) {
    metrics.transaction_throughput = tx_per_second;
}

void NINANetworkHealthMonitor::recordNetworkHashrate(uint64_t hashrate) {
    metrics.network_hashrate = hashrate;
}

void NINANetworkHealthMonitor::recordPeerConnectivity(double percentage) {
    metrics.peer_connectivity = percentage;
    
    if (percentage < 50.0) {
        std::cout << "[NINA Health Monitor] CRITICAL: Low peer connectivity: " << percentage << "%" << std::endl;
    }
}

const NetworkMetrics& NINANetworkHealthMonitor::getCurrentMetrics() const {
    return metrics;
}

NetworkCondition NINANetworkHealthMonitor::diagnoseNetworkHealth() {
    // Score-based diagnosis
    int warning_count = 0;
    int critical_count = 0;
    
    // Check latency
    if (metrics.average_latency > latency_threshold_ms) {
        warning_count++;
    }
    if (metrics.average_latency > latency_threshold_ms * 2) {
        critical_count++;
    }
    
    // Check reorgs
    if (metrics.block_reorgs_24h > reorg_threshold) {
        warning_count++;
    }
    if (metrics.block_reorgs_24h > reorg_threshold * 2) {
        critical_count++;
    }
    
    // Check orphans
    if (metrics.orphaned_blocks_24h > orphan_threshold) {
        warning_count++;
    }
    if (metrics.orphaned_blocks_24h > orphan_threshold * 2) {
        critical_count++;
    }
    
    // Check connectivity
    if (metrics.peer_connectivity < 75.0) {
        warning_count++;
    }
    if (metrics.peer_connectivity < 50.0) {
        critical_count++;
    }
    
    if (critical_count >= 2) {
        current_condition = NetworkCondition::CRITICAL;
    } else if (critical_count >= 1 || warning_count >= 3) {
        current_condition = NetworkCondition::STRESSED;
    } else if (warning_count >= 1) {
        current_condition = NetworkCondition::DEGRADED;
    } else {
        current_condition = NetworkCondition::OPTIMAL;
    }
    
    updateTrustScore();
    return current_condition;
}

double NINANetworkHealthMonitor::calculateTrustScore() {
    // Base score
    double score = 1.0;
    
    // Latency penalty
    if (metrics.average_latency < 100) {
        // Good
    } else if (metrics.average_latency < 500) {
        score -= 0.1;
    } else if (metrics.average_latency < 1000) {
        score -= 0.2;
    } else {
        score -= 0.4;
    }
    
    // Reorg penalty
    if (metrics.block_reorgs_24h > 0) {
        score -= (std::min(10.0, (double)metrics.block_reorgs_24h) / 10.0) * 0.3;
    }
    
    // Orphan penalty
    if (metrics.orphaned_blocks_24h > 0) {
        score -= (std::min(20.0, (double)metrics.orphaned_blocks_24h) / 20.0) * 0.2;
    }
    
    // Connectivity boost/penalty
    score += (metrics.peer_connectivity / 100.0) * 0.1;
    
    // Clamp to [0, 1]
    return std::max(0.0, std::min(1.0, score));
}

void NINANetworkHealthMonitor::updateTrustScore() {
    metrics.trust_score = calculateTrustScore();
    trust_history.push_back(metrics.trust_score);
    if (trust_history.size() > 100) {
        trust_history.erase(trust_history.begin());
    }
}

std::string NINANetworkHealthMonitor::getHealthReport() const {
    std::stringstream ss;
    ss << "\n====== NINA NETWORK HEALTH REPORT ======\n";
    ss << "Condition: ";
    
    switch (current_condition) {
        case NetworkCondition::OPTIMAL:    ss << "OPTIMAL"; break;
        case NetworkCondition::DEGRADED:   ss << "DEGRADED"; break;
        case NetworkCondition::STRESSED:   ss << "STRESSED"; break;
        case NetworkCondition::CRITICAL:   ss << "CRITICAL"; break;
    }
    
    ss << "\nTrust Score: " << (metrics.trust_score * 100.0) << "%\n";
    ss << "Average Latency: " << metrics.average_latency << " ms\n";
    ss << "Peer Connectivity: " << metrics.peer_connectivity << "%\n";
    ss << "Orphaned Blocks (24h): " << metrics.orphaned_blocks_24h << "\n";
    ss << "Block Reorgs (24h): " << metrics.block_reorgs_24h << "\n";
    ss << "Propagation Time: " << metrics.propagation_time << " ms\n";
    ss << "Transaction Throughput: " << metrics.transaction_throughput << " tx/s\n";
    ss << "========================================\n\n";
    
    return ss.str();
}

bool NINANetworkHealthMonitor::isNetworkUnderAttack() const {
    return (current_condition == NetworkCondition::CRITICAL && 
            (metrics.block_reorgs_24h > 5 || metrics.orphaned_blocks_24h > 20));
}

bool NINANetworkHealthMonitor::isNetworkSaturated() const {
    return (metrics.transaction_throughput > 1000.0 &&  // Too many pending
            metrics.propagation_time > 5000.0);          // Slow propagation
}

bool NINANetworkHealthMonitor::isNetworkIsolated() const {
    return (metrics.peer_connectivity < 25.0);  // Less than 25% of peers reachable
}

void NINANetworkHealthMonitor::setLatencyThreshold(double ms) {
    latency_threshold_ms = ms;
}

void NINANetworkHealthMonitor::setReorgThreshold(uint64_t count) {
    reorg_threshold = count;
}

void NINANetworkHealthMonitor::setOrphanThreshold(uint64_t count) {
    orphan_threshold = count;
}

} // namespace ninacatcoin_ai
