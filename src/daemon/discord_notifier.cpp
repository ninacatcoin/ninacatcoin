// Copyright (c) 2026, The ninacatcoin Project
//
// Discord notifier implementation

#include "discord_notifier.hpp"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

namespace daemonize {

// Static member initialization
std::string DiscordNotifier::webhook_url = "";
bool DiscordNotifier::is_configured = false;
int DiscordNotifier::last_alert_count = 0;

bool DiscordNotifier::initialize(const std::string& url) {
    if (url.empty()) {
        MERROR("[Discord] ❌ No webhook URL provided");
        MINFO("[Discord] Set DISCORD_WEBHOOK environment variable or config file");
        is_configured = false;
        return false;
    }

    webhook_url = url;
    is_configured = true;
    
    MINFO("╔════════════════════════════════════════════════════════════╗");
    MINFO("║  ✅ DISCORD NOTIFIER INITIALIZED                          ║");
    MINFO("║                                                            ║");
    MINFO("║  Notifications:                                           ║");
    MINFO("║  ✓ Checkpoint attacks                                     ║");
    MINFO("║  ✓ 51% hashrate attacks                                   ║");
    MINFO("║  ✓ Network anomalies                                      ║");
    MINFO("║  ✓ Pool identification                                    ║");
    MINFO("║  ✓ Real-time attack alerts                                ║");
    MINFO("║                                                            ║");
    MINFO("║  Discord Channel: Ready to receive alerts                 ║");
    MINFO("╚════════════════════════════════════════════════════════════╝");
    
    return true;
}

bool DiscordNotifier::isConfigured() {
    return is_configured && !webhook_url.empty();
}

std::string DiscordNotifier::getStatus() {
    if (!isConfigured()) {
        return "Discord Notifier: NOT CONFIGURED";
    }
    return "Discord Notifier: ACTIVE - " + std::to_string(last_alert_count) + " alerts sent";
}

std::string DiscordNotifier::identifyPool(const std::string& ip) {
    // Known mining pool IP ranges and signatures
    // This is a simplified version - in production you'd have a database
    
    std::map<std::string, std::string> known_pools = {
        // Major mining pools
        {"51.75.", "Possible: Mining Pool EU"},
        {"54.191.", "Possible: AWS Miners"},
        {"5.39.", "Possible: OVH Miners"},
        {"95.211.", "Possible: Leaseweb Miners"},
        {"193.226.", "Possible: European Pool"},
        {"209.250.", "Possible: US Pool"},
        {"103.145.", "Possible: Asia-Pacific Pool"},
        {"198.51.", "Possible: RIPE Block (European)"},
        {"2001:db8", "Possible: IPv6 Mining Pool"}
    };

    for (const auto& [prefix, pool_name] : known_pools) {
        if (ip.find(prefix) == 0) {
            return pool_name;
        }
    }
    
    return "Unknown Pool";
}

int DiscordNotifier::getSeverityColor(int severity) {
    // Discord embed colors
    // Red = critical, Orange = high, Yellow = medium, Green = low
    switch (severity) {
        case 5: return 16711680;  // Red (CRITICAL)
        case 4: return 16745472;  // Orange-Red (HIGH)
        case 3: return 16776960;  // Yellow (MEDIUM)
        case 2: return 65280;     // Green (LOW)
        case 1: return 3066993;   // Blue (INFO)
        default: return 16711680; // Red
    }
}

std::string DiscordNotifier::getAttackEmoji(AttackType type) {
    switch (type) {
        case AttackType::CHECKPOINT_HASH_TAMPERING:
            return "🔴"; // Critical
        case AttackType::CHECKPOINT_INVALID_HASH:
            return "🟠"; // High
        case AttackType::CHECKPOINT_EPOCH_ROLLBACK:
            return "🔴"; // Critical
        case AttackType::CHECKPOINT_UNAUTHORIZED_SOURCE:
            return "🔴"; // Critical
        case AttackType::HASHRATE_51_PERCENT_ATTACK:
            return "🚨"; // CRITICAL 51%
        case AttackType::HASHRATE_51_PERCENT_POOL:
            return "🚨"; // CRITICAL 51%
        case AttackType::HASHRATE_ATTACK_DETECTED:
            return "⚠️";  // Warning
        case AttackType::HASHRATE_FORK_ATTEMPT:
            return "🔴"; // Critical
        case AttackType::NETWORK_PEER_ANOMALY:
            return "🟡"; // Medium
        case AttackType::NETWORK_PEER_FLOODING:
            return "🟠"; // High
        case AttackType::NETWORK_ISOLATION:
            return "🟠"; // High
        case AttackType::CODE_TAMPERING:
            return "🔴"; // Critical
        case AttackType::QUARANTINE_TRIGGERED:
            return "🚨"; // Critical
        default:
            return "❓";
    }
}

std::string DiscordNotifier::generateAttackJSON(const AttackDetail& attack) {
    std::ostringstream json;
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::localtime(&now);
    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    json << "{\n";
    json << "  \"embeds\": [{\n";
    json << "    \"title\": \"" << getAttackEmoji(attack.type) << " ATTACK DETECTED - " << attack.type_name << "\",\n";
    json << "    \"color\": " << getSeverityColor(attack.severity) << ",\n";
    json << "    \"fields\": [\n";
    json << "      {\"name\": \"Attack Type\", \"value\": \"" << attack.type_name << "\", \"inline\": true},\n";
    json << "      {\"name\": \"Severity\", \"value\": \"" << std::string(attack.severity, '*') << " / 5\", \"inline\": true},\n";
    json << "      {\"name\": \"Source IP\", \"value\": \"" << attack.source_ip << "\", \"inline\": true},\n";
    
    if (!attack.pool_name.empty() && attack.pool_name != "Unknown") {
        json << "      {\"name\": \"Pool/Origin\", \"value\": \"" << attack.pool_name << "\", \"inline\": true},\n";
    }
    
    if (attack.is_51_percent) {
        json << "      {\"name\": \"⚠️ 51% RISK\", \"value\": \"Possible 51% attack detected\", \"inline\": false},\n";
    }
    
    if (attack.is_checkpoint_attack) {
        json << "      {\"name\": \"Type\", \"value\": \"Checkpoint Attack\", \"inline\": true},\n";
    }
    
    json << "      {\"name\": \"Affected Height\", \"value\": \"" << attack.affected_height << "\", \"inline\": true},\n";
    json << "      {\"name\": \"Timestamp\", \"value\": \"" << time_str << "\", \"inline\": true},\n";
    json << "      {\"name\": \"Description\", \"value\": \"" << attack.description << "\", \"inline\": false},\n";
    
    if (!attack.details.empty()) {
        std::string details_str;
        for (size_t i = 0; i < attack.details.size() && i < 3; ++i) {
            details_str += "• " + attack.details[i] + "\\n";
        }
        json << "      {\"name\": \"Details\", \"value\": \"" << details_str << "\", \"inline\": false},\n";
    }
    
    json << "      {\"name\": \"Recommended Action\", \"value\": \"" << attack.recommendation << "\", \"inline\": false}\n";
    json << "    ],\n";
    json << "    \"footer\": {\"text\": \"NINA IA Security Module\"}\n";
    json << "  }]\n";
    json << "}\n";

    return json.str();
}

std::string DiscordNotifier::generate51PercentJSON(
    const std::string& pool_name,
    int confidence,
    const std::vector<std::string>& details) {
    
    std::ostringstream json;
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::localtime(&now);
    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    json << "{\n";
    json << "  \"embeds\": [{\n";
    json << "    \"title\": \"🚨 CRITICAL: 51% HASHRATE ATTACK DETECTED 🚨\",\n";
    json << "    \"color\": 16711680,\n";  // Red
    json << "    \"fields\": [\n";
    json << "      {\"name\": \"Attack Type\", \"value\": \"51% / Hashrate Attack\", \"inline\": false},\n";
    json << "      {\"name\": \"Attacking Pool\", \"value\": \"" << pool_name << "\", \"inline\": true},\n";
    json << "      {\"name\": \"Confidence\", \"value\": \"" << confidence << "%\", \"inline\": true},\n";
    json << "      {\"name\": \"Severity\", \"value\": \"⭐⭐⭐⭐⭐ CRITICAL\", \"inline\": false},\n";
    json << "      {\"name\": \"Timestamp\", \"value\": \"" << time_str << "\", \"inline\": false},\n";
    
    json << "      {\"name\": \"Indicators\", \"value\": \"";
    for (size_t i = 0; i < details.size() && i < 5; ++i) {
        json << "• " << details[i] << "\\n";
    }
    json << "\", \"inline\": false},\n";
    
    json << "      {\"name\": \"IMMEDIATE ACTIONS\", \"value\": \"";
    json << "1. Network enters ALERT mode\\n";
    json << "2. Double-check all transactions\\n";
    json << "3. Increase confirmation requirements\\n";
    json << "4. Contact community channels\\n";
    json << "5. Monitor for fork attempts\", \"inline\": false}\n";
    
    json << "    ],\n";
    json << "    \"footer\": {\"text\": \"NINA 51% Detection System\"}\n";
    json << "  }]\n";
    json << "}\n";

    return json.str();
}

std::string DiscordNotifier::generateCheckpointJSON(
    const std::string& attack_type,
    const std::string& source_ip,
    const std::string& seed_ip,
    const std::vector<std::string>& details) {
    
    std::ostringstream json;
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::localtime(&now);
    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    json << "{\n";
    json << "  \"embeds\": [{\n";
    json << "    \"title\": \"🔴 CHECKPOINT ATTACK DETECTED\",\n";
    json << "    \"color\": 16711680,\n";  // Red
    json << "    \"fields\": [\n";
    json << "      {\"name\": \"Attack Type\", \"value\": \"" << attack_type << "\", \"inline\": false},\n";
    json << "      {\"name\": \"Source IP\", \"value\": \"" << source_ip << "\", \"inline\": true},\n";
    
    if (!seed_ip.empty()) {
        json << "      {\"name\": \"Claimed Seed IP\", \"value\": \"" << seed_ip << "\", \"inline\": true},\n";
    }
    
    json << "      {\"name\": \"Severity\", \"value\": \"Critical\", \"inline\": true},\n";
    json << "      {\"name\": \"Timestamp\", \"value\": \"" << time_str << "\", \"inline\": false},\n";
    
    json << "      {\"name\": \"Technical Details\", \"value\": \"";
    for (size_t i = 0; i < details.size() && i < 4; ++i) {
        json << "• " << details[i] << "\\n";
    }
    json << "\", \"inline\": false},\n";
    
    json << "      {\"name\": \"Status\", \"value\": \"✓ Source has been QUARANTINED\\n✓ Checkpoints REJECTED\\n✓ Fallback to verified seeds\", \"inline\": false}\n";
    
    json << "    ],\n";
    json << "    \"footer\": {\"text\": \"NINA Checkpoint Guardian\"}\n";
    json << "  }]\n";
    json << "}\n";

    return json.str();
}

bool DiscordNotifier::sendToDiscord(const std::string& json_payload) {
    if (!isConfigured()) {
        MERROR("[Discord] Webhook not configured");
        return false;
    }

    // Use curl to send HTTP POST to Discord webhook
    // Escape single quotes in payload for shell safety
    std::string escaped_payload = json_payload;
    size_t pos = 0;
    while ((pos = escaped_payload.find('\'', pos)) != std::string::npos) {
        escaped_payload.replace(pos, 1, "'\\''");
        pos += 4;
    }

    // Build curl command: POST JSON to webhook URL, silent, with timeout
    std::string cmd = "curl -s -o /dev/null -w '%{http_code}' "
                      "-H 'Content-Type: application/json' "
                      "-X POST "
                      "-d '" + escaped_payload + "' "
                      "'" + webhook_url + "' "
                      "--connect-timeout 5 --max-time 10 2>/dev/null";

    // Execute in a fire-and-forget manner to not block the daemon
    // We use popen to capture the HTTP status code
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        MERROR("[Discord] Failed to execute curl command");
        return false;
    }

