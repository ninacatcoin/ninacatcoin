// NINA Checkpoint Validator Integration Examples
// Cómo integrar el validador de checkpoints en el daemon

#ifndef NINACATCOIN_CHECKPOINT_VALIDATOR_INTEGRATION_HPP
#define NINACATCOIN_CHECKPOINT_VALIDATOR_INTEGRATION_HPP

#include "src/ai/ai_checkpoint_validator.hpp"
#include "src/ai/ai_quarantine_system.hpp"
#include <json/json.h>
#include <string>

namespace ninacatcoin_checkpoint_integration {

// ===================================================================
// EJEMPLO 1: Inicializar el Validator en daemon startup
// ===================================================================

void example_initialize_validator() {
    /*
    // En tu main.cpp o daemon initialization:
    
    #include "src/ai/ai_checkpoint_validator.hpp"
    
    int main() {
        // ... otros inicios ...
        
        // Inicializar NINA Checkpoint Validator
        auto& validator = ninacatcoin_ai::CheckpointValidator::getInstance();
        validator.initialize();
        
        // Pasar referencia a blockchain database
        // (blockchain_db es tu BlockchainDB* del core)
        validator.setBlockchainRef((void*)&blockchain_db);
        
        std::cout << "NINA Checkpoint Validator initialized" << std::endl;
        
        // ... continuar con daemon ...
    }
    */
}

// ===================================================================
// EJEMPLO 2: Validar archivo descargado de HTTP
// ===================================================================

void example_validate_http_download() {
    /*
    // En tu HTTP downloader/checkpoint loader:
    
    #include "src/ai/ai_checkpoint_validator.hpp"
    #include <curl/curl.h>
    
    bool download_and_validate_checkpoints(
        const std::string& source_url,
        BLOCKCHAIN& blockchain
    ) {
        // 1. Descargar archivo JSON desde URL
        std::string json_content = http_get(source_url);
        
        // 2. Parsear JSON
        Json::Value checkpoint_json;
        Json::CharReaderBuilder builder;
        std::string errs;
        
        std::istringstream iss(json_content);
        if (!Json::parseFromStream(builder, iss, &checkpoint_json, &errs)) {
            std::cerr << "Failed to parse checkpoint JSON: " << errs << std::endl;
            return false;
        }
        
        // 3. Validar usando NINA
        auto& validator = ninacatcoin_ai::CheckpointValidator::getInstance();
        ninacatcoin_ai::CheckpointChanges changes;
        
        auto status = validator.validateCheckpointFile(
            checkpoint_json,
            source_url,
            changes
        );
        
        // 4. Procesar resultado
        switch (status) {
            case ninacatcoin_ai::CheckpointValidationStatus::VALID_IDENTICAL:
                std::cout << "[Checkpoint] ✅ Valid (identical file)" << std::endl;
                return true;
                
            case ninacatcoin_ai::CheckpointValidationStatus::VALID_NEW_EPOCH:
                std::cout << "[Checkpoint] ✅ Valid (new epoch with validated hashes)" << std::endl;
                // Aplicar nuevos checkpoints
                apply_checkpoint_data(checkpoint_json, blockchain);
                return true;
                
            case ninacatcoin_ai::CheckpointValidationStatus::VALID_EPOCH_UNCHANGED:
                std::cout << "[Checkpoint] ✅ Valid (same epoch, normal polling)" << std::endl;
                return true;
                
            // Casos de ataque
            case ninacatcoin_ai::CheckpointValidationStatus::ATTACK_EPOCH_ROLLBACK:
                std::cerr << "[Checkpoint] 🚨 ATTACK: Epoch rollback from " 
                          << changes.previous_epoch_id << " to " 
                          << changes.current_epoch_id << std::endl;
                quarantine_source(source_url);
                fallback_to_seed_nodes();
                return false;
                
            case ninacatcoin_ai::CheckpointValidationStatus::ATTACK_INVALID_HASHES:
                std::cerr << "[Checkpoint] 🚨 ATTACK: New hashes not in blockchain" << std::endl;
                std::cerr << "[Checkpoint] Invalid hashes: " << changes.new_hashes.size() << std::endl;
                quarantine_source(source_url);
                fallback_to_seed_nodes();
                return false;
                
            case ninacatcoin_ai::CheckpointValidationStatus::ATTACK_MODIFIED_HASHES:
                std::cerr << "[Checkpoint] 🚨 ATTACK: Existing hashes were modified" << std::endl;
                std::cerr << "[Checkpoint] Modified count: " << changes.modified_hashes.size() << std::endl;
                quarantine_source(source_url);
                fallback_to_seed_nodes();
                return false;
                
            case ninacatcoin_ai::CheckpointValidationStatus::ATTACK_EPOCH_TAMPERING:
                std::cerr << "[Checkpoint] 🚨 ATTACK: Epoch tampering detected" << std::endl;
                quarantine_source(source_url);
                fallback_to_seed_nodes();
                return false;
                
            default:
                std::cerr << "[Checkpoint] ❌ Validation error: " 
                          << validator.getLastError() << std::endl;
                return false;
        }
    }
    */
}

// ===================================================================
// EJEMPLO 3: Log detallado de cambios detectados
// ===================================================================

void example_log_checkpoint_changes() {
    /*
    // Cuando hay cambios detectados:
    
    ninacatcoin_ai::CheckpointChanges changes;
    auto status = validator.validateCheckpointFile(json, source_url, changes);
    
    // Log detallado de cambios
    std::cout << "\n=== CHECKPOINT CHANGES AUDIT ===" << std::endl;
    std::cout << "Previous epoch: " << changes.previous_epoch_id << std::endl;
    std::cout << "Current epoch:  " << changes.current_epoch_id << std::endl;
    std::cout << "Time elapsed:   " << changes.time_since_last_epoch << " seconds" << std::endl;
    std::cout << std::endl;
    
    if (changes.is_identical) {
        std::cout << "Status: Identical file (normal polling)" << std::endl;
    } else if (changes.is_new_epoch) {
        std::cout << "Status: New epoch detected" << std::endl;
        std::cout << "  New hashes:      " << changes.new_hashes.size() << std::endl;
        std::cout << "  Modified hashes: " << changes.modified_hashes.size() << std::endl;
        std::cout << "  Removed hashes:  " << changes.removed_hashes.size() << std::endl;
        std::cout << std::endl;
        
        std::cout << "Height progression: " << changes.previous_height 
                  << " → " << changes.current_height << std::endl;
        
        // Mostrar primeros números de alturas de nuevos hashes
        std::cout << "New block heights: ";
        int count = 0;
        for (const auto& [hash, height] : changes.new_hash_heights) {
            if (count++ < 5) {
                std::cout << height << " ";
            }
        }
        if (changes.new_hash_heights.size() > 5) {
            std::cout << "... +" << (changes.new_hash_heights.size() - 5) << " more";
        }
        std::cout << std::endl;
    }
    
    if (!changes.modified_hashes.empty()) {
        std::cout << "\n⚠️  MODIFIED HASHES DETECTED:" << std::endl;
        for (const auto& [old_hash, new_hash] : changes.modified_hashes) {
            std::cout << "  Old: " << old_hash.substr(0, 16) << "..." << std::endl;
            std::cout << "  New: " << new_hash.substr(0, 16) << "..." << std::endl;
        }
    }
    
    std::cout << "\n" << validator.getValidationReport() << std::endl;
    */
}

// ===================================================================
// EJEMPLO 4: Fallback inteligente a seed nodes
// ===================================================================

void example_seed_node_fallback() {
    /*
    // Cuando checkpoints from HTTP fallan validación:
    
    bool download_from_seeds(BLOCKCHAIN& blockchain) {
        // Lista de semillas confiables
        const std::vector<std::string> SEED_NODES = {
            "http://seed11.ninacatcoin.es:81",
            "http://seed12.ninacatcoin.es:81",
            "http://seed13.ninacatcoin.es:81",
        };
        
        auto& validator = CheckpointValidator::getInstance();
        
        for (const auto& seed_url : SEED_NODES) {
            std::string checkpoint_url = seed_url + "/checkpoints/checkpoints.json";
            
            try {
                std::string json_content = http_get(checkpoint_url);
                Json::Value checkpoint_json;
                // ... parse JSON ...
                
                CheckpointChanges changes;
                auto status = validator.validateCheckpointFile(
                    checkpoint_json,
                    checkpoint_url,
                    changes
                );
                
                if (status == CheckpointValidationStatus::VALID_NEW_EPOCH ||
                    status == CheckpointValidationStatus::VALID_IDENTICAL ||
                    status == CheckpointValidationStatus::VALID_EPOCH_UNCHANGED) {
                    
                    std::cout << "[Seeds] ✅ Checkpoints validated from: " << checkpoint_url << std::endl;
                    apply_checkpoint_data(checkpoint_json, blockchain);
                    return true;
                }
            } catch (const std::exception& e) {
                std::cerr << "[Seeds] ⚠️  Failed to contact: " << seed_url << std::endl;
                continue;
            }
        }
        
        std::cerr << "[Seeds] 🚨 Failed to validate checkpoints from any seed node" << std::endl;
        return false;
    }
    */
}

// ===================================================================
// EJEMPLO 5: Integración con quarantine system
// ===================================================================

void example_quarantine_integration() {
    /*
    // Cuando se detecta un ataque:
    
    #include "src/ai/ai_quarantine_system.hpp"
    
    void handle_checkpoint_attack(
        const std::string& source_url,
        CheckpointValidationStatus attack_type
    ) {
        auto& quarantine = ninacatcoin_ai::QuarantineSystem::getInstance();
        
        std::string reason;
        int severity = ninacatcoin_ai::QuarantineSeverity::CRITICAL;
        
        switch (attack_type) {
            case CheckpointValidationStatus::ATTACK_EPOCH_ROLLBACK:
                reason = "Epoch rollback attack (epoch ID decreased)";
                severity = ninacatcoin_ai::QuarantineSeverity::CRITICAL;
                break;
                
            case CheckpointValidationStatus::ATTACK_INVALID_HASHES:
                reason = "Invalid hashes (not in blockchain)";
                severity = ninacatcoin_ai::QuarantineSeverity::CRITICAL;
                break;
                
            case CheckpointValidationStatus::ATTACK_MODIFIED_HASHES:
                reason = "Modified hashes (existing hashes changed)";
                severity = ninacatcoin_ai::QuarantineSeverity::CRITICAL;
                break;
                
            case CheckpointValidationStatus::ATTACK_EPOCH_TAMPERING:
                reason = "Epoch metadata tampering";
                severity = ninacatcoin_ai::QuarantineSeverity::CRITICAL;
                break;
                
            default:
                return;
        }
        
        // Quarantine the malicious source
        bool quarantined = quarantine.quarantineSource(
            source_url,
            reason,
            severity,
            86400  // 24 hour quarantine
        );
        
        if (quarantined) {
            std::cout << "[Quarantine] 🚨 " << source_url << " quarantined for: " 
                      << reason << std::endl;
        }
    }
    */
}

// ===================================================================
// EJEMPLO 6: Testing - Simular diferentes escenarios
// ===================================================================

void example_unit_tests() {
    /*
    // Tests unitarios para el validador:
    
    void test_identical_checkpoints() {
        Json::Value checkpoint1 = load_json("test_checkpoint_1.json");
        Json::Value checkpoint2 = load_json("test_checkpoint_2.json");
        
        auto& validator = CheckpointValidator::getInstance();
        CheckpointChanges changes;
        
        auto status = validator.validateCheckpointFile(
            checkpoint1, "http://source1.test", changes
        );
        assert(status == CheckpointValidationStatus::VALID_IDENTICAL);
        
        // Second call with identical data
        status = validator.validateCheckpointFile(
            checkpoint2, "http://source2.test", changes
        );
        assert(status == CheckpointValidationStatus::VALID_IDENTICAL);
        assert(changes.is_identical);
    }
    
    void test_new_epoch_with_valid_hashes() {
        Json::Value old_checkpoint = load_json("epoch_1771376400.json");
        Json::Value new_checkpoint = load_json("epoch_1771376404.json");
        
        auto& validator = CheckpointValidator::getInstance();
        CheckpointChanges changes;
        
        // Cargar old state
        auto status = validator.validateCheckpointFile(
            old_checkpoint, "http://seed.test", changes
        );
        
        // Validar new state
        status = validator.validateCheckpointFile(
            new_checkpoint, "http://seed.test", changes
        );
        assert(status == CheckpointValidationStatus::VALID_NEW_EPOCH);
        assert(changes.is_new_epoch);
        assert(changes.new_hashes.size() == 30);  // ~30 new blocks
    }
    
    void test_attack_epoch_rollback() {
        Json::Value new_checkpoint = load_json("epoch_1771376404.json");
        Json::Value rollback_checkpoint = load_json("epoch_1771376400.json");
        
        auto& validator = CheckpointValidator::getInstance();
        CheckpointChanges changes;
        
        // Load new epoch
        validator.validateCheckpointFile(
            new_checkpoint, "http://seed.test", changes
        );
        
        // Try to rollback
        auto status = validator.validateCheckpointFile(
            rollback_checkpoint, "http://attacker.test", changes
        );
        assert(status == CheckpointValidationStatus::ATTACK_EPOCH_ROLLBACK);
    }
    
    void test_attack_modified_hashes() {
        Json::Value original = load_json("original_checkpoints.json");
        Json::Value modified = load_json("original_checkpoints.json");
        
        // Modify one hash
        modified["hashlines"][50]["hash"] = "deadbeef...";
        
        auto& validator = CheckpointValidator::getInstance();
        CheckpointChanges changes;
        
        // Load original
        validator.validateCheckpointFile(
            original, "http://seed.test", changes
        );
        
        // Try to load modified
        auto status = validator.validateCheckpointFile(
            modified, "http://attacker.test", changes
        );
        assert(status == CheckpointValidationStatus::ATTACK_MODIFIED_HASHES);
        assert(changes.modified_hashes.size() == 1);
    }
    */
}

// ===================================================================
// CONFIGURACIÓN RECOMENDADA EN daemon.cpp
// ===================================================================

/*
En tu daemon.cpp o core.cpp, la inicialización debería verse así:

#include "src/ai/ai_checkpoint_validator.hpp"

class daemon {
private:
    CHECK_AND_ASSERT_THROW_MES(
        core.initialize(...),
        "Failed to initialize core");
    
    // Inicializar NINA Checkpoint Validator
    auto& validator = ninacatcoin_ai::CheckpointValidator::getInstance();
    CHECK_AND_ASSERT_THROW_MES(
        validator.initialize(),
        "Failed to initialize checkpoint validator");
    
    // Pasar referencia a blockchain
    validator.setBlockchainRef((void*)&core.get_blockchain_storage().get_db());
    
    MLOG_GREEN(el, green, "NINA Checkpoint Validator ready");
};

// Cuando se descargan checkpoints periódicamente:

void daemon::update_checkpoints() {
    // ... código de descarga HTTP ...
    
    // Validar antes de aplicar
    CheckpointChanges changes;
    auto status = validator.validateCheckpointFile(
        downloaded_json,
        source_url,
        changes
    );
    
    if (is_valid_status(status)) {
        apply_checkpoints(downloaded_json);
    } else {
        handle_checkpoint_attack(source_url, status);
    }
}
*/

} // namespace ninacatcoin_checkpoint_integration

#endif // NINACATCOIN_CHECKPOINT_VALIDATOR_INTEGRATION_HPP
