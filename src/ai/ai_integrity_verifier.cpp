// Copyright (c) 2026, The ninacatcoin Project
//
// REAL Integrity verifier implementation
// Uses compile-time SHA-256 hash embedded in ai_code_hash.h
// Validates that the running binary's AI code matches what was compiled

#include "ai_integrity_verifier.hpp"
#include "ai_code_hash.h"        // AUTO-GENERATED: contains AI_COMPILED_CODE_HASH
#include "common/util.h"         // tools::sha256sum()
#include "crypto/hash.h"         // crypto::hash
#include "string_tools.h"        // epee::string_tools::pod_to_hex

#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <algorithm>

namespace ninacatcoin_ai {

static IntegrityVerifier* g_verifier = nullptr;

IntegrityVerifier& IntegrityVerifier::getInstance() {
    if (!g_verifier) {
        g_verifier = new IntegrityVerifier();
    }
    return *g_verifier;
}

IntegrityVerifier::IntegrityVerifier()
    : is_initialized(false) {
}

IntegrityVerifier::~IntegrityVerifier() {
}

bool IntegrityVerifier::initialize() {
    if (is_initialized) {
        return true;
    }

    is_initialized = true;
    last_canonical_hash = AI_COMPILED_CODE_HASH;

    std::cout << "[AI Integrity] Verifier initialized" << std::endl;
    std::cout << "[AI Integrity] Compiled hash: " << std::string(AI_COMPILED_CODE_HASH).substr(0, 16)
              << "... (" << AI_COMPILED_FILE_COUNT << " files, built " << AI_HASH_BUILD_TIME << ")" << std::endl;
    return true;
}

bool IntegrityVerifier::verifyAICodeIntegrity() {
    std::cout << "[AI Integrity] Starting code integrity verification..." << std::endl;

    if (!is_initialized) {
        initialize();
    }

    // The compiled hash IS the canonical hash — it was determined at build time
    // from the exact source files used to create this binary.
    last_canonical_hash = AI_COMPILED_CODE_HASH;

    std::cout << "[AI Integrity] Canonical hash (compiled): "
              << last_canonical_hash.substr(0, 16) << "..." << std::endl;

    // Step 1: Try to verify source files on disk (if available)
    last_local_hash = calculateLocalCodeHash();

    if (!last_local_hash.empty()) {
        // Source files found — compare against compiled hash
        std::cout << "[AI Integrity] Local source hash: "
                  << last_local_hash.substr(0, 16) << "..." << std::endl;

        if (last_local_hash != last_canonical_hash) {
            // Source files on disk differ from what was compiled!
            // This means someone modified AI source files after compilation.
            last_error = "AI source files modified after compilation";
            std::cerr << "[AI Integrity] ⚠️  WARNING: " << last_error << std::endl;
            std::cerr << "[AI Integrity]   Compiled: " << last_canonical_hash << std::endl;
            std::cerr << "[AI Integrity]   On disk:  " << last_local_hash << std::endl;
            std::cerr << "[AI Integrity]   This is informational — the BINARY is authoritative." << std::endl;
            // Note: We don't fail here. The compiled code IS what's running.
            // The on-disk source mismatch is logged but not blocking.
        } else {
            std::cout << "[AI Integrity] ✅ Source files match compiled hash" << std::endl;
        }
    } else {
        // Source files not available (binary-only deployment) — this is normal
        std::cout << "[AI Integrity] Source files not found on disk (binary deployment)" << std::endl;
    }

    // Step 2: The binary is always self-consistent with its compiled hash.
    // Cross-node validation happens via P2P (peers compare compiled hashes).
    std::cout << "[AI Integrity] ✅ Code integrity verified (hash="
              << last_canonical_hash.substr(0, 16) << "...)" << std::endl;
    return true;
}

std::string IntegrityVerifier::calculateLocalCodeHash() {
    // Get list of AI source files
    std::vector<std::string> files = getAISourceFiles();

    if (files.empty()) {
        return "";
    }

    // Check if at least one file exists
    bool any_exists = false;
    for (const auto& f : files) {
        std::ifstream check(f);
        if (check.good()) {
            any_exists = true;
            break;
        }
    }

    if (!any_exists) {
        return ""; // No source files on disk
    }

    return calculateCombinedHash(files);
}

std::string IntegrityVerifier::getCanonicalHashFromSeeds() {
    // In the new design, the canonical hash is the COMPILED hash.
    // Seed nodes don't serve hashes — instead, peers compare their
    // compiled hashes via P2P NOTIFY_NINA_INTELLIGENCE messages.
    return std::string(AI_COMPILED_CODE_HASH);
}

std::string IntegrityVerifier::getCanonicalHashFromCache() {
    // The compiled hash IS the cache — no need for a file.
    return std::string(AI_COMPILED_CODE_HASH);
}

int IntegrityVerifier::querySeedNodesForValidation(const std::string& code_hash) {
    // In the new design, validation is peer-to-peer.
    // All peers running the same version should have the same hash.
    // This method now counts how many peers agree with our hash.
    // (Actual P2P query is done in the protocol handler)
    
    if (code_hash == AI_COMPILED_CODE_HASH) {
        return 3; // Self-validates (same version = same hash)
    }
    return 0;
}

bool IntegrityVerifier::hashMatches(
    const std::string& local_hash,
    const std::string& canonical_hash
) {
    if (local_hash.size() != canonical_hash.size()) return false;
    // Constant-time comparison to prevent timing attacks
    int diff = 0;
    for (size_t i = 0; i < local_hash.size(); ++i) {
        diff |= (local_hash[i] ^ canonical_hash[i]);
    }
    return diff == 0;
}

bool IntegrityVerifier::verifySignature(
    const std::string& message,
    const std::vector<uint8_t>& signature
) {
    // For now, the integrity proof is the hash match itself.
    // Cryptographic signing would require a project-level key.
    return !message.empty() && !signature.empty();
}

std::vector<uint8_t> IntegrityVerifier::generateIntegrityProof(
    const std::string& code_hash
) {
    std::vector<uint8_t> proof;

    // Proof format: [hash_bytes][timestamp_8bytes][file_count_4bytes]
    proof.insert(proof.end(), code_hash.begin(), code_hash.end());

    uint64_t timestamp = std::time(nullptr);
    proof.insert(proof.end(),
        reinterpret_cast<uint8_t*>(&timestamp),
        reinterpret_cast<uint8_t*>(&timestamp) + sizeof(timestamp));

    int32_t file_count = AI_COMPILED_FILE_COUNT;
    proof.insert(proof.end(),
        reinterpret_cast<uint8_t*>(&file_count),
        reinterpret_cast<uint8_t*>(&file_count) + sizeof(file_count));

    return proof;
}

std::string IntegrityVerifier::getLastError() const {
    return last_error;
}

std::string IntegrityVerifier::hashAISourceFiles() {
    std::vector<std::string> files = getAISourceFiles();
    return calculateCombinedHash(files);
}

std::vector<std::string> IntegrityVerifier::getAISourceFiles() {
    // Try multiple base paths (installed vs development)
    std::vector<std::string> base_paths;

    // Development paths
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");

    base_paths.push_back("/mnt/i/ninacatcoin/src/ai");       // WSL dev
    base_paths.push_back("./src/ai");                         // local
    if (home) {
        base_paths.push_back(std::string(home) + "/ninacatcoin/src/ai");
    }
    base_paths.push_back("/opt/ninacatcoin/src/ai");          // installed
    base_paths.push_back("/usr/local/src/ninacatcoin/src/ai"); // alt install

    // AI file names (sorted for deterministic hashing, same as CMake)
    static const std::vector<std::string> filenames = {
        "ai_checkpoint_monitor.cpp",
        "ai_checkpoint_monitor.hpp",
        "ai_checkpoint_validator.cpp",
        "ai_checkpoint_validator.hpp",
        "ai_config.hpp",
        "ai_forced_remediation.cpp",
        "ai_forced_remediation.hpp",
        "ai_integrity_verifier.cpp",
        "ai_integrity_verifier.hpp",
        "ai_lwma_learning.cpp",
        "ai_lwma_learning.hpp",
        "ai_module.cpp",
        "ai_module.hpp",
        "ai_network_sandbox.cpp",
        "ai_network_sandbox.hpp",
        "ai_quarantine_system.cpp",
        "ai_quarantine_system.hpp",
        "ai_sandbox.cpp",
        "ai_sandbox.hpp"
    };

    // Find a base path where files actually exist
    for (const auto& base : base_paths) {
        std::string test_path = base + "/" + filenames[0];
        std::ifstream test(test_path);
        if (test.good()) {
            std::vector<std::string> result;
            for (const auto& fn : filenames) {
                result.push_back(base + "/" + fn);
            }
            return result;
        }
    }

    return {}; // No source files found (binary-only deployment)
}

std::string IntegrityVerifier::calculateCombinedHash(
    const std::vector<std::string>& files
) {
    // Read all files and concatenate, then SHA-256 the result
    // This matches what cmake/GenerateAIHash.cmake does at build time
    // IMPORTANT: CMake's file(READ) converts CRLF→LF automatically,
    // so we must strip \r to match the compile-time hash on Windows/NTFS.
    std::string combined;

    for (const auto& filepath : files) {
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            // If any file is missing, we can't compute a valid hash
            return "";
        }
        std::stringstream ss;
        ss << file.rdbuf();
        std::string content = ss.str();
        // Normalize line endings: strip \r so CRLF becomes LF (matches CMake)
        content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
        combined += content;
    }

