// Copyright (c) 2026, The ninacatcoin Project
//
// Main AI module

#pragma once

#include "ai_config.hpp"
#include "ai_sandbox.hpp"
#include "ai_network_sandbox.hpp"
#include "ai_integrity_verifier.hpp"
#include "ai_forced_remediation.hpp"
#include "ai_quarantine_system.hpp"
#include <string>
#include <memory>
#include <cstdint>

namespace ninacatcoin_ai {

/**
 * @class AIModule
 * @brief Main AI Security Module for ninacatcoin network
 * 
 * This module provides:
 * - Network anomaly detection and prevention
 * - Peer reputation tracking and validation
 * - Transaction validation and optimization
 * - Attack detection and prevention
 * - Code integrity verification
 * - Automatic remediation of compromised nodes
 * - Quarantine of persistently malicious nodes
 * 
 * CRITICAL: All operations are SANDBOXED
 * - Filesystem access restricted to whitelisted paths
 * - Network access restricted to P2P protocol
 * - Memory limited to 2GB
 * - CPU limited to 2 cores
 */
class AIModule {
public:
    enum class ModuleState {
        UNINITIALIZED = 0,
        VALIDATING = 1,
        ACTIVE = 2,
        DISABLED = 3,
        QUARANTINED = 4,
        REMEDIATION = 5,
        CRITICAL_ERROR = 6
    };

    // Singleton instance
    static AIModule& getInstance();

    /**
     * Initialize the AI module
     * Must be called once at daemon startup
     * Returns true if initialization successful
     */
    bool initialize();

    /**
     * Shutdown the AI module gracefully
     */
    void shutdown();

    /**
     * Check if AI module is active and operational
     */
    bool isActive() const;

    /**
     * Get current module state
     */
    ModuleState getState() const;

    /**
     * Get human-readable state name
     */
    std::string getStateName() const;

    /**
     * Get version string
     */
    std::string getVersion() const;

    /**
     * Get last error message
     */
    std::string getLastError() const;

    /**
     * Disable the AI module with reason
     */
    void disable(const std::string& reason);

    /**
     * Validate code integrity on startup
     */
    bool validateCodeIntegrity();

    /**
     * Register peer for monitoring
     */
    void registerPeer(const std::string& peer_ip);

    /**
     * Unregister/blacklist peer
     */
    void blacklistPeer(
        const std::string& peer_ip,
        const std::string& reason
    );

    /**
     * Analyze transaction for anomalies
     */
    bool analyzeTransaction(const std::vector<uint8_t>& tx_data);

    /**
     * Check if peer is blacklisted
     */
    bool isPeerBlacklisted(const std::string& peer_ip);

    /**
     * Get list of monitored peers
     */
    std::vector<std::string> getMonitoredPeers() const;

    /**
     * Check sandboxes are active
     */
    bool verifySandboxesActive();

    /**
     * Get diagnostic information
     */
    std::string getDiagnosticInfo() const;

private:
    AIModule();
    ~AIModule();

    // Delete copy/move constructors
    AIModule(const AIModule&) = delete;
    AIModule& operator=(const AIModule&) = delete;
    AIModule(AIModule&&) = delete;
    AIModule& operator=(AIModule&&) = delete;

    ModuleState current_state;
    bool is_active;
    std::string last_error;

    // Helper initialization functions
    bool loadConfiguration();
    bool performSecurityChecks();
    void initializeMonitoring();
    void monitoringLoop();
};

/**
 * @class AIAnomalyDetector
 * @brief Detects anomalies in network traffic/transactions
 */
class AIAnomalyDetector {
public:
    static AIAnomalyDetector& getInstance();

    // Analyze transaction for suspicious patterns
    bool isTransactionAnomaly(const std::vector<uint8_t>& tx_data);

    // Check if peer behavior is suspicious
    bool isPeerBehaviorSuspicious(const std::string& peer_ip);

    // Get peer reputation score (0-100)
    int getPeerReputation(const std::string& peer_ip);

private:
    AIAnomalyDetector();
    ~AIAnomalyDetector();

    AIAnomalyDetector(const AIAnomalyDetector&) = delete;
    AIAnomalyDetector& operator=(const AIAnomalyDetector&) = delete;
};

/**
 * @class AIPeerMonitor
 * @brief Monitors peer connections and reputations
 */
class AIPeerMonitor {
public:
    static AIPeerMonitor& getInstance();

    // Register new peer connection
    void registerConnection(const std::string& peer_ip);

    // Update peer statistics
    void updatePeerStats(
        const std::string& peer_ip,
        int blocks_received,
        int transactions_received,
        int validation_time_ms
    );

    // Get peer statistics
    std::string getPeerStats(const std::string& peer_ip);

private:
    AIPeerMonitor();
    ~AIPeerMonitor();

    AIPeerMonitor(const AIPeerMonitor&) = delete;
    AIPeerMonitor& operator=(const AIPeerMonitor&) = delete;
};

} // namespace ninacatcoin_ai
