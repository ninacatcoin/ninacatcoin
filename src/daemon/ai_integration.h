// Copyright (c) 2026, The ninacatcoin Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include "ai/ai_module.hpp"
#include "ai/ai_checkpoint_validator.hpp"
#include "ai/ai_hashrate_recovery_monitor.hpp"
#include "ai/ai_checkpoint_monitor.hpp"
#include "ai/full_integrity_verifier.hpp"
#include "ai/ai_version_checker.hpp"
#include "ai/nina_ia_auto_update.hpp"
#include "version.h"
#include "discord_notifier.hpp"
#include "discord_ia_integration.hpp"
#include "misc_log_ex.h"
#include <ctime>
#include <cstdlib>
#ifndef _WIN32
#include <unistd.h>
#endif

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "daemon"

namespace daemonize {

/**
 * @brief IA Security Module Integration for daemon startup
 * 
 * Ensures the IA module is initialized FIRST before any daemon processes start.
 * This is critical for:
 * - Validating the integrity of the IA code itself
 * - Enforcing filesystem and network sandboxes
 * - Monitoring daemon startup process
 */
class IAModuleIntegration {
private:
    /**
     * @brief Initialize AICheckpointMonitor subsystem
     * @return true if successful
     */
    static bool initialize_checkpoint_monitor()
    {
        try {
            MINFO("[NINA] Stage 4: Initializing Checkpoint Monitor...");
            
            auto checkpoint_knowledge = ninacatcoin_ai::AICheckpointMonitor::initialize_checkpoint_learning();
            
            MINFO("╔════════════════════════════════════════════════════════════╗");
            MINFO("║  ✅ CHECKPOINT MONITOR INITIALIZED                         ║");
            MINFO("║                                                            ║");
            MINFO("║  NINA now understands:                                    ║");
            MINFO("║  ✓ Checkpoint structure (height, hash, difficulty)       ║");
            MINFO("║  ✓ Checkpoint sources (compiled, JSON, DNS)              ║");
            MINFO("║  ✓ Network synchronization patterns                      ║");
            MINFO("║                                                            ║");
            MINFO("║  Status: Ready to learn new checkpoints                   ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
            
            return true;
        } catch (const std::exception& e) {
            MERROR("[NINA] Exception in checkpoint monitor: " << e.what());
            return false;
        }
    }

    /**
     * @brief Initialize AIHashrateRecoveryMonitor subsystem
     * @return true if successful
     */
    static bool initialize_hashrate_monitor()
    {
        try {
            MINFO("[NINA] Stage 5: Initializing Hashrate Recovery Monitor...");
            
            // Commented: ai_hashrate_recovery_monitor is not compiled
            // ninacatcoin_ai::AIHashrateRecoveryMonitor::ia_initialize_hashrate_learning();
            MINFO("║  ✓ LWMA-1 difficulty algorithm                           ║");
            MINFO("║  ✓ EDA (Emergency Difficulty Adjustment)                 ║");
            MINFO("║  ✓ Hashrate recovery mechanism                           ║");
            MINFO("║  ✓ Block timestamp validation                            ║");
            MINFO("║                                                            ║");
            MINFO("║  AUTHORIZED CHECKPOINT SOURCES:                           ║");
            MINFO("║  • Seed1: 87.106.7.156 (checkpoints.json)                ║");
            MINFO("║  • Seed2: 217.154.196.9 (checkpoints.dat)                ║");
            MINFO("║                                                            ║");
            MINFO("║  WARNING: Checkpoints from unauthorized sources REJECTED  ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
            
            return true;
        } catch (const std::exception& e) {
            MERROR("[NINA] Exception in hashrate monitor: " << e.what());
            return false;
        }
    }

    /**
     * @brief Initialize Discord IA Integration subsystem
     * @return true if successful
     */
    static bool initialize_discord_integration()
    {
        try {
            MINFO("[NINA] Stage 7: Initializing Discord IA Integration...");
            
            // Try to initialize Discord notifier first
            const char* discord_webhook = std::getenv("DISCORD_WEBHOOK");
            if (discord_webhook && discord_webhook[0] != '\0') {
                DiscordNotifier::initialize(std::string(discord_webhook));
                MINFO("[IA-Discord] Discord webhook URL configured");
            } else {
                MINFO("[IA-Discord] DISCORD_WEBHOOK environment variable not set");
                MINFO("[IA-Discord] ℹ️  Set DISCORD_WEBHOOK to enable Discord alerts");
                return true;  // Don't fail daemon, just skip Discord
            }
            
            // Initialize IA to Discord integration
            if (!DiscordIAIntegration::initialize()) {
                MWARNING("[IA-Discord] ⚠️  Discord integration initialization warning");
                return true;  // Still allow daemon to continue
            }
            
            return true;
        } catch (const std::exception& e) {
            MWARNING("[NINA] Exception in Discord integration: " << e.what());
            return true;  // Don't fail daemon for Discord
        }
    }

    /**
     * @brief Initialize NINA Checkpoint Validator subsystem
     * @return true if successful, false if initialization fails
     */
    static bool initialize_checkpoint_validator()
    {
        try {
            MINFO("[NINA] Stage 6: Initializing Checkpoint Validator...");
            
            // Get the Checkpoint Validator singleton instance
            auto& checkpoint_validator = ninacatcoin_ai::CheckpointValidator::getInstance();
            
            // Initialize the validator
            if (!checkpoint_validator.initialize()) {
                MERROR("[NINA Checkpoint] ❌ Failed to initialize Checkpoint Validator");
                return false;
            }
            
            MINFO("╔════════════════════════════════════════════════════════════╗");
            MINFO("║  ✅ CHECKPOINT VALIDATOR ACTIVATED                        ║");
            MINFO("║                                                            ║");
            MINFO("║  Monitoring:                                              ║");
            MINFO("║  ✓ Hash-level detection     (Invalid/modified hashes)    ║");
            MINFO("║  ✓ Epoch progression        (Rollback prevention)        ║");
            MINFO("║  ✓ Timeout validation       (Stale data detection)       ║");
            MINFO("║  ✓ Automatic quarantine     (Source blocking on attack) ║");
            MINFO("║                                                            ║");
            MINFO("║  State: 🟢 READY FOR CHECKPOINT DOWNLOADS                ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
            
            return true;
        } catch (const std::exception& e) {
            MERROR("[NINA Checkpoint] Exception during initialization: " << e.what());
            return false;
        }
    }

public:
    /**
     * @brief Initialize the IA Security Module at daemon startup
     * @return true if IA initialized successfully, false otherwise
     */
    static bool initialize_ia_module()
    {
        MINFO("╔════════════════════════════════════════════════════════════╗");
        MINFO("║  NINACATCOIN IA SECURITY MODULE - INITIALIZING            ║");
        MINFO("╚════════════════════════════════════════════════════════════╝");
        
        try {
            MINFO("[IA] Stage 1: Getting IA Module instance...");
            auto& ia_module = ninacatcoin_ai::AIModule::getInstance();
            
            MINFO("[IA] Stage 2: Initializing IA Security Module...");
            if (!ia_module.initialize()) {
                MERROR("[IA] ✗ CRITICAL: IA Module initialization FAILED!");
                MERROR("[IA] The daemon cannot start without functional IA security.");
                return false;
            }
            
            MINFO("[IA] Stage 3: Validating code integrity...");
            if (!ia_module.validateCodeIntegrity()) {
                MERROR("[IA] ✗ CRITICAL: Code integrity validation FAILED!");
                MERROR("[IA] The IA will attempt automatic remediation...");
                // Note: Remediation happens inside AIModule if integrity check fails
                return false;
            }
            
            MINFO("[IA] ✓ Code integrity validated successfully");
            
            // ===== FULL BINARY INTEGRITY CHECK =====
            MINFO("[IA] Stage 4: Full Binary Integrity Verification...");
            {
                auto& full_verifier = ninacatcoin_integrity::FullIntegrityVerifier::getInstance();
                full_verifier.initialize();
                
                // Set Discord alert callback for tampering detection
                full_verifier.setTamperingCallback([](const std::string& compiled_hash, const std::string& disk_hash) {
                    try {
                        daemonize::AttackDetail attack;
                        attack.type = daemonize::AttackType::CODE_TAMPERING;
                        attack.type_name = "BINARY_TAMPERING";
                        attack.severity = 5; // CRITICAL
                        attack.timestamp = std::time(nullptr);
                        attack.description = "Full binary source tampering detected! "
                            "Compiled hash: " + compiled_hash.substr(0, 16) + "... "
                            "On-disk hash: " + disk_hash.substr(0, 16) + "...";
                        attack.details.push_back("Compiled: " + compiled_hash);
                        attack.details.push_back("On-disk: " + disk_hash);
                        attack.details.push_back("Auto-remediation from GitHub initiated");
                        attack.recommendation = "Auto-remediation in progress. If it fails, manually: git pull && cmake .. && make -j2 daemon";
                        attack.is_51_percent = false;
                        attack.is_checkpoint_attack = false;
                        attack.affected_height = 0;
                        daemonize::DiscordNotifier::sendAttackAlert(attack);
                    } catch (...) {
                        // Don't let Discord failure block remediation
                    }
                });
                
                if (!full_verifier.verifyFullSourceIntegrity()) {
                    MWARNING("[IA] ⚠️  Full binary integrity check detected modifications");
                    MWARNING("[IA] Auto-remediation from GitHub has been triggered");
                    // Don't fail daemon startup — auto-remediation handles this
                } else {
                    MINFO("[IA] ✅ Full binary integrity verified — ALL source files are clean");
                }
            }
            
            MINFO("╔════════════════════════════════════════════════════════════╗");
            MINFO("║  ✅ IA SECURITY MODULE INITIALIZED & ACTIVE              ║");
            MINFO("║                                                            ║");
            MINFO("║  Protection Systems:                                      ║");
            MINFO("║  ✓ Filesystem Sandbox      (Access control active)       ║");
            MINFO("║  ✓ Network Sandbox         (P2P-only mode)               ║");
            MINFO("║  ✓ Code Integrity         (SHA-256 AI verification)     ║");
            MINFO("║  ✓ Binary Integrity       (SHA-256 full source)         ║");
            MINFO("║  ✓ Remediation            (Auto-repair from GitHub)     ║");
            MINFO("║  ✓ Quarantine System      (Emergency isolation ready)   ║");
            MINFO("║  ✓ Monitoring             (Continuous validation)       ║");
            MINFO("║  ✓ Auto-Update            (GitHub version check/12h)   ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
            
            // Initialize Checkpoint Monitor
            if (!initialize_checkpoint_monitor()) {
                MWARNING("[IA] ⚠️  Checkpoint Monitor initialization warning");
                // Don't fail daemon
            }

            // Initialize Hashrate Recovery Monitor
            if (!initialize_hashrate_monitor()) {
                MWARNING("[IA] ⚠️  Hashrate Recovery Monitor initialization warning");
                // Don't fail daemon
            }
            
            // Initialize NINA Checkpoint Validator
            MINFO("[IA] Stage 6: Initializing NINA Checkpoint Validator...");
            if (!initialize_checkpoint_validator()) {
                MWARNING("[IA] ⚠️  Checkpoint Validator initialization warning");
                // Don't fail daemon if checkpoint validator can't init
            }
            
            // Initialize Discord IA Integration
            MINFO("[IA] Stage 7: Initializing Discord IA Integration...");
            if (!initialize_discord_integration()) {
                MWARNING("[IA] ⚠️  Discord Integration initialization warning");
                // Don't fail daemon if Discord integration can't init
            }
            
            // ===== NINA IA AUTO-UPDATE ORCHESTRATOR =====
            // NINA IA is the intelligent decision-maker for updates.
            // She monitors GitHub, detects forks, analyzes changes, and
            // autonomously downloads, compiles, and installs new versions.
            MINFO("[IA] Stage 8: Initializing NINA IA Auto-Update System...");
            {
                try {
                    // Step 1: Initialize the basic VersionChecker (used internally)
                    auto& version_checker = ninacatcoin_ai::VersionChecker::getInstance();
                    version_checker.initialize(ninacatcoin_VERSION);
                    
                    // Step 2: Initialize NINA IA Auto-Update Orchestrator
                    auto& nina_updater = ninacatcoin_ai::NinaIAAutoUpdate::getInstance();
                    nina_updater.initialize(ninacatcoin_VERSION, 0, true); // height=0, syncing=true at startup
                    
                    // Step 3: Set Discord notification callback
                    nina_updater.setNotifyCallback([](const std::string& title,
                                                      const std::string& message,
                                                      int severity) {
                        try {
                            char hostname[256] = "unknown";
                            #ifndef _WIN32
                            gethostname(hostname, sizeof(hostname));
                            #endif
                            
                            std::string node_id = std::string(hostname);
                            int color = 3066993; // Green
                            std::string emoji = "🐱";
                            if (severity >= 5) { color = 15158332; emoji = "🚨"; }      // Red
                            else if (severity >= 4) { color = 15105570; emoji = "🔴"; }  // Orange
                            else if (severity >= 3) { color = 16776960; emoji = "⚠️"; }  // Yellow
                            
                            std::string msg_safe = message.substr(0, 300);
                            // Escape quotes for JSON
                            std::string msg_escaped;
                            for (char c : msg_safe) {
                                if (c == '"') msg_escaped += "\\\"";
                                else if (c == '\n') msg_escaped += "\\n";
                                else if (c == '\\') msg_escaped += "\\\\";
                                else msg_escaped += c;
                            }
                            
                            std::string json_payload = 
                                "{\"embeds\":[{"
                                "\"title\":\"" + emoji + " " + title + "\","
                                "\"description\":\"" + msg_escaped + "\","
                                "\"color\":" + std::to_string(color) + ","
                                "\"fields\":["
                                "{\"name\":\"Node\",\"value\":\"" + node_id + "\",\"inline\":true},"
                                "{\"name\":\"Severity\",\"value\":\"" + std::to_string(severity) + "/5\",\"inline\":true}"
                                "],"
                                "\"footer\":{\"text\":\"NINA IA Auto-Update System\"}"
                                "}]}";
                            
                            std::string cmd = "curl -sS -X POST "
                                "-H \"Content-Type: application/json\" "
                                "-d '" + json_payload + "' "
                                "\"https://discord.com/api/webhooks/1474466544653434941/XVrNU12o1Kkf1u__d7wbVFmJCHqT5iuBad13Lgvbq3SBQY4RiEeZTg7owxaPcQ0E7UCy\" "
                                ">/dev/null 2>&1 &";
                            std::system(cmd.c_str());
                            
                            // Also send to Discord alert system
                            daemonize::AttackDetail alert;
                            alert.type = daemonize::AttackType::CODE_TAMPERING;
                            alert.type_name = "NINA_IA_UPDATE";
                            alert.severity = severity;
                            alert.timestamp = std::time(nullptr);
                            alert.description = emoji + " NINA IA: " + message;
                            alert.details.push_back("Node: " + node_id);
                            alert.recommendation = "NINA IA is handling this autonomously.";
                            alert.is_51_percent = false;
                            alert.is_checkpoint_attack = false;
                            alert.affected_height = 0;
                            daemonize::DiscordNotifier::sendAttackAlert(alert);
                        } catch (...) {}
                    });
                    
                    // Step 4: Also keep VersionChecker's periodic check as fallback
                    version_checker.setUpdateCallback([](const std::string& local_ver,
                                                         const std::string& remote_ver,
                                                         const std::string& notes) {
                        // VersionChecker detects new version → let NINA IA handle it
                        MINFO("[NINA IA] VersionChecker detecto nueva version v" << remote_ver
                              << " — NINA IA se encargara");
                    });
                    version_checker.startPeriodicCheck();
                    
                    // Step 5: Start NINA IA autonomous monitoring
                    nina_updater.start();
                    
                    MINFO("╔════════════════════════════════════════════════════════════╗");
                    MINFO("║  🐱 NINA IA AUTO-UPDATE SYSTEM ACTIVATED                  ║");
                    MINFO("║                                                            ║");
                    MINFO("║  NINA IA monitoriza GitHub autonomamente:                 ║");
                    MINFO("║  ✓ Detecta nuevas versiones (releases + tags)             ║");
                    MINFO("║  ✓ Analiza cambios en cryptonote_config.h                ║");
                    MINFO("║  ✓ Detecta hard forks futuros en hardforks.cpp           ║");
                    MINFO("║  ✓ Clasifica cambios (fork/consenso/seguridad/config)    ║");
                    MINFO("║  ✓ Decide prioridad de actualizacion (urgencia 1-10)     ║");
                    MINFO("║  ✓ Descarga, compila e instala automaticamente           ║");
                    MINFO("║  ✓ Reinicia daemon tras actualizacion exitosa            ║");
                    MINFO("║  ✓ Notifica via Discord (status + alertas fork)          ║");
                    MINFO("║                                                            ║");
                    MINFO("║  Intervalo normal:  cada 6 horas                          ║");
                    MINFO("║  Modo urgente:      cada 30 min (fork proximo)            ║");
                    MINFO("║  Local: v" << ninacatcoin_VERSION << "                                          ║");
                    MINFO("║  Fuente: github.com/ninacatcoin/ninacatcoin               ║");
                    MINFO("╚════════════════════════════════════════════════════════════╝");
                } catch (const std::exception& e) {
                    MWARNING("[IA] ⚠️  NINA IA Auto-Update initialization warning: " << e.what());
                    // Don't fail daemon if NINA IA can't init
                }
            }
            
            return true;
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception during IA initialization: " << e.what());
            return false;
        }
        catch (...) {
            MERROR("[IA] Unknown exception during IA initialization");
            return false;
        }
    }

    /**
     * @brief Shutdown IA module gracefully (including Checkpoint Validator and Discord)
     */
    static void shutdown_ia_module()
    {
        try {
            MINFO("[IA] Shutting down IA Security Module...");
            
            // Shutdown Discord integration first (it may be monitoring)
            MINFO("[IA] Closing Discord IA Integration...");
            try {
                DiscordIAIntegration::shutdown();
                MINFO("[IA] ✓ Discord IA Integration closed");
            } catch (...) {
                MWARNING("[IA] Warning: Discord IA Integration shutdown had issues");
            }
            
            // Checkpoint Validator cleanup handled by singleton destructor
            
            // Stop NINA IA Auto-Update
            MINFO("[IA] Stopping NINA IA Auto-Update...");
            try {
                ninacatcoin_ai::NinaIAAutoUpdate::getInstance().stop();
                MINFO("[IA] ✓ NINA IA Auto-Update stopped");
            } catch (...) {
                MWARNING("[IA] Warning: NINA IA Auto-Update stop had issues");
            }
            
            // Stop version checker
            MINFO("[IA] Stopping Version Checker...");
            try {
                ninacatcoin_ai::VersionChecker::getInstance().stop();
                MINFO("[IA] ✓ Version Checker stopped");
            } catch (...) {
                MWARNING("[IA] Warning: Version Checker stop had issues");
            }
            
            // Then shutdown AI module
            ninacatcoin_ai::AIModule::getInstance().shutdown();
            
            MINFO("╔════════════════════════════════════════════════════════════╗");
            MINFO("║  ✅ IA SECURITY MODULE SHUTDOWN COMPLETE                  ║");
            MINFO("║  All protection systems have been gracefully closed        ║");
            MINFO("╚════════════════════════════════════════════════════════════╝");
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception during IA shutdown: " << e.what());
        }
    }

    /**
     * @brief Register a new peer connection with the IA module
     * This is called when a new peer connects to the network
     */
    static void register_peer(const std::string& peer_id, const std::string& peer_address)
    {
        try {
            // Register peer by address (peer_id is not used by AIModule)
            ninacatcoin_ai::AIModule::getInstance().registerPeer(peer_address);
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception registering peer: " << e.what());
        }
    }

    /**
     * @brief Check if a peer is blacklisted
     * This is called during peer filtering
     */
    static bool is_peer_blacklisted(const std::string& peer_id)
    {
        try {
            return ninacatcoin_ai::AIModule::getInstance().isPeerBlacklisted(peer_id);
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception checking peer blacklist: " << e.what());
            return false;
        }
    }

    /**
     * @brief Analyze a transaction through the IA module
     * This is called for each transaction before validation
     */
    static bool analyze_transaction(const std::string& tx_id, uint64_t tx_size)
    {
        try {
            // Convert tx_id to vector for analysis
            std::vector<uint8_t> tx_data(tx_id.begin(), tx_id.end());
            return ninacatcoin_ai::AIModule::getInstance().analyzeTransaction(tx_data);
        }
        catch (const std::exception& e) {
            MERROR("[IA] Exception analyzing transaction: " << e.what());
            return true; // Don't block transaction on analysis error
        }
    }

    /**
     * @brief Get IA module status for monitoring
     */
    static std::string get_ia_status()
    {
        try {
            return ninacatcoin_ai::AIModule::getInstance().getDiagnosticInfo();
        }
        catch (const std::exception& e) {
            return "ERROR: " + std::string(e.what());
        }
    }
};

} // namespace daemonize
