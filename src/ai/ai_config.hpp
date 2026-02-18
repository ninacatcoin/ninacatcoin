// Copyright (c) 2026, The ninacatcoin Project
//
// Immutable security configuration for AI module

#pragma once

#include <string>
#include <array>
#include <set>
#include <cstdint>

namespace ninacatcoin_ai {

// KB, MB, GB helper literals - MUST be defined BEFORE usage
constexpr uint64_t operator""_KB(unsigned long long x) {
    return x * 1024ULL;
}

constexpr uint64_t operator""_MB(unsigned long long x) {
    return x * 1024ULL * 1024ULL;
}

constexpr uint64_t operator""_GB(unsigned long long x) {
    return x * 1024ULL * 1024ULL * 1024ULL;
}

/**
 * CRITICAL SECURITY CONFIG
 * These values are HARDCODED at compile time
 * NOT modifiable at runtime by user
 */
class AISecurityConfig {
public:
    // GitHub repository (official source)
    static constexpr const char* OFFICIAL_GITHUB_REPO = 
        "https://github.com/ninacatcoin/ninacatcoin.git";

    // Seed nodes for validation (immutable)
    static constexpr const char* SEED_NODES[] = {
        "seed1.ninacatcoin.com",
        "seed2.ninacatcoin.com",
        "seed3.ninacatcoin.com"
    };

    // Maximum remediation attempts before quarantine
    static constexpr int MAX_REMEDIATION_ATTEMPTS = 3;

    // Validation timeout (seconds)
    static constexpr int VALIDATION_TIMEOUT = 10;

    // Filesystem whitelist - ALLOWED paths
    static constexpr const char* ALLOWED_PATHS[] = {
        "/ninacatcoin/src/ai/",
        "/ninacatcoin/build/",
        "/ninacatcoin/blockchain/",
        "/ninacatcoin_data/ai_module/"
    };

    // Network whitelist - ONLY P2P
    static constexpr int P2P_PORT_MIN = 30000;
    static constexpr int P2P_PORT_MAX = 30100;

    // Memory constraints
    static constexpr uint64_t MAX_AI_MEMORY_BYTES = 2_GB; // 2 GB
    static constexpr uint64_t MEMORY_WARNING_THRESHOLD = 1900_MB; // 95%

    // CPU constraints (cores)
    static constexpr int MAX_CPU_CORES = 2;

    // Hash algorithm for integrity checking
    static constexpr const char* HASH_ALGORITHM = "sha256";

    // Consensus requirement from seed nodes
    static constexpr int SEED_NODES_REQUIRED = 2; // 2 out of 3

    // Monitoring interval (seconds)
    static constexpr int INTEGRITY_CHECK_INTERVAL = 60; // 1 minute

    // Blacklist check interval (hours)
    static constexpr int BLACKLIST_SYNC_INTERVAL = 24;

    // Quarantine detection parameters
    static constexpr int MAX_FAILED_VALIDATIONS = 3;

    // File access audit log location
    static constexpr const char* AUDIT_LOG_PATH = 
        "/ninacatcoin_data/ai_module/audit.log";

    // Quarantine status file
    static constexpr const char* QUARANTINE_FLAG_FILE = 
        "/ninacatcoin_data/.quarantined";

    // Canonical hash cache file
    static constexpr const char* CANONICAL_HASH_CACHE = 
        "/ninacatcoin_data/ai_code_certificate.json";

    // Remediation logfile
    static constexpr const char* REMEDIATION_LOG = 
        "/ninacatcoin_data/ai_module/remediation.log";

    // Compilation must be reproducible/deterministic
    static constexpr bool REQUIRE_DETERMINISTIC_BUILD = true;

    // Version of AI module
    static constexpr const char* AI_MODULE_VERSION = "1.0.0";

    // Git commit hash that this version was compiled from
    static constexpr const char* EXPECTED_GIT_COMMIT = 
        "__NINACATCOIN_GIT_COMMIT__"; // Set at compile time
};

} // namespace ninacatcoin_ai