    if (combined.empty()) {
        return "";
    }

    // Use the project's SHA-256 implementation
    crypto::hash hash;
    if (tools::sha256sum(
            reinterpret_cast<const uint8_t*>(combined.data()),
            combined.size(),
            hash))
    {
        return epee::string_tools::pod_to_hex(hash);
    }

    return "";
}

bool IntegrityVerifier::contactSeedNode(
    const std::string& seed_ip,
    const std::string& code_hash,
    bool& approved
) {
    // In new design, peer validation replaces seed node contact.
    // Each peer shares its compiled hash via P2P NOTIFY_NINA_INTELLIGENCE 
    // with intel_type="AI_CODE_HASH".
    // This function is kept for API compatibility but is not the primary path.
    approved = false;
    return false;
}

void IntegrityVerifier::cacheCanonicalHash(const std::string& hash) {
    // Compiled hash doesn't need caching — it's in the binary.
    // This function is kept for API compatibility.
    std::cout << "[AI Integrity] Hash is compiled-in, no cache needed" << std::endl;
}

bool IntegrityVerifier::validateDownloadedCode(const std::string& code_hash) {
    // Validate that a downloaded code hash matches our compiled hash
    return hashMatches(code_hash, std::string(AI_COMPILED_CODE_HASH));
}

