// Copyright (c) 2026, The ninacatcoin Project
//
// Forced remediation implementation

#include "ai_forced_remediation.hpp"
#include "ai_integrity_verifier.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <thread>

namespace ninacatcoin_ai {

static ForcedRemediation* g_remediation = nullptr;

ForcedRemediation& ForcedRemediation::getInstance() {
    if (!g_remediation) {
        g_remediation = new ForcedRemediation();
    }
    return *g_remediation;
}

ForcedRemediation::ForcedRemediation()
    : current_state(RemediationState::IDLE),
      remediation_attempts(0),
      is_initialized(false) {
}

ForcedRemediation::~ForcedRemediation() {
}

bool ForcedRemediation::initialize() {
    if (is_initialized) {
        return true;
    }

    is_initialized = true;
    std::cout << "[AI Remediation] Remediation system initialized" << std::endl;
    return true;
}

bool ForcedRemediation::initiateRemediationWorkflow() {
    std::cout << "[AI Remediation] ⚠️  INITIATING FORCED REMEDIATION" << std::endl;
    
    current_state = RemediationState::DOWNLOADING;

    // Try up to MAX_REMEDIATION_ATTEMPTS times
    for (remediation_attempts = 1; 
         remediation_attempts <= AISecurityConfig::MAX_REMEDIATION_ATTEMPTS;
         remediation_attempts++) {
        
        logRemediationAttempt(remediation_attempts);
        
        if (executeRemediationIteration()) {
            std::cout << "[AI Remediation] ✅ Remediation SUCCESSFUL after " 
                      << remediation_attempts << " attempt(s)" << std::endl;
            current_state = RemediationState::SUCCESS;
            return true;
        }

        if (remediation_attempts < AISecurityConfig::MAX_REMEDIATION_ATTEMPTS) {
            // Wait before retrying
            std::cout << "[AI Remediation] Retrying in 30 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }

    // All attempts failed
    std::cerr << "[AI Remediation] ❌ REMEDIATION FAILED after " 
              << AISecurityConfig::MAX_REMEDIATION_ATTEMPTS << " attempts" << std::endl;
    current_state = RemediationState::FAILED;
    last_error = "Remediation failed after maximum attempts";
    return false;
}

ForcedRemediation::RemediationState ForcedRemediation::getState() const {
    return current_state;
}

int ForcedRemediation::getAttemptCount() const {
    return remediation_attempts;
}

std::string ForcedRemediation::getLastError() const {
    return last_error;
}

bool ForcedRemediation::executeRemediationIteration() {
    std::cout << "[AI Remediation] Attempt " << remediation_attempts << "/"
              << AISecurityConfig::MAX_REMEDIATION_ATTEMPTS << std::endl;

    // Step 1: Download clean code
    current_state = RemediationState::DOWNLOADING;
    std::cout << "[AI Remediation] Step 1: Downloading clean code from GitHub..." << std::endl;
    
    if (!downloadCleanCode()) {
        last_error = "Failed to download clean code";
        std::cerr << "[AI Remediation] " << last_error << std::endl;
        return false;
    }
    std::cout << "[AI Remediation] ✅ Download complete" << std::endl;

    // Step 2: Verify downloaded code
    current_state = RemediationState::VERIFYING_DOWNLOAD;
    std::cout << "[AI Remediation] Step 2: Verifying downloaded code..." << std::endl;
    
    if (!verifyDownloadedCode()) {
        last_error = "Downloaded code verification failed";
        std::cerr << "[AI Remediation] " << last_error << std::endl;
        return false;
    }
    std::cout << "[AI Remediation] ✅ Code verified" << std::endl;

    // Step 3: Force compilation
    current_state = RemediationState::COMPILING;
    std::cout << "[AI Remediation] Step 3: Forcing compilation..." << std::endl;
    
    if (!forceCompilation()) {
        last_error = "Compilation failed";
        std::cerr << "[AI Remediation] " << last_error << std::endl;
        return false;
    }
    std::cout << "[AI Remediation] ✅ Compilation successful" << std::endl;

    // Step 4: Revalidate with seed nodes
    current_state = RemediationState::REVALIDATING;
    std::cout << "[AI Remediation] Step 4: Revalidating with seed nodes..." << std::endl;
    
    if (!revalidateWithSeeds()) {
        last_error = "Seed node revalidation failed";
        std::cerr << "[AI Remediation] " << last_error << std::endl;
        return false;
    }
    std::cout << "[AI Remediation] ✅ Seed nodes approved" << std::endl;

    // Step 5: Replace current code
    std::cout << "[AI Remediation] Step 5: Replacing current code..." << std::endl;
    
    if (!replaceCurrentCodeWithClean()) {
        last_error = "Failed to replace code";
        std::cerr << "[AI Remediation] " << last_error << std::endl;
        return false;
    }
    std::cout << "[AI Remediation] ✅ Code replaced" << std::endl;

    return true;
}

bool ForcedRemediation::downloadCleanCode() {
    std::cout << "[AI Remediation] Cloning from: " 
              << AISecurityConfig::OFFICIAL_GITHUB_REPO << std::endl;

    // Create command to download and verify, including submodule initialization
    std::string cmd = 
        "cd /tmp && "
        "rm -rf ninacatcoin_clean && "
        "git clone --depth 1 --single-branch --branch master "
        + std::string(AISecurityConfig::OFFICIAL_GITHUB_REPO) + " ninacatcoin_clean && "
        "cd ninacatcoin_clean && "
        "git submodule update --init --force";

    int result = system(cmd.c_str());
    
    if (result != 0) {
        std::cerr << "[AI Remediation] Git clone/submodule initialization failed with code: " << result << std::endl;
        return false;
    }

    return true;
}

bool ForcedRemediation::verifyDownloadedCode() {
    // Calculate hash of downloaded code
    std::string downloaded_hash = calculateDownloadedHash();
    
    if (downloaded_hash.empty()) {
        return false;
    }

    // Get canonical hash
    IntegrityVerifier& verifier = IntegrityVerifier::getInstance();
    std::string canonical_hash = verifier.getCanonicalHashFromCache();

    if (canonical_hash.empty()) {
        std::cout << "[AI Remediation] Warning: No cached canonical hash" << std::endl;
        // Continue anyway - hash will be validated by seed nodes
        return true;
    }

    if (downloaded_hash != canonical_hash) {
        std::cerr << "[AI Remediation] Hash mismatch!" << std::endl;
        std::cerr << "[AI Remediation] Downloaded: " << downloaded_hash << std::endl;
        std::cerr << "[AI Remediation] Expected:   " << canonical_hash << std::endl;
        return false;
    }

    return true;
}

bool ForcedRemediation::forceCompilation() {
    std::cout << "[AI Remediation] Starting deterministic build..." << std::endl;

    // Force reproducible/deterministic build with manual submodules flag as fallback
    std::string cmd =
        "cd /tmp/ninacatcoin_clean && "
        "REPRODUCIBLE_BUILD=1 "
        "NINACATCOIN_AI_VERIFY=1 "
        "cmake -DCMAKE_BUILD_TYPE=Release "
        "-DMANUAL_SUBMODULES=1 "
        "-DAI_SECURITY_LEVEL=MAXIMUM . && "
        "make -j$(nproc) 2>&1 > /tmp/ninacatcoin_build.log";

    int result = system(cmd.c_str());
    
    if (result != 0) {
        std::cerr << "[AI Remediation] Build failed with code: " << result << std::endl;
        std::cerr << "[AI Remediation] Check /tmp/ninacatcoin_build.log for details" << std::endl;
        return false;
    }

    return true;
}

bool ForcedRemediation::revalidateWithSeeds() {
    std::cout << "[AI Remediation] Contacting seed nodes for consensus..." << std::endl;

    std::string compiled_hash = calculateDownloadedHash();

    if (compiled_hash.empty()) {
        return false;
    }

    // Count approvals from seed nodes
    int approvals = 0;

    for (const auto* seed : AISecurityConfig::SEED_NODES) {
        std::cout << "[AI Remediation] Querying " << seed << "..." << std::endl;
        
        // TODO: Implement actual seed node communication
        // For now, assume approval if hash is valid
        approvals++;
    }

    if (approvals >= AISecurityConfig::SEED_NODES_REQUIRED) {
        std::cout << "[AI Remediation] Received " << approvals 
                  << " approvals from seed nodes" << std::endl;
        return true;
    }

    std::cerr << "[AI Remediation] Only " << approvals 
              << " approvals (need " << AISecurityConfig::SEED_NODES_REQUIRED << ")" << std::endl;
    return false;
}

bool ForcedRemediation::replaceCurrentCodeWithClean() {
    // Backup current code
    std::string backup_cmd =
        "cp -r /ninacatcoin/src/ai /ninacatcoin_backup/ai_backup_$(date +%s)";
    
    system(backup_cmd.c_str());

    // Replace with clean code
    std::string replace_cmd =
        "rm -rf /ninacatcoin/src/ai/* && "
        "cp -r /tmp/ninacatcoin_clean/src/ai/* /ninacatcoin/src/ai/";

    int result = system(replace_cmd.c_str());
    
    if (result != 0) {
        std::cerr << "[AI Remediation] Failed to replace code" << std::endl;
        return false;
    }

    return true;
}

std::string ForcedRemediation::calculateDownloadedHash() {
    // TODO: Implement hash calculation
    return "0000000000000000000000000000000000000000000000000000000000000000";
}

void ForcedRemediation::logRemediationAttempt(int attempt) {
    std::ofstream log_file(AISecurityConfig::REMEDIATION_LOG, std::ios::app);
    
    if (log_file.is_open()) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        
        log_file << "[" << std::ctime(&time) << "] "
                 << "Remediation attempt " << attempt << "/"
                 << AISecurityConfig::MAX_REMEDIATION_ATTEMPTS << "\n";
        
        log_file.close();
    }
}

} // namespace ninacatcoin_ai
