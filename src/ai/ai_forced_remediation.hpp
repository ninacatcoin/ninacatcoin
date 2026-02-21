// Copyright (c) 2026, The ninacatcoin Project
//
// Forced remediation - auto-repair of corrupted/altered IA code

#pragma once

#include "ai_config.hpp"
#include <string>
#include <vector>

namespace ninacatcoin_ai {

/**
 * @class ForcedRemediation
 * @brief Automatic code remediation when validation fails
 * 
 * CRITICAL SECURITY:
 * - Downloads clean code directly from GitHub
 * - Forces recompilation with deterministic build
 * - Re-validates with seed nodes
 * - Up to 3 automatic attempts
 * - Quarantines node if all attempts fail
 */
class ForcedRemediation {
public:
    static ForcedRemediation& getInstance();

    enum class RemediationState {
        IDLE,
        DOWNLOADING,
        VERIFYING_DOWNLOAD,
        COMPILING,
        REVALIDATING,
        SUCCESS,
        FAILED
    };

    // Initialize remediation system
    bool initialize();

    /**
     * Initiate forced remediation workflow
     * Called when code validation fails
     */
    bool initiateRemediationWorkflow();

    /**
     * Get current state
     */
    RemediationState getState() const;

    /**
     * Get remediation attempt count
     */
    int getAttemptCount() const;

    /**
     * Get last error message
     */
    std::string getLastError() const;

private:
    ForcedRemediation();
    ~ForcedRemediation();

    // Delete copy/move
    ForcedRemediation(const ForcedRemediation&) = delete;
    ForcedRemediation& operator=(const ForcedRemediation&) = delete;

    RemediationState current_state;
    int remediation_attempts;
    std::string last_error;
    std::string m_secure_tmp_dir;  // Secure mkdtemp-created temp directory
    bool is_initialized;

    // Remediation steps (private implementation)
    
    /**
     * Step 1: Download clean code from GitHub
     */
    bool downloadCleanCode();

    /**
     * Step 2: Verify downloaded code integrity
     */
    bool verifyDownloadedCode();

    /**
     * Step 3: Force compilation with deterministic build
     */
    bool forceCompilation();

    /**
     * Step 4: Revalidate with seed nodes
     */
    bool revalidateWithSeeds();

    /**
     * Step 5: Replace current code with clean version
     */
    bool replaceCurrentCodeWithClean();

    // Helper functions
    std::string calculateDownloadedHash();
    
    bool executeRemediationIteration();
    
    void logRemediationAttempt(int attempt);
};

} // namespace ninacatcoin_ai