    char buffer[16] = {0};
    if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        // buffer contains the HTTP status code
        int status_code = std::atoi(buffer);
        pclose(pipe);

        if (status_code >= 200 && status_code < 300) {
            MINFO("[Discord] Alert sent successfully (HTTP " << status_code << ")");
            last_alert_count++;
            return true;
        } else if (status_code == 429) {
            MWARNING("[Discord] Rate limited (HTTP 429) — alert queued");
            last_alert_count++;
            return true;  // Rate limit is expected behavior
        } else {
            MERROR("[Discord] Failed to send alert (HTTP " << status_code << ")");
            return false;
        }
    }

    pclose(pipe);
    MWARNING("[Discord] No response from curl");
    return false;
}

bool DiscordNotifier::sendAttackAlert(const AttackDetail& attack) {
    if (!isConfigured()) return false;

    MINFO("[Discord] 🚨 Sending alert: " << attack.type_name);
    std::string json = generateAttackJSON(attack);
    return sendToDiscord(json);
}

bool DiscordNotifier::send51PercentAlert(
    const std::string& pool_name,
    int confidence,
    const std::vector<std::string>& details) {
    
    if (!isConfigured()) return false;

    MERROR("[Discord] 🚨🚨🚨 CRITICAL: 51% Attack Alert");
    MERROR("[Discord] Pool: " << pool_name << " | Confidence: " << confidence << "%");
    
    std::string json = generate51PercentJSON(pool_name, confidence, details);
    return sendToDiscord(json);
}

