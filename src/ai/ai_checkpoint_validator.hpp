// Copyright (c) 2026, The ninacatcoin Project
//
// Checkpoint validator - Advanced hash-level validation
// Validates checkpoint files against blockchain state

#ifndef NINACATCOIN_AI_CHECKPOINT_VALIDATOR_HPP
#define NINACATCOIN_AI_CHECKPOINT_VALIDATOR_HPP

#include <string>
#include <vector>
#include <map>
#include <jsoncpp/json/json.h>
#include <cstdint>
#include "crypto/hash.h"

namespace ninacatcoin_ai {

/**
 * Checkpoint validation result status codes
 */
enum class CheckpointValidationStatus {
    VALID_IDENTICAL = 0,           // Same as previous (normal polling)
    VALID_NEW_EPOCH = 1,           // New epoch with new hashes (valid)
    VALID_EPOCH_UNCHANGED = 2,     // Same epoch within timeout (normal polling)
    ATTACK_EPOCH_ROLLBACK = 10,    // epoch_id decreased (rollback attack)
    ATTACK_INVALID_HASHES = 11,    // New hashes not in blockchain
    ATTACK_MODIFIED_HASHES = 12,   // Existing hashes were changed
    ATTACK_EPOCH_TAMPERING = 13,   // epoch metadata tampering detected
    ERROR_PARSE_FAILED = 20,       // Failed to parse checkpoint file
    ERROR_NO_PREVIOUS_STATE = 21,  // No previous checkpoint to compare
    ERROR_BLOCKCHAIN_ACCESS = 22   // Cannot access blockchain DB
};

/**
 * Detailed information about changes in checkpoints
 */
struct CheckpointChanges {
    uint64_t previous_epoch_id = 0;
    uint64_t current_epoch_id = 0;
    uint64_t previous_height = 0;
    uint64_t current_height = 0;
    
    std::vector<std::string> new_hashes;        // New hashes added
    std::vector<std::pair<std::string, std::string>> modified_hashes;  // (old_hash, new_hash)
    std::vector<std::string> removed_hashes;    // Hashes that were removed
    
    std::map<std::string, uint64_t> new_hash_heights;  // height -> hash mapping for new ones
    
    bool is_identical = false;
    bool is_new_epoch = false;
    int64_t time_since_last_epoch = 0;
};

/**
 * AI Checkpoint Validator
 * Implements intelligent hash-level validation for checkpoint integrity
 */
class CheckpointValidator {
public:
    static CheckpointValidator& getInstance();
    
    bool initialize();
    
    /**
     * Validate downloaded checkpoint file
     * 
     * @param checkpoint_json Parsed JSON from checkpoints.json
     * @param source_url URL where checkpoint was downloaded from
     * @return Validation status and detailed information
     */
    CheckpointValidationStatus validateCheckpointFile(
        const Json::Value& checkpoint_json,
        const std::string& source_url,
        CheckpointChanges& changes_out
    );
    
    /**
     * Get detailed validation result
     */
    std::string getValidationReport() const;
    
    /**
     * Get last validation error message
     */
    std::string getLastError() const;
    
    /**
     * Get previous checkpoint state (for comparison)
     */
    const Json::Value& getPreviousCheckpointState() const;
    
    /**
     * Force update of known good checkpoint state (after validation)
     */
    void updateKnownGoodCheckpoint(const Json::Value& checkpoint_json);
    
    /**
     * Set blockchain database reference for hash validation
     * This allows validation against actual blockchain state
     */
    void setBlockchainRef(void* blockchain_db_ref);
    
private:
    CheckpointValidator();
    ~CheckpointValidator();
    
    bool is_initialized = false;
    void* blockchain_db = nullptr;  // Reference to BlockchainDB
    
    Json::Value last_valid_checkpoint;
    uint64_t last_valid_epoch_id = 0;
    uint64_t last_valid_epoch_timestamp = 0;
    std::string last_error;
    std::string last_validation_report;
    
    // ==================== Validation Functions ====================
    
    /**
     * Compare current with previous checkpoint file
     * @return Changes detected between versions
     */
    CheckpointChanges compareWithPrevious(
        const Json::Value& current_checkpoint,
        const Json::Value& previous_checkpoint
    );
    
    /**
     * Check if two checkpoints are completely identical
     */
    bool areCheckpointsIdentical(
        const Json::Value& a,
        const Json::Value& b
    ) const;
    
    /**
     * Extract hash data from checkpoint JSON
     * @return Map of height -> hash
     */
    std::map<uint64_t, std::string> extractHashMap(
        const Json::Value& checkpoint
    ) const;
    
    /**
     * Validate that epoch_id only increased or stayed same (never decreased)
     */
    bool validateEpochProgression(
        uint64_t previous_epoch,
        uint64_t current_epoch,
        int64_t time_elapsed
    );
    
    /**
     * Check if time since last epoch is within acceptable range
     * 
     * Normal polling: 0-30 minutes (same epoch)
     * Acceptable staleness: 0-70 minutes (log warning at 70min)
     * Critical: >120 minutes (seed nodes may be offline)
     */
    bool isTimeWithinAcceptableRange(int64_t time_since_epoch);
    
    /**
     * Validate that new hashes exist in the blockchain
     * Uses data.mdb (blockchain database) to verify
     */
    bool validateNewHashesAgainstBlockchain(
        const std::vector<std::pair<uint64_t, std::string>>& new_hashes,
        uint64_t expected_min_height
    );
    
    /**
     * Check if a hash exists in the blockchain at given height
     * This queries data.mdb
     */
    bool hashExistsInBlockchain(
        const std::string& hash_hex,
        uint64_t height
    );
    
    /**
     * Detect if hashes were modified (changed, not added)
     * This is the strongest indicator of an attack
     */
    bool detectModifiedHashes(
        const std::map<uint64_t, std::string>& previous_hashes,
        const std::map<uint64_t, std::string>& current_hashes
    );
    
    /**
     * Validate epoch_id field consistency
     * epoch_id should equal generated_at_ts and be a valid timestamp
     */
    bool validateEpochMetadata(const Json::Value& checkpoint);
    
    /**
     * Extract epoch_id from checkpoint JSON
     */
    uint64_t extractEpochId(const Json::Value& checkpoint) const;
    
    /**
     * Get current time in seconds since epoch
     */
    uint64_t getCurrentTimestamp() const;
    
    /**
     * Log validation details for audit trail
     */
    void logValidationDetails(
        const std::string& source,
        CheckpointValidationStatus status,
        const CheckpointChanges& changes
    );
    
    /**
     * Generate human-readable validation report
     */
    std::string generateValidationReport(
        CheckpointValidationStatus status,
        const CheckpointChanges& changes
    );
};

} // namespace ninacatcoin_ai

#endif // NINACATCOIN_AI_CHECKPOINT_VALIDATOR_HPP
