// Copyright (c) 2026, The ninacatcoin Project
// NINA Network Health Monitor - Analyzes network conditions for FASE 3

#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <ctime>

namespace ninacatcoin_ai {

struct NetworkMetrics {
    double average_latency = 0.0;           // ms
    double peer_connectivity = 0.0;         // % of peers reachable
    uint64_t orphaned_blocks_24h = 0;       // blocks without parent
    uint64_t block_reorgs_24h = 0;          // chain reorganizations
    double propagation_time = 0.0;          // ms to propagate block
    double transaction_throughput = 0.0;    // tx/sec
    uint64_t network_hashrate = 0;          // hashes/sec
    double trust_score = 0.5;               // 0.0 (bad) to 1.0 (perfect)
};

enum class NetworkCondition {
    OPTIMAL,        // All metrics green
    DEGRADED,       // Some metrics yellow
    STRESSED,       // Multiple metrics red
    CRITICAL        // Network under attack
};

class NINANetworkHealthMonitor {
public:
    static NINANetworkHealthMonitor& getInstance();
    
    bool initialize();
    
    // Data collection
    void recordLatency(const std::string& peer_ip, double latency_ms);
    void recordBlockPropagation(uint64_t block_height, double propagation_time_ms);
    void recordOrphanedBlock(uint64_t block_height);
    void recordBlockReorg(uint64_t block_height, uint64_t reorg_depth);
    void recordTransactionThroughput(double tx_per_second);
    void recordNetworkHashrate(uint64_t hashrate);
    void recordPeerConnectivity(double percentage);
    
    // Analysis
    const NetworkMetrics& getCurrentMetrics() const;
    NetworkCondition diagnoseNetworkHealth();
    double calculateTrustScore();
    
    std::string getHealthReport() const;
    bool isNetworkUnderAttack() const;
    bool isNetworkSaturated() const;
    bool isNetworkIsolated() const;
    
    // Thresholds (configurable)
    void setLatencyThreshold(double ms);
    void setReorgThreshold(uint64_t count);
    void setOrphanThreshold(uint64_t count);
    
private:
    NINANetworkHealthMonitor();
    ~NINANetworkHealthMonitor();
    
    void updateTrustScore();
    void analyzeAttackPatterns();
    
    NetworkMetrics metrics;
    NetworkCondition current_condition = NetworkCondition::OPTIMAL;
    
    // Historical data for trend analysis
    std::vector<double> latency_history;
    std::vector<double> trust_history;
    std::vector<uint64_t> reorg_history;
    
    // Thresholds
    double latency_threshold_ms = 1000.0;
    uint64_t reorg_threshold = 3;           // More than 3 reorgs in 24h = warning
    uint64_t orphan_threshold = 10;         // More than 10 orphaned blocks = concern
    
    time_t last_analysis = 0;
};

} // namespace ninacatcoin_ai
