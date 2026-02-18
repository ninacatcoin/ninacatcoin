// Copyright (c) 2026, The ninacatcoin Project
//
// Integration between IA module and Discord notifier

#pragma once

#include "discord_notifier.hpp"
#include "ai/ai_module.hpp"
#include "ai/ai_checkpoint_validator.hpp"
#include <string>
#include <vector>
#include <thread>

namespace daemonize {

/**
 * @class DiscordIAIntegration
 * @brief Bridges IA module alerts to Discord notifications
 * 
 * The IA (NINA) detects attacks and logs them.
 * This class reads those logs and sends alerts to Discord.
 * 
 * Attack Categories:
 * 1. CHECKPOINT ATTACKS - From ai_checkpoint_validator.hpp
 *    ├─ Hash tampering
 *    ├─ Invalid hashes
 *    ├─ Epoch rollback
 *    └─ Unauthorized sources
 * 
 * 2. HASHRATE/51% ATTACKS - From ai_hashrate_recovery_monitor.hpp
 *    ├─ 51% hashrate concentration
 *    ├─ Pool identification
 *    ├─ Fork attempts
 *    └─ Difficulty anomalies
 * 
 * 3. NETWORK ATTACKS - From ai_network_sandbox.hpp
 *    ├─ Peer anomalies
 *    ├─ DDoS patterns
 *    └─ Isolation attempts
 */
class DiscordIAIntegration {
public:
    /**
     * @brief Initialize the integration
     * Starts monitoring thread for IA alerts
     */
    static bool initialize();

    /**
     * @brief Process checkpoint validator alert
     * Called when NINA detects checkpoint anomalies
     */
    static void processCheckpointAlert(
        const std::string& validation_status,
        const std::string& source_ip,
        const std::string& affected_checkpoints,
        bool is_attack
    );

    /**
     * @brief Process 51% attack detection
     * Called when hashrate recovery monitor detects concentration
     */
    static void process51PercentAlert(
        const std::string& pool_name,
        int hashrate_percentage,
        const std::vector<std::string>& evidence
    );

    /**
     * @brief Process network attack detection
     * Called when network sandbox detects anomalies
     */
    static void processNetworkAlert(
        const std::string& alert_type,
        const std::string& peer_ip,
        const std::vector<std::string>& details
    );

    /**
     * @brief Process code integrity violations
     * Called when IA code is tampered with
     */
    static void processSecurityAlert(
        const std::string& violation_type,
        const std::string& details
    );

    /**
     * @brief Manual alert for monitoring/testing
     */
    static void sendCustomAlert(
        const std::string& title,
        const std::string& description
    );

    /**
     * @brief Shutdown integration gracefully
     */
    static void shutdown();

private:
    static bool is_initialized;
    static std::thread monitoring_thread;
    static volatile bool should_monitor;

    /**
     * @brief Main monitoring loop
     * Continuously checks for IA alerts
     */
    static void monitoringLoop();

    /**
     * @brief Read IA diagnostic info and process alerts
     */
    static void checkIAAlerts();

    /**
     * @brief Categorize checkpoint attack
     */
    static AttackType categorizeCheckpointAttack(const std::string& status);

    /**
     * @brief Calculate 51% attack confidence
     */
    static int calculate51PercentConfidence(int hashrate_percentage);

    /**
     * @brief Parse pool info from hashrate data
     */
    static std::string parsePoolFromHashrate(const std::string& hashrate_data);
};

} // namespace daemonize

