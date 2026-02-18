// Copyright (c) 2026, The ninacatcoin Project
//
// Discord notification system for IA-detected attacks

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <map>
#include "misc_log_ex.h"

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "discord"

namespace daemonize {

/**
 * @enum AttackType
 * @brief Classification of detected attacks
 */
enum class AttackType {
    // Checkpoint-based attacks
    CHECKPOINT_HASH_TAMPERING,      // Hashes modificados en checkpoint
    CHECKPOINT_INVALID_HASH,        // Hash no en blockchain
    CHECKPOINT_EPOCH_ROLLBACK,      // Intento de epoch rollback
    CHECKPOINT_STALE_DATA,          // Datos obsoletos
    CHECKPOINT_UNAUTHORIZED_SOURCE, // De seed no autorizada
    
    // 51% / Hashrate attacks
    HASHRATE_51_PERCENT_ATTACK,     // Posible ataque 51%
    HASHRATE_51_PERCENT_POOL,       // Pool específica atacando
    HASHRATE_ATTACK_DETECTED,       // Ataque de dificultad
    HASHRATE_FORK_ATTEMPT,          // Intento de fork
    
    // Network attacks
    NETWORK_PEER_ANOMALY,           // Comportamiento anómalo de peer
    NETWORK_PEER_FLOODING,          // DDoS de peer
    NETWORK_ISOLATION,              // Nodo aislado
    NETWORK_DESYNC,                 // Red desincronizada
    
    // Security attacks
    CODE_TAMPERING,                 // Código IA comprometido
    SANDBOX_VIOLATION,              // Violación de sandbox
    QUARANTINE_TRIGGERED,           // Cuarentena activada
    
    UNKNOWN_ATTACK
};

/**
 * @struct AttackDetail
 * @brief Información detallada de un ataque detectado
 */
struct AttackDetail {
    AttackType type;
    std::string type_name;
    std::string source_ip;
    std::string pool_name;              // Si es identificable
    uint64_t timestamp;
    std::string description;
    std::vector<std::string> details;
    int severity;                       // 1-5 (1=bajo, 5=crítico)
    bool is_51_percent;                // ¿Parece ser 51%?
    bool is_checkpoint_attack;         // ¿Ataque de checkpoint?
    uint64_t affected_height;          // Altura afectada
    std::string recommendation;        // Qué hacer
};

/**
 * @class DiscordNotifier
 * @brief Sends attack notifications to Discord
 * 
 * Receives alerts from NINA (IA module) and sends to Discord webhook.
 * The IA remains in sandbox - daemon handles external communication.
 */
class DiscordNotifier {
public:
    /**
     * @brief Initialize Discord notifier
     * @param webhook_url Discord webhook URL from environment
     */
    static bool initialize(const std::string& webhook_url);

    /**
     * @brief Send attack alert to Discord
     * @param attack Attack details
     */
    static bool sendAttackAlert(const AttackDetail& attack);

    /**
     * @brief Send 51% attack alert (CRITICAL)
     * @param pool_name Name of attacking pool
     * @param confidence Confidence level (0-100%)
     * @param details Additional details
     */
    static bool send51PercentAlert(
        const std::string& pool_name,
        int confidence,
        const std::vector<std::string>& details
    );

    /**
     * @brief Send checkpoint attack alert
     * @param attack_type Specific checkpoint attack type
     * @param source_ip Source of malicious checkpoint
     * @param seed_ip Authorized seed it claimed to be from
     * @param details Additional details
     */
    static bool sendCheckpointAttackAlert(
        const std::string& attack_type,
        const std::string& source_ip,
        const std::string& seed_ip,
        const std::vector<std::string>& details
    );

    /**
     * @brief Send network status update
     * @param peer_count Number of connected peers
     * @param height Current blockchain height
     * @param timestamp Update time
     */
    static bool sendNetworkStatus(
        int peer_count,
        uint64_t height,
        uint64_t timestamp
    );

    /**
     * @brief Check if notifier is configured
     */
    static bool isConfigured();

    /**
     * @brief Get configuration status
     */
    static std::string getStatus();

private:
    static std::string webhook_url;
    static bool is_configured;
    static int last_alert_count;

    /**
     * @brief Actually send JSON to Discord
     */
    static bool sendToDiscord(const std::string& json_payload);

    /**
     * @brief Generate JSON embed for attack
     */
    static std::string generateAttackJSON(const AttackDetail& attack);

    /**
     * @brief Generate JSON embed for 51% attack
     */
    static std::string generate51PercentJSON(
        const std::string& pool_name,
        int confidence,
        const std::vector<std::string>& details
    );

    /**
     * @brief Generate JSON embed for checkpoint attack
     */
    static std::string generateCheckpointJSON(
        const std::string& attack_type,
        const std::string& source_ip,
        const std::string& seed_ip,
        const std::vector<std::string>& details
    );

    /**
     * @brief Identify pool from IP address
     */
    static std::string identifyPool(const std::string& ip);

    /**
     * @brief Get severity color for Discord
     */
    static int getSeverityColor(int severity);

    /**
     * @brief Get emoji for attack type
     */
    static std::string getAttackEmoji(AttackType type);
};

} // namespace daemonize