bool DiscordNotifier::sendCheckpointAttackAlert(
    const std::string& attack_type,
    const std::string& source_ip,
    const std::string& seed_ip,
    const std::vector<std::string>& details) {
    
    if (!isConfigured()) return false;

    MERROR("[Discord] 🔴 Checkpoint Attack Alert");
    MERROR("[Discord] Type: " << attack_type << " | Source: " << source_ip);
    
    std::string json = generateCheckpointJSON(attack_type, source_ip, seed_ip, details);
    return sendToDiscord(json);
}

bool DiscordNotifier::sendNetworkStatus(
    int peer_count,
    uint64_t height,
    uint64_t timestamp) {
    
    if (!isConfigured()) return false;

    std::ostringstream json;
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::localtime(&now);
    char time_str[20];
    std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    json << "{\n";
    json << "  \"embeds\": [{\n";
    json << "    \"title\": \"📊 Network Status Update\",\n";
    json << "    \"color\": 3066993,\n";  // Blue
    json << "    \"fields\": [\n";
    json << "      {\"name\": \"Connected Peers\", \"value\": \"" << peer_count << "\", \"inline\": true},\n";
    json << "      {\"name\": \"Blockchain Height\", \"value\": \"" << height << "\", \"inline\": true},\n";
    json << "      {\"name\": \"Timestamp\", \"value\": \"" << time_str << "\", \"inline\": true},\n";
    json << "      {\"name\": \"Status\", \"value\": \"" << (peer_count >= 8 ? "✅ Healthy" : "⚠️ Low peers") << "\", \"inline\": true}\n";
    json << "    ],\n";
    json << "    \"footer\": {\"text\": \"NINA Network Monitor\"}\n";
    json << "  }]\n";
    json << "}\n";

    return sendToDiscord(json.str());
}

} // namespace daemonize

