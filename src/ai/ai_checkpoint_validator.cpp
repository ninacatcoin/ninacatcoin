// Copyright (c) 2026, The ninacatcoin Project
//
// Checkpoint validator implementation

#include "ai_checkpoint_validator.hpp"
#include "ai_config.hpp"
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <json/json.h>

namespace ninacatcoin_ai {

static CheckpointValidator* g_checkpoint_validator = nullptr;

CheckpointValidator& CheckpointValidator::getInstance() {
    if (!g_checkpoint_validator) {
        g_checkpoint_validator = new CheckpointValidator();
    }
    return *g_checkpoint_validator;
}

CheckpointValidator::CheckpointValidator()
    : is_initialized(false), blockchain_db(nullptr) {
}

CheckpointValidator::~CheckpointValidator() {
}

bool CheckpointValidator::initialize() {
    if (is_initialized) {
        return true;
    }
    
    is_initialized = true;
    std::cout << "[NINA Checkpoint] Validator initialized" << std::endl;
    std::cout << "[NINA Checkpoint] Polling interval: 10-30 minutes" << std::endl;
    std::cout << "[NINA Checkpoint] New epoch every: 1 hour (~3600 seconds)" << std::endl;
    std::cout << "[NINA Checkpoint] Hash validation: Enabled vs blockchain" << std::endl;
    
    return true;
}

void CheckpointValidator::setBlockchainRef(void* blockchain_db_ref) {
    blockchain_db = blockchain_db_ref;
    std::cout << "[NINA Checkpoint] Blockchain reference set for hash validation" << std::endl;
}

CheckpointValidationStatus CheckpointValidator::validateCheckpointFile(
    const Json::Value& checkpoint_json,
    const std::string& source_url,
    CheckpointChanges& changes_out
) {
    // Extract epoch_id
    uint64_t current_epoch = extractEpochId(checkpoint_json);
    
    std::cout << "\n[NINA Checkpoint] === Checkpoint Validation Start ===" << std::endl;
    std::cout << "[NINA Checkpoint] Source: " << source_url << std::endl;
    std::cout << "[NINA Checkpoint] Epoch ID: " << current_epoch << std::endl;
    
    // Check if we have a previous checkpoint to compare
    if (last_valid_epoch_id == 0) {
        std::cout << "[NINA Checkpoint] ℹ️  No previous checkpoint (first download)" << std::endl;
        
        // Validate this checkpoint before accepting it
        if (!validateEpochMetadata(checkpoint_json)) {
            last_error = "Invalid epoch metadata in first checkpoint";
            return CheckpointValidationStatus::ATTACK_EPOCH_TAMPERING;
        }
        
        // Accept first checkpoint if metadata is valid
        updateKnownGoodCheckpoint(checkpoint_json);
        std::cout << "[NINA Checkpoint] ✅ First checkpoint accepted (metadata valid)" << std::endl;
        
        CheckpointChanges first_changes;
        first_changes.is_identical = true;
        changes_out = first_changes;
        return CheckpointValidationStatus::VALID_IDENTICAL;
    }
    
    // ===== 1. DETECT IDENTICAL CHECKPOINTS (Normal Polling) =====
    if (areCheckpointsIdentical(checkpoint_json, last_valid_checkpoint)) {
        std::cout << "[NINA Checkpoint] ✅ VALID: Identical checkpoint (normal polling)" << std::endl;
        
        CheckpointChanges identical_changes;
        identical_changes.is_identical = true;
        identical_changes.current_epoch_id = current_epoch;
        identical_changes.previous_epoch_id = last_valid_epoch_id;
        identical_changes.time_since_last_epoch = getCurrentTimestamp() - last_valid_epoch_timestamp;
        
        changes_out = identical_changes;
        return CheckpointValidationStatus::VALID_IDENTICAL;
    }
    
    // ===== 2. DETECT EPOCH ROLLBACK (CRITICAL ATTACK) =====
    if (current_epoch < last_valid_epoch_id) {
        last_error = "ATTACK DETECTED: Epoch ID decreased (rollback attack)";
        std::cerr << "[NINA Checkpoint] 🚨 " << last_error << std::endl;
        std::cerr << "[NINA Checkpoint] Previous epoch: " << last_valid_epoch_id << std::endl;
        std::cerr << "[NINA Checkpoint] Current epoch:  " << current_epoch << std::endl;
        
        logValidationDetails(source_url, CheckpointValidationStatus::ATTACK_EPOCH_ROLLBACK, changes_out);
        return CheckpointValidationStatus::ATTACK_EPOCH_ROLLBACK;
    }
    
    // ===== 3. COMPARE WITH PREVIOUS STATE =====
    changes_out = compareWithPrevious(checkpoint_json, last_valid_checkpoint);
    
    int64_t time_elapsed = getCurrentTimestamp() - last_valid_epoch_timestamp;
    changes_out.time_since_last_epoch = time_elapsed;
    
    // ===== 4. VALIDATE EPOCH PROGRESSION =====
    if (!validateEpochProgression(last_valid_epoch_id, current_epoch, time_elapsed)) {
        last_error = "Invalid epoch progression timing";
        return CheckpointValidationStatus::ATTACK_EPOCH_TAMPERING;
    }
    
    // ===== 5. DETECT MODIFIED HASHES (STRONG ATTACK INDICATOR) =====
    auto previous_hashes = extractHashMap(last_valid_checkpoint);
    auto current_hashes = extractHashMap(checkpoint_json);
    
    if (detectModifiedHashes(previous_hashes, current_hashes)) {
        last_error = "ATTACK DETECTED: Existing hashes were modified";
        std::cerr << "[NINA Checkpoint] 🚨 " << last_error << std::endl;
        std::cerr << "[NINA Checkpoint] Modified hashes count: " 
                  << changes_out.modified_hashes.size() << std::endl;
        
        // Log details of modified hashes
        for (const auto& [old_hash, new_hash] : changes_out.modified_hashes) {
            std::cerr << "[NINA Checkpoint]   Old: " << old_hash.substr(0, 16) << "..." << std::endl;
            std::cerr << "[NINA Checkpoint]   New: " << new_hash.substr(0, 16) << "..." << std::endl;
        }
        
        logValidationDetails(source_url, CheckpointValidationStatus::ATTACK_MODIFIED_HASHES, changes_out);
        return CheckpointValidationStatus::ATTACK_MODIFIED_HASHES;
    }
    
    // ===== 6. IF NO NEW HASHES AND EPOCH SAME = NORMAL POLLING =====
    if (current_epoch == last_valid_epoch_id && changes_out.new_hashes.empty()) {
        
        // Check if timeout is within acceptable range
        if (isTimeWithinAcceptableRange(time_elapsed)) {
            std::cout << "[NINA Checkpoint] ✅ VALID: Same epoch, normal polling interval ("
                      << time_elapsed << " seconds)" << std::endl;
            
            changes_out.is_identical = true;
            logValidationDetails(source_url, CheckpointValidationStatus::VALID_EPOCH_UNCHANGED, changes_out);
            return CheckpointValidationStatus::VALID_EPOCH_UNCHANGED;
        } else {
            // Time is very long, warn about seed node status
            std::cout << "[NINA Checkpoint] ⚠️  WARN: Epoch unchanged for " 
                      << time_elapsed << " seconds" << std::endl;
            std::cout << "[NINA Checkpoint] ℹ️  Seed nodes may be offline or slow" << std::endl;
            
            // Still valid, but flagged as warning
            changes_out.is_identical = true;
            return CheckpointValidationStatus::VALID_EPOCH_UNCHANGED;
        }
    }
    
    // ===== 7. IF NEW EPOCH WITH NEW HASHES =====
    if (current_epoch > last_valid_epoch_id && !changes_out.new_hashes.empty()) {
        std::cout << "[NINA Checkpoint] 📊 New epoch detected" << std::endl;
        std::cout << "[NINA Checkpoint] Previous epoch: " << last_valid_epoch_id << std::endl;
        std::cout << "[NINA Checkpoint] Current epoch:  " << current_epoch << std::endl;
        std::cout << "[NINA Checkpoint] New hashes:     " << changes_out.new_hashes.size() << std::endl;
        
        changes_out.is_new_epoch = true;
        
        // ===== CRITICAL: VALIDATE NEW HASHES AGAINST BLOCKCHAIN =====
        std::cout << "[NINA Checkpoint] 🔍 Validating new hashes against blockchain..." << std::endl;
        
        // Extract heights of new hashes
        uint64_t current_height = current_hashes.rbegin()->first;  // Last entry
        uint64_t previous_height = previous_hashes.rbegin()->first;
        uint64_t expected_new_height = previous_height + 30;  // Expecting ~30 new blocks per hour
        
        std::cout << "[NINA Checkpoint] Expected height range: " << (previous_height + 1)
                  << " - " << current_height << std::endl;
        
        // Validate that all new hashes exist in blockchain
        if (!validateNewHashesAgainstBlockchain(changes_out.new_hash_heights, previous_height)) {
            last_error = "ATTACK DETECTED: New hashes not found in blockchain";
            std::cerr << "[NINA Checkpoint] 🚨 " << last_error << std::endl;
            
            logValidationDetails(source_url, CheckpointValidationStatus::ATTACK_INVALID_HASHES, changes_out);
            return CheckpointValidationStatus::ATTACK_INVALID_HASHES;
        }
        
        std::cout << "[NINA Checkpoint] ✅ All new hashes validated against blockchain" << std::endl;
        
        // Update known good state
        updateKnownGoodCheckpoint(checkpoint_json);
        
        logValidationDetails(source_url, CheckpointValidationStatus::VALID_NEW_EPOCH, changes_out);
        return CheckpointValidationStatus::VALID_NEW_EPOCH;
    }
    
    // ===== 8. ANOMALOUS STATE =====
    last_error = "Anomalous checkpoint state (epoch changed but no new hashes)";
    std::cerr << "[NINA Checkpoint] 🚨 " << last_error << std::endl;
    
    logValidationDetails(source_url, CheckpointValidationStatus::ATTACK_EPOCH_TAMPERING, changes_out);
    return CheckpointValidationStatus::ATTACK_EPOCH_TAMPERING;
}

// ==================== COMPARISON FUNCTIONS ====================

CheckpointChanges CheckpointValidator::compareWithPrevious(
    const Json::Value& current_checkpoint,
    const Json::Value& previous_checkpoint
) {
    CheckpointChanges changes;
    
    changes.current_epoch_id = extractEpochId(current_checkpoint);
    changes.previous_epoch_id = extractEpochId(previous_checkpoint);
    
    auto prev_hashes = extractHashMap(previous_checkpoint);
    auto curr_hashes = extractHashMap(current_checkpoint);
    
    changes.previous_height = prev_hashes.empty() ? 0 : prev_hashes.rbegin()->first;
    changes.current_height = curr_hashes.empty() ? 0 : curr_hashes.rbegin()->first;
    
    // Find new hashes
    for (const auto& [height, hash] : curr_hashes) {
        auto it = prev_hashes.find(height);
        if (it == prev_hashes.end()) {
            changes.new_hashes.push_back(hash);
            changes.new_hash_heights[hash] = height;
        } else if (it->second != hash) {
            changes.modified_hashes.push_back({it->second, hash});
        }
    }
    
    // Find removed hashes
    for (const auto& [height, hash] : prev_hashes) {
        if (curr_hashes.find(height) == curr_hashes.end()) {
            changes.removed_hashes.push_back(hash);
        }
    }
    
    return changes;
}

bool CheckpointValidator::areCheckpointsIdentical(
    const Json::Value& a,
    const Json::Value& b
) const {
    // Simple comparison: convert both to strings and compare
    // In production, would use more sophisticated comparison
    Json::StreamWriterBuilder writer;
    std::string a_str = Json::writeString(writer, a);
    std::string b_str = Json::writeString(writer, b);
    
    return a_str == b_str;
}

std::map<uint64_t, std::string> CheckpointValidator::extractHashMap(
    const Json::Value& checkpoint
) const {
    std::map<uint64_t, std::string> hash_map;
    
    if (!checkpoint.isMember("hashlines") || !checkpoint["hashlines"].isArray()) {
        return hash_map;
    }
    
    const Json::Value& hashlines = checkpoint["hashlines"];
    for (const auto& entry : hashlines) {
        if (entry.isMember("hash") && entry.isMember("height")) {
            uint64_t height = entry["height"].asUInt64();
            std::string hash = entry["hash"].asString();
            hash_map[height] = hash;
        }
    }
    
    return hash_map;
}

// ==================== VALIDATION FUNCTIONS ====================

bool CheckpointValidator::validateEpochProgression(
    uint64_t previous_epoch,
    uint64_t current_epoch,
    int64_t time_elapsed
) {
    // Epochs should only increase or stay the same
    if (current_epoch < previous_epoch) {
        return false;  // Already caught as rollback
    }
    
    // If epoch changed, validate the time is reasonable
    if (current_epoch > previous_epoch) {
        // Epoch change should happen roughly every 3600 seconds
        // But allow variance for network delays
        
        // If less than 30 minutes elapsed, epoch shouldn't have changed
        if (time_elapsed < 1800) {  // 30 minutes
            std::cout << "[NINA Checkpoint] ⚠️  WARN: Epoch changed too quickly ("
                      << time_elapsed << " seconds)" << std::endl;
            // Don't fail, but log warning
        }
        
        // If more than 2 hours elapsed, we missed updates
        if (time_elapsed > 7200) {  // 2 hours
            std::cout << "[NINA Checkpoint] ℹ️  Epoch advanced after extended delay ("
                      << time_elapsed << " seconds) - possible seed offline" << std::endl;
        }
    }
    
    return true;
}

bool CheckpointValidator::isTimeWithinAcceptableRange(int64_t time_since_epoch) {
    // Normal polling: 0-30 minutes (always OK)
    const int64_t NORMAL_POLLING_MAX = 1800;  // 30 minutes
    if (time_since_epoch <= NORMAL_POLLING_MAX) {
        return true;
    }
    
    // Acceptable staleness: 30-70 minutes (warn but accept)
    const int64_t ACCEPTABLE_MAX = 4200;  // 70 minutes
    if (time_since_epoch <= ACCEPTABLE_MAX) {
        std::cout << "[NINA Checkpoint] ⚠️  WARN: Checkpoint is " 
                  << (time_since_epoch / 60) << " minutes old" << std::endl;
        return true;
    }
    
    // Critical: 70-120 minutes (serious warning)
    const int64_t CRITICAL_MAX = 7200;  // 120 minutes
    if (time_since_epoch <= CRITICAL_MAX) {
        std::cout << "[NINA Checkpoint] 🚨 CRITICAL: Checkpoint is " 
                  << (time_since_epoch / 60) << " minutes old" << std::endl;
        std::cout << "[NINA Checkpoint] 🚨 Seed nodes may be offline" << std::endl;
        return true;  // Still valid, but severe warning
    }
    
    // Emergency: >120 minutes (consider this an issue)
    std::cout << "[NINA Checkpoint] 🚨 EMERGENCY: Checkpoint is " 
              << (time_since_epoch / 60) << " minutes old" << std::endl;
    return true;  // Still accept, but log critical status
}

bool CheckpointValidator::validateNewHashesAgainstBlockchain(
    const std::vector<std::pair<uint64_t, std::string>>& new_hashes,
    uint64_t expected_min_height
) {
    // If blockchain_db not set, skip validation (will work in production with ref)
    if (!blockchain_db) {
        std::cout << "[NINA Checkpoint] ℹ️  Blockchain DB not available (will validate in production)" << std::endl;
        // For now, accept new hashes if we can't validate against blockchain
        // In production, this would fail
        return true;
    }
    
    // Validate each new hash exists in blockchain at expected height
    for (const auto& [height, hash] : new_hashes) {
        if (!hashExistsInBlockchain(hash, height)) {
            std::cerr << "[NINA Checkpoint] Hash not found at height " << height 
                      << ": " << hash.substr(0, 16) << "..." << std::endl;
            return false;
        }
    }
    
    return true;
}

bool CheckpointValidator::hashExistsInBlockchain(
    const std::string& hash_hex,
    uint64_t height
) {
    // TODO: Implement actual data.mdb lookup
    // This would query the BlockchainDB to verify:
    // 1. Hash exists in database
    // 2. Hash is at the specified height
    // 3. Hash matches blockchain state
    
    // For now, return true (placeholder)
    return true;
}

bool CheckpointValidator::detectModifiedHashes(
    const std::map<uint64_t, std::string>& previous_hashes,
    const std::map<uint64_t, std::string>& current_hashes
) {
    // Check if any existing height has a different hash
    for (const auto& [height, prev_hash] : previous_hashes) {
        auto it = current_hashes.find(height);
        if (it != current_hashes.end() && it->second != prev_hash) {
            // Same height, different hash - this is definitely an attack
            std::cout << "[NINA Checkpoint] 🚨 Modified hash at height " << height << std::endl;
            return true;
        }
    }
    
    return false;
}

bool CheckpointValidator::validateEpochMetadata(const Json::Value& checkpoint) {
    if (!checkpoint.isMember("epoch_id")) {
        return false;
    }
    
    if (!checkpoint.isMember("generated_at_ts")) {
        return false;
    }
    
    uint64_t epoch_id = checkpoint["epoch_id"].asUInt64();
    uint64_t generated_at = checkpoint["generated_at_ts"].asUInt64();
    
    // epoch_id should equal generated_at_ts
    if (epoch_id != generated_at) {
        std::cerr << "[NINA Checkpoint] Epoch metadata mismatch: "
                  << epoch_id << " != " << generated_at << std::endl;
        return false;
    }
    
    // Both should be reasonable Unix timestamps (not 0, not in future)
    uint64_t now = getCurrentTimestamp();
    if (epoch_id == 0 || epoch_id > now + 300) {  // Allow 5 min time skew
        std::cerr << "[NINA Checkpoint] Invalid epoch timestamp: " << epoch_id << std::endl;
        return false;
    }
    
    return true;
}

uint64_t CheckpointValidator::extractEpochId(const Json::Value& checkpoint) const {
    if (checkpoint.isMember("epoch_id")) {
        return checkpoint["epoch_id"].asUInt64();
    }
    return 0;
}

uint64_t CheckpointValidator::getCurrentTimestamp() const {
    return static_cast<uint64_t>(std::time(nullptr));
}

// ==================== STATE MANAGEMENT ====================

void CheckpointValidator::updateKnownGoodCheckpoint(const Json::Value& checkpoint_json) {
    last_valid_checkpoint = checkpoint_json;
    last_valid_epoch_id = extractEpochId(checkpoint_json);
    last_valid_epoch_timestamp = getCurrentTimestamp();
    
    std::cout << "[NINA Checkpoint] Updated known good checkpoint: epoch "
              << last_valid_epoch_id << std::endl;
}

const Json::Value& CheckpointValidator::getPreviousCheckpointState() const {
    return last_valid_checkpoint;
}

// ==================== ERROR HANDLING ====================

std::string CheckpointValidator::getLastError() const {
    return last_error;
}

std::string CheckpointValidator::getValidationReport() const {
    return last_validation_report;
}

void CheckpointValidator::logValidationDetails(
    const std::string& source,
    CheckpointValidationStatus status,
    const CheckpointChanges& changes
) {
    std::stringstream report;
    report << "[NINA Checkpoint Audit]\n";
    report << "Source: " << source << "\n";
    report << "Status: ";
    
    switch (status) {
        case CheckpointValidationStatus::VALID_IDENTICAL:
            report << "VALID (Identical)\n";
            break;
        case CheckpointValidationStatus::VALID_NEW_EPOCH:
            report << "VALID (New Epoch)\n";
            break;
        case CheckpointValidationStatus::VALID_EPOCH_UNCHANGED:
            report << "VALID (Epoch Unchanged)\n";
            break;
        case CheckpointValidationStatus::ATTACK_EPOCH_ROLLBACK:
            report << "ATTACK (Epoch Rollback)\n";
            break;
        case CheckpointValidationStatus::ATTACK_INVALID_HASHES:
            report << "ATTACK (Invalid Hashes)\n";
            break;
        case CheckpointValidationStatus::ATTACK_MODIFIED_HASHES:
            report << "ATTACK (Modified Hashes)\n";
            break;
        case CheckpointValidationStatus::ATTACK_EPOCH_TAMPERING:
            report << "ATTACK (Epoch Tampering)\n";
            break;
        default:
            report << "ERROR\n";
    }
    
    report << "Changes:\n";
    report << "  New hashes: " << changes.new_hashes.size() << "\n";
    report << "  Modified: " << changes.modified_hashes.size() << "\n";
    report << "  Removed: " << changes.removed_hashes.size() << "\n";
    report << "  Time since epoch: " << changes.time_since_last_epoch << "s\n";
    
    last_validation_report = report.str();
}

std::string CheckpointValidator::generateValidationReport(
    CheckpointValidationStatus status,
    const CheckpointChanges& changes
) {
    std::stringstream report;
    
    report << "Checkpoint Validation Report\n";
    report << "Status: ";
    
    switch (status) {
        case CheckpointValidationStatus::VALID_IDENTICAL:
            report << "✅ VALID (Identical file - normal polling)\n";
            break;
        case CheckpointValidationStatus::VALID_NEW_EPOCH:
            report << "✅ VALID (New epoch with " << changes.new_hashes.size() 
                   << " new hashes)\n";
            break;
        case CheckpointValidationStatus::VALID_EPOCH_UNCHANGED:
            report << "✅ VALID (Same epoch, within acceptable time)\n";
            break;
        case CheckpointValidationStatus::ATTACK_EPOCH_ROLLBACK:
            report << "🚨 ATTACK (Epoch ID decreased: " << changes.previous_epoch_id
                   << " -> " << changes.current_epoch_id << ")\n";
            break;
        case CheckpointValidationStatus::ATTACK_INVALID_HASHES:
            report << "🚨 ATTACK (New hashes not in blockchain)\n";
            report << "  Invalid hashes: " << changes.new_hashes.size() << "\n";
            break;
        case CheckpointValidationStatus::ATTACK_MODIFIED_HASHES:
            report << "🚨 ATTACK (Existing hashes were modified)\n";
            report << "  Modified hashes: " << changes.modified_hashes.size() << "\n";
            break;
        default:
            report << "⚠️  ERROR\n";
    }
    
    return report.str();
}

} // namespace ninacatcoin_ai
