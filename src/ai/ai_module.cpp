// Copyright (c) 2026, The ninacatcoin Project
//
// AI Module implementation

#include "ai_module.hpp"
#include "ai_lwma_learning.hpp"
#include "full_integrity_verifier.hpp"
#include <iostream>
#include <thread>
#include <chrono>

namespace ninacatcoin_ai {

static AIModule* g_ai_instance = nullptr;

AIModule::AIModule()
    : current_state(ModuleState::UNINITIALIZED),
      is_active(false) {
}

AIModule::~AIModule() {
    shutdown();
}

AIModule& AIModule::getInstance() {
    if (!g_ai_instance) {
        g_ai_instance = new AIModule();
    }
    return *g_ai_instance;
}

bool AIModule::initialize() {
    if (current_state != ModuleState::UNINITIALIZED) {
        last_error = "AI module already initialized";
        return false;
    }

    current_state = ModuleState::VALIDATING;
    std::cout << "[AI] Starting initialization..." << std::endl;

    // Step 1: Load configuration
    if (!loadConfiguration()) {
        last_error = "Failed to load AI configuration";
        current_state = ModuleState::DISABLED;
        std::cerr << "[AI] " << last_error << std::endl;
        return false;
    }
    std::cout << "[AI] Configuration loaded" << std::endl;

    // Step 2: Validate code integrity
    if (!validateCodeIntegrity()) {
        last_error = "Code integrity validation failed";
        std::cerr << "[AI] " << last_error << std::endl;
        std::cout << "[AI] Initiating forced remediation..." << std::endl;
        
        current_state = ModuleState::REMEDIATION;
        
        // Attempt forced remediation
        ForcedRemediation& remediation = ForcedRemediation::getInstance();
        if (!remediation.initialize()) {
            last_error = "Failed to initialize remediation";
            current_state = ModuleState::DISABLED;
            return false;
        }

        if (!remediation.initiateRemediationWorkflow()) {
            last_error = "Remediation workflow failed";
            std::cerr << "[AI] Remediation failed after 3 attempts" << std::endl;
            
            // Implement quarantine
            QuarantineSystem& quarantine = QuarantineSystem::getInstance();
            if (quarantine.initialize()) {
                quarantine.implementQuarantine();
                current_state = ModuleState::QUARANTINED;
            }
            return false;
        }
        
        // After remediation, verify again
        if (!validateCodeIntegrity()) {
            last_error = "Code still invalid after remediation";
            current_state = ModuleState::DISABLED;
            return false;
        }
        
        std::cout << "[AI] Code remediation successful" << std::endl;
    }
    std::cout << "[AI] Code integrity verified" << std::endl;

    // Step 3: Initialize filesystem sandbox
    FileSystemSandbox& fs_sandbox = FileSystemSandbox::getInstance();
    if (!fs_sandbox.initialize()) {
        last_error = "Failed to initialize filesystem sandbox";
        current_state = ModuleState::DISABLED;
        std::cerr << "[AI] " << last_error << std::endl;
        return false;
    }
    std::cout << "[AI] Filesystem sandbox initialized" << std::endl;

    // Step 4: Initialize network sandbox
    NetworkSandbox& net_sandbox = NetworkSandbox::getInstance();
    if (!net_sandbox.initialize()) {
        last_error = "Failed to initialize network sandbox";
        current_state = ModuleState::DISABLED;
        std::cerr << "[AI] " << last_error << std::endl;
        return false;
    }
    std::cout << "[AI] Network sandbox initialized" << std::endl;

    // Step 5: Perform security checks
    if (!performSecurityChecks()) {
        last_error = "Security checks failed";
        current_state = ModuleState::DISABLED;
        std::cerr << "[AI] " << last_error << std::endl;
        return false;
    }
    std::cout << "[AI] Security checks passed" << std::endl;

    // Step 6: Initialize monitoring
    initializeMonitoring();

    current_state = ModuleState::ACTIVE;
    is_active = true;

    std::cout << "[AI] ✅ Module initialized successfully" << std::endl;
    std::cout << "[AI] Version: " << getVersion() << std::endl;
    return true;
}

void AIModule::shutdown() {
    if (is_active) {
        std::cout << "[AI] Shutting down..." << std::endl;
        is_active = false;
        current_state = ModuleState::DISABLED;
        std::cout << "[AI] Shutdown complete" << std::endl;
    }
}

bool AIModule::isActive() const {
    return is_active && (current_state == ModuleState::ACTIVE);
}

AIModule::ModuleState AIModule::getState() const {
    return current_state;
}

std::string AIModule::getStateName() const {
    switch (current_state) {
        case ModuleState::UNINITIALIZED:
            return "UNINITIALIZED";
        case ModuleState::VALIDATING:
            return "VALIDATING";
        case ModuleState::ACTIVE:
            return "ACTIVE";
        case ModuleState::DISABLED:
            return "DISABLED";
        case ModuleState::QUARANTINED:
            return "QUARANTINED";
        case ModuleState::REMEDIATION:
            return "REMEDIATION";
        case ModuleState::CRITICAL_ERROR:
            return "CRITICAL_ERROR";
        default:
            return "UNKNOWN";
    }
}

std::string AIModule::getVersion() const {
    return AISecurityConfig::AI_MODULE_VERSION;
}

std::string AIModule::getLastError() const {
    return last_error;
}

void AIModule::disable(const std::string& reason) {
    is_active = false;
    current_state = ModuleState::DISABLED;
    last_error = reason;
    std::cerr << "[AI] Module disabled: " << reason << std::endl;
}

bool AIModule::validateCodeIntegrity() {
    IntegrityVerifier& verifier = IntegrityVerifier::getInstance();
    if (!verifier.initialize()) {
        last_error = "Failed to initialize integrity verifier";
        return false;
    }
    return verifier.verifyAICodeIntegrity();
}

void AIModule::registerPeer(const std::string& peer_ip) {
    if (!isActive()) {
        return;
    }
    
    NetworkSandbox& net_sandbox = NetworkSandbox::getInstance();
    net_sandbox.registerValidPeer(peer_ip);
    
    AIPeerMonitor& monitor = AIPeerMonitor::getInstance();
    monitor.registerConnection(peer_ip);
}

void AIModule::blacklistPeer(
    const std::string& peer_ip,
    const std::string& reason
) {
    NetworkSandbox& net_sandbox = NetworkSandbox::getInstance();
    net_sandbox.addToBlacklist(peer_ip, reason);
    
    GlobalBlacklist& blacklist = GlobalBlacklist::getInstance();
    blacklist.addToBlacklist(peer_ip, reason);
}

bool AIModule::analyzeTransaction(const std::vector<uint8_t>& tx_data) {
    if (!isActive()) {
        return false;
    }
    
    AIAnomalyDetector& detector = AIAnomalyDetector::getInstance();
    return !detector.isTransactionAnomaly(tx_data);
}

bool AIModule::isPeerBlacklisted(const std::string& peer_ip) {
    if (!isActive()) {
        return false;
    }
    
    NetworkSandbox& net_sandbox = NetworkSandbox::getInstance();
    return net_sandbox.isPeerBlacklisted(peer_ip);
}

std::vector<std::string> AIModule::getMonitoredPeers() const {
    if (!isActive()) {
        return {};
    }
    
    NetworkSandbox& net_sandbox = NetworkSandbox::getInstance();
    return net_sandbox.getValidPeers();
}

bool AIModule::verifySandboxesActive() {
    FileSystemSandbox& fs_sandbox = FileSystemSandbox::getInstance();
    NetworkSandbox& net_sandbox = NetworkSandbox::getInstance();
    
    return fs_sandbox.verifySandboxActive() &&
           net_sandbox.verifySandboxActive();
}

std::string AIModule::getDiagnosticInfo() const {
    std::string info;
    info += "AI Module Diagnostics:\n";
    info += "  State: " + getStateName() + "\n";
    info += "  Active: " + std::string(isActive() ? "Yes" : "No") + "\n";
    info += "  Version: " + getVersion() + "\n";
    if (!last_error.empty()) {
        info += "  Last Error: " + last_error + "\n";
    }
    return info;
}

bool AIModule::loadConfiguration() {
    // Load AI module configuration
    // For now, just return true (default config)
    return true;
}

bool AIModule::performSecurityChecks() {
    // Verify all sandboxes are operational
    return verifySandboxesActive();
}

void AIModule::initializeMonitoring() {
    // Initialize LWMA-1 learning module
    AILWMALearning::initialize();
    MGINFO("[AI] LWMA-1 Learning module initialized");
    
    // Start monitoring thread for continuous integrity checks
    std::thread monitor_thread([this]() {
        monitoringLoop();
    });
    monitor_thread.detach();
}

void AIModule::monitoringLoop() {
    int full_check_counter = 0;  // Counter for full binary integrity checks
    
    while (is_active && current_state == ModuleState::ACTIVE) {
        try {
            // Every 60 seconds, verify AI code integrity
            std::this_thread::sleep_for(
                std::chrono::seconds(AISecurityConfig::INTEGRITY_CHECK_INTERVAL)
            );
            
            if (!validateCodeIntegrity()) {
                std::cerr << "[AI] Code tampering detected during monitoring!" << std::endl;
                disable("Code integrity check failed during monitoring");
                break;
            }
            
            // Every 5 minutes (5 cycles of 60s), verify FULL binary integrity
            full_check_counter++;
            if (full_check_counter >= 5) {
                full_check_counter = 0;
                
                auto& full_verifier = ninacatcoin_integrity::FullIntegrityVerifier::getInstance();
                if (!full_verifier.verifyFullSourceIntegrity()) {
                    std::cerr << "[AI] FULL BINARY tampering detected during monitoring!" << std::endl;
                    std::cerr << "[AI] Auto-remediation from GitHub triggered!" << std::endl;
                    // Don't disable — let the auto-remediation handle it
                    // The daemon will be restarted after recompilation
                }
            }
            
            // Verify sandboxes still active
            if (!verifySandboxesActive()) {
                std::cerr << "[AI] Sandbox violation detected!" << std::endl;
                disable("Sandbox integrity check failed");
                break;
            }
        } catch (const std::exception& e) {
            std::cerr << "[AI] Monitoring error: " << e.what() << std::endl;
            disable("Monitoring exception");
            break;
        }
    }
}

// AIAnomalyDetector implementation

static AIAnomalyDetector* g_anomaly_detector = nullptr;

AIAnomalyDetector& AIAnomalyDetector::getInstance() {
    if (!g_anomaly_detector) {
        g_anomaly_detector = new AIAnomalyDetector();
    }
    return *g_anomaly_detector;
}

AIAnomalyDetector::AIAnomalyDetector() {
}

AIAnomalyDetector::~AIAnomalyDetector() {
}

bool AIAnomalyDetector::isTransactionAnomaly(
    const std::vector<uint8_t>& tx_data
) {
    // TODO: Implement transaction anomaly detection
    // For now, accept all transactions
    return false;
}

bool AIAnomalyDetector::isPeerBehaviorSuspicious(const std::string& peer_ip) {
    // TODO: Implement peer behavior analysis
    return false;
}

int AIAnomalyDetector::getPeerReputation(const std::string& peer_ip) {
    // TODO: Implement reputation scoring
    // Return 50 (unknown) by default
    return 50;
}

// AIPeerMonitor implementation

static AIPeerMonitor* g_peer_monitor = nullptr;

AIPeerMonitor& AIPeerMonitor::getInstance() {
    if (!g_peer_monitor) {
        g_peer_monitor = new AIPeerMonitor();
    }
    return *g_peer_monitor;
}

AIPeerMonitor::AIPeerMonitor() {
}

AIPeerMonitor::~AIPeerMonitor() {
}

void AIPeerMonitor::registerConnection(const std::string& peer_ip) {
    // TODO: Register peer connection for monitoring
}

void AIPeerMonitor::updatePeerStats(
    const std::string& peer_ip,
    int blocks_received,
    int transactions_received,
    int validation_time_ms
) {
    // TODO: Update peer statistics
}

std::string AIPeerMonitor::getPeerStats(const std::string& peer_ip) {
    // TODO: Return peer statistics
    return "";
}

} // namespace ninacatcoin_ai
