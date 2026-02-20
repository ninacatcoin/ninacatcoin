// Copyright (c) 2026, The ninacatcoin Project
//
// Integrity verification - validates IA code hasn't been tampered

#pragma once

#include "ai_config.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>

namespace ninacatcoin_ai {

/**
 * @class IntegrityVerifier
 * @brief Validates IA code integrity against canonical hash
 * 
 * CRITICAL SECURITY:
 * - Calculates SHA256 hash of all IA source files
 * - Compares against canonical hash from seed nodes
 * - Detects any tampering/modification
 * - Consensus-based validation (2/3 seed nodes required)
 */
class IntegrityVerifier {
public:
    static IntegrityVerifier& getInstance();

    // Initialize verifier
    bool initialize();

    /**
     * Verify IA code integrity
     * Returns true if code is valid and approved by seed nodes
     */
    bool verifyAICodeIntegrity();

    /**
     * Calculate hash of local IA code
     */
    std::string calculateLocalCodeHash();

    /**
     * Get canonical hash from seed nodes
     */
    std::string getCanonicalHashFromSeeds();

    /**
     * Load cached canonical hash
     */
    std::string getCanonicalHashFromCache();

    /**
     * Query seed nodes for validation
     * Returns number of approvals (0-3)
     */
    int querySeedNodesForValidation(const std::string& code_hash);

    /**
     * Check if hash matches canonical
     */
    bool hashMatches(
        const std::string& local_hash,
        const std::string& canonical_hash
    );

    /**
     * Verify cryptographic signature from seed node
     */
    bool verifySignature(
        const std::string& message,
        const std::vector<uint8_t>& signature
    );

    /**
     * Generate proof of integrity
     */
    std::vector<uint8_t> generateIntegrityProof(
        const std::string& code_hash
    );

    /**
     * Get last verification result
     */
    std::string getLastError() const;

    // ======= P2P Hash Exchange =======

    /**
     * Get the compile-time AI code hash (static, no instance needed)
     */
    static const char* getCompiledHash();

    /**
     * Record a peer's AI code hash for consensus tracking
     * @param peer_id   Unique peer identifier
     * @param hash      Peer's reported AI code hash
     */
    void recordPeerHash(const std::string& peer_id, const std::string& hash);

    /**
     * Get network consensus on AI code hash
     * @return pair of (majority_hash, agreement_percentage 0.0-1.0)
     */
    std::pair<std::string, double> getNetworkConsensus() const;

    /**
     * Check if we should trigger auto-update (our hash is in minority)
     */
    bool shouldTriggerAutoUpdate() const;

    /**
     * Get count of peers that have been queried
     */
    int getPeerCount() const;

private:
    IntegrityVerifier();
    ~IntegrityVerifier();

    // Delete copy/move
    IntegrityVerifier(const IntegrityVerifier&) = delete;
    IntegrityVerifier& operator=(const IntegrityVerifier&) = delete;

    std::string last_error;
    std::string last_local_hash;
    std::string last_canonical_hash;
    bool is_initialized;

    // Hash all AI source files
    std::string hashAISourceFiles();

    // Files to be hashed (in order)
    std::vector<std::string> getAISourceFiles();

    // Calculate combined hash of multiple files
    std::string calculateCombinedHash(
        const std::vector<std::string>& files
    );

    // Contact seed node for validation
    bool contactSeedNode(
        const std::string& seed_ip,
        const std::string& code_hash,
        bool& approved
    );

    // Save canonical hash cache
    void cacheCanonicalHash(const std::string& hash);

    // Validate downloaded code from GitHub
    bool validateDownloadedCode(const std::string& code_hash);

    // P2P hash tracking
    mutable std::mutex m_peer_hash_mutex;
    std::map<std::string, std::string> m_peer_hashes;  // peer_id → hash
    std::map<std::string, int> m_hash_counts;           // hash → count
};

} // namespace ninacatcoin_ai