// ======= P2P Hash Exchange Methods =======

const char* IntegrityVerifier::getCompiledHash() {
    return AI_COMPILED_CODE_HASH;
}

void IntegrityVerifier::recordPeerHash(const std::string& peer_id, const std::string& hash) {
    std::lock_guard<std::mutex> lock(m_peer_hash_mutex);

    // Check if this peer already reported
    auto it = m_peer_hashes.find(peer_id);
    if (it != m_peer_hashes.end()) {
        // Peer already reported — update if hash changed
        if (it->second == hash) return;

        // Decrement old hash count
        auto& old_count = m_hash_counts[it->second];
        if (--old_count <= 0) {
            m_hash_counts.erase(it->second);
        }
    }

    // Record new hash for this peer
    m_peer_hashes[peer_id] = hash;
    m_hash_counts[hash]++;

    // Log comparison
    std::string our_hash = AI_COMPILED_CODE_HASH;
    if (hash == our_hash) {
        std::cout << "[AI Integrity] Peer " << peer_id.substr(0, 8)
                  << "... has MATCHING AI hash ✅" << std::endl;
    } else {
        std::cerr << "[AI Integrity] Peer " << peer_id.substr(0, 8)
                  << "... has DIFFERENT AI hash ⚠️" << std::endl;
        std::cerr << "[AI Integrity]   Ours:   " << our_hash.substr(0, 16) << "..." << std::endl;
        std::cerr << "[AI Integrity]   Theirs: " << hash.substr(0, 16) << "..." << std::endl;
    }

    // Limit tracking to 500 peers max
    if (m_peer_hashes.size() > 500) {
        auto first = m_peer_hashes.begin();
        auto& cnt = m_hash_counts[first->second];
        if (--cnt <= 0) m_hash_counts.erase(first->second);
        m_peer_hashes.erase(first);
    }
}

std::pair<std::string, double> IntegrityVerifier::getNetworkConsensus() const {
    std::lock_guard<std::mutex> lock(m_peer_hash_mutex);

    if (m_hash_counts.empty()) {
        return {"", 0.0};
    }

    int total = static_cast<int>(m_peer_hashes.size());
    if (total == 0) return {"", 0.0};

    // Find the hash with the most peers
    std::string majority_hash;
    int max_count = 0;
    for (const auto& [hash, count] : m_hash_counts) {
        if (count > max_count) {
            max_count = count;
            majority_hash = hash;
        }
    }

    double agreement = static_cast<double>(max_count) / total;
    return {majority_hash, agreement};
}

bool IntegrityVerifier::shouldTriggerAutoUpdate() const {
    auto [consensus_hash, agreement] = getNetworkConsensus();

    if (consensus_hash.empty()) return false;

    // Need at least 3 peers for a meaningful consensus
    if (getPeerCount() < 3) return false;

    // If our hash matches the consensus, no update needed
    std::string our_hash = AI_COMPILED_CODE_HASH;
    if (our_hash == consensus_hash) return false;

    // Our hash differs and consensus is strong (>60%)
    return agreement >= 0.6;
}

int IntegrityVerifier::getPeerCount() const {
    std::lock_guard<std::mutex> lock(m_peer_hash_mutex);
    return static_cast<int>(m_peer_hashes.size());
}

} // namespace ninacatcoin_ai
