# NINA Checkpoint Validator (Advanced Hash-Level Validation)

## Visión General

El **Checkpoint Validator** es un sistema inteligente que detecta ataques a nivel de hash individual en los checkpoints. Va más allá de simple validación de epoch_id - verifica que los hashes reales en el archivo checkpoints.json existen en la blockchain.

## Descripción del Problema Que Resuelve

### Escenario de Ataque Potencial

```
Archivo checkpoints.json original (3:00 AM):
├─ epoch_id: 1771376400
├─ hashlines:
│  ├─ height: 8970, hash: "abc123..."
│  ├─ height: 8940, hash: "def456..."
│  └─ ... (300 hashes)

Archivo descargado a las 3:10 AM (ATAQUE):
├─ epoch_id: 1771376400  ← Mismo epoch
├─ hashlines:
│  ├─ height: 8970, hash: "xyz789..." ← HASH MODIFICADO (es el ataque)
│  ├─ height: 8940, hash: "def456..."
│  └─ ... 
```

**Sin validación a nivel hash**: El sistema vería mismo epoch_id y lo consideraría válido.

**Con validación de hash**: El sistema detecta que el hash en altura 8970 cambió.

## Cómo Funciona

### 1. DESCARGA IDÉNTICA (Normal Polling)

```
Nodo descarga cada 10 minutos:
- 2:50 AM → Descarga checkpoints.json (epoch 1771376395)
- 3:00 AM → Descarga checkpoints.json (epoch 1771376400) ← Nuevo
- 3:10 AM → Descarga checkpoints.json (epoch 1771376400) ← Idéntico
- 3:20 AM → Descarga checkpoints.json (epoch 1771376400) ← Idéntico
- 3:30 AM → Descarga checkpoints.json (epoch 1771376400) ← Idéntico
- 4:00 AM → Descarga checkpoints.json (epoch 1771376404) ← Nuevo

Validación NINA:
✅ 3:10 AM = Idéntico → VÁLIDO (aleatorio verificación)
✅ 3:20 AM = Idéntico → VÁLIDO (aleatorio verificación)
✅ 3:30 AM = Idéntico → VÁLIDO (aleatorio verificación)
```

### 2. NUEVOS HASHES (Nueva Época)

```
Estado anterior (3:00 AM):
- epoch_id: 1771376400
- Último hash en altura 8970

Estado nuevo (4:00 AM):
- epoch_id: 1771376404
- Último hash en altura 9000
- 30 nuevos hashes (8971-9000)

Validación NINA:
1. ✅ Detecta nuevo epoch (1771376400 → 1771376404)
2. ✅ Identifica 30 nuevos hashes
3. ✅ Valida contra data.mdb: Cada hash existe en blockchain
4. ✅ VÁLIDO: Nueva época aceptada
```

### 3. ATAQUE: Hash Modificado

```
Estado anterior (4:00 AM):
- Altura 9000: hash = "abc123def456..."

Archivo atacado (4:10 AM):
- epoch_id: Mismo (1771376404)
- Altura 9000: hash = "xyz789foo999..." ← DIFERENTE

Validación NINA:
1. ✅ Detecta que hash en altura 9000 cambió
2. 🚨 ATTACK DETECTED: Existing hashes were modified
3. 🚨 Quarantine source immediately
```

### 4. ATAQUE: Hash no existe en blockchain

```
Archivo atacado (4:10 AM):
- epoch_id: 1771376408 (nuevo)
- 30 nuevos hashes añadidos
- PERO: Los hashes no existen en data.mdb

Validación NINA:
1. ✅ Detecta nuevo epoch
2. ✅ Identifica 30 nuevos hashes
3. 🚨 Valida contra blockchain: Hash no encontrado en altura X
4. 🚨 ATTACK DETECTED: New hashes not in blockchain
5. 🚨 Quarantine source immediately
```

### 5. ATAQUE: Rollback (Epoch disminuye)

```
Estado anterior:
- epoch_id: 1771376410

Archivo atacado:
- epoch_id: 1771376408 ← Menor (rollback)

Validación NINA:
1. 🚨 ATTACK DETECTED: Epoch ID decreased
2. 🚨 Immediate quarantine
3. 🚨 Fallback to seed nodes
```

## Estados de Validación

### ✅ VÁLIDOS

| Estado | Significado | Acción |
|--------|------------|--------|
| `VALID_IDENTICAL` | Archivo idéntico al anterior | Aceptar, actualizar timestamp |
| `VALID_NEW_EPOCH` | Nueva época con hashes válidos | Aceptar, actualizar estado |
| `VALID_EPOCH_UNCHANGED` | Mismo epoch dentro de timeframe | Aceptar (verificación legítima) |

### 🚨 ATAQUES

| Estado | Indicador de Ataque | Acción |
|--------|-------------------|--------|
| `ATTACK_EPOCH_ROLLBACK` | epoch_id disminuyó | Quarantine inmediato |
| `ATTACK_INVALID_HASHES` | Hashes no en blockchain | Quarantine inmediato |
| `ATTACK_MODIFIED_HASHES` | Hashes existentes cambiaron | Quarantine inmediato |
| `ATTACK_EPOCH_TAMPERING` | Inconsistencia en período/epoch | Quarantine inmediato |

## Ciclo Temporal

```
Intervalo de polling normal (10-30 minutos):
├─ 0-30 min: Mismo epoch → ✅ VÁLIDO
├─ 30-70 min: Mismo epoch → ✅ VÁLIDO + ⚠️  WARN (si sin actualización)
└─ >70 min: Mismo epoch → ✅ VÁLIDO + 🚨 CRÍTICO (seeds posiblemente offline)

Generación de nuevos epochs:
├─ Cada hora (3 AM, 4 AM, 5 AM, etc.)
├─ ~30 nuevos hashes por hora
├─ epoch_id incrementa en ~3600 (1 segundo menos por cada ajuste)
└─ NUNCA disminuye
```

## Integración en Código

### 1. Inicializar Validator

```cpp
#include "src/ai/ai_checkpoint_validator.hpp"

// En main o daemon startup:
auto& validator = CheckpointValidator::getInstance();
validator.initialize();

// Pasar referencia a blockchain database
validator.setBlockchainRef((void*)&blockchain_db);
```

### 2. Validar Descarga de Checkpoints

```cpp
#include <json/json.h>

// Cuando se descarga checkpoints.json:
Json::Value checkpoint_json;
// ... parse JSON from HTTP response ...

CheckpointChanges changes;
auto status = validator.validateCheckpointFile(
    checkpoint_json,
    "https://ninacatcoin.es/checkpoints/checkpoints_mainnet.json",
    changes
);

switch (status) {
    case CheckpointValidationStatus::VALID_IDENTICAL:
    case CheckpointValidationStatus::VALID_NEW_EPOCH:
    case CheckpointValidationStatus::VALID_EPOCH_UNCHANGED:
        // ✅ Aceptar e usar checkpoints
        applyCheckpointsToBlockchain(checkpoint_json);
        break;
        
    case CheckpointValidationStatus::ATTACK_EPOCH_ROLLBACK:
    case CheckpointValidationStatus::ATTACK_INVALID_HASHES:
    case CheckpointValidationStatus::ATTACK_MODIFIED_HASHES:
    case CheckpointValidationStatus::ATTACK_EPOCH_TAMPERING:
        // 🚨 Ataque detectado
        quarantineSource(source_url);
        fallbackToSeedNodes();
        break;
}
```

### 3. Obtener Detalles de Cambios

```cpp
CheckpointChanges changes;
validator.validateCheckpointFile(checkpoint_json, source_url, changes);

std::cout << "Cambios detectados:\n";
std::cout << "  Nuevos hashes: " << changes.new_hashes.size() << "\n";
std::cout << "  Hashes modificados: " << changes.modified_hashes.size() << "\n";
std::cout << "  Hashes removidos: " << changes.removed_hashes.size() << "\n";
std::cout << "  Tiempo desde última época: " << changes.time_since_last_epoch << "s\n";

if (changes.is_new_epoch) {
    std::cout << "  NUEVA ÉPOCA: " << changes.previous_epoch_id 
              << " → " << changes.current_epoch_id << "\n";
}

if (changes.is_identical) {
    std::cout << "  Verificación legítima (archivo idéntico)\n";
}
```

## Validación de Hashes Contra Blockchain

### Implementación (TODO en ai_checkpoint_validator.cpp)

La función `hashExistsInBlockchain()` necesita:

```cpp
bool CheckpointValidator::hashExistsInBlockchain(
    const std::string& hash_hex,
    uint64_t height
) {
    // Implementar búsqueda en BlockchainDB (data.mdb)
    
    // 1. Convertir hash hex a crypto::hash
    crypto::hash block_hash;
    epee::string_tools::hex_to_pod(hash_hex, block_hash);
    
    // 2. Obtener bloque a altura
    block block_data;
    if (!blockchain_db->get_block_at_height(height, block_data)) {
        return false;  // Altura no existe
    }
    
    // 3. Calcular hash del bloque
    crypto::hash calculated_hash = get_block_hash(block_data);
    
    // 4. Comparar con hash esperado
    return calculated_hash == block_hash;
}
```

## Tiempos y Umbrales

```cpp
// En ai_checkpoint_validator.hpp o ai_config.hpp

// Intervalo de polling del nodo
static constexpr int64_t POLLING_INTERVAL_MIN = 600;      // 10 minutos
static constexpr int64_t POLLING_INTERVAL_MAX = 1800;     // 30 minutos

// Interval de generación de nuevos epochs (en seed nodes)
static constexpr int64_t EPOCH_GENERATION_INTERVAL = 3600; // 1 hora

// Tolerancias
static constexpr int64_t TOLERANCE_MS = 120;              // 2 minutos de tolerancia

// Umbrales de tiempo
static constexpr int64_t ACCEPTABLE_TIME_MAX = 4200;      // 70 minutos (warn)
static constexpr int64_t CRITICAL_TIME_MAX = 7200;        // 120 minutos (critical warn)
static constexpr int64_t EMERGENCY_TIME_MAX = 10800;      // 180 minutos (emergency)

// Hashes por época (aproximado)
static constexpr uint64_t EXPECTED_HASHES_PER_HOUR = 30;  // ~30 bloques/hora
```

## Casos de Uso

### Caso 1: Usuario descarga checkpoint a las 3:15 AM
```
Acción:   Descarga checkpoints.json
Época:    1771376400 (generada 3:00 AM)
Resultado: ✅ VÁLIDO (nova época)
Razón:    Nueva época con hashes validados contra blockchain
```

### Caso 2: Usuario descarga mismo checkpoint a las 3:45 AM
```
Acción:   Descarga checkpoints.json nuevamente (verificación)
Época:    1771376400 (MISMA)
Hashes:   Idénticos
Resultado: ✅ VÁLIDO (verificación legítima)
Razón:    Archivo idéntico detectado, es revisión normal
```

### Caso 3: Atacante inyecta hash falso a las 3:50 AM
```
Acción:    Intenta descargar checkpoints.json manipulado
Cambio:    1 hash modificado en altura 8970
Época:     1771376400 (MISMA)
Resultado: 🚨 ATAQUE DETECTADO
Razón:     Hashes existentes fueron modificados
Acción:    Quarantine fuente, fallback a seeds
```

### Caso 4: Attackante inyecta nuevos hashes falsos a las 4:10 AM
```
Acción:    Intenta descargar checkpoints.json con nueva época
Cambio:    30 nuevos hashes (alturas 8971-9000)
Época:     1771376404 (NUEVA)
Hashes:    No existen en blockchain
Resultado: 🚨 ATAQUE DETECTADO
Razón:     Nuevos hashes no encontrados en data.mdb
Acción:    Quarantine fuente, fallback a seeds
```

## Ventajas del Sistema

✅ **Protección granular**: Detecta cambios a nivel individual de hash
✅ **Uso de blockchain como fuente de verdad**: Valida contra data.mdb
✅ **Distinción inteligente**: Permite normal polling, rechaza ataques
✅ **Auditoría completa**: Registra todos cambios detectados
✅ **Respuesta inmediata**: Quarantine instantáneo ante aataques
✅ **Fallback seguro**: Cambio automático a semillas confiables

## Status de Implementación

| Función | Status | Notas |
|---------|--------|-------|
| Comparación de archivos | ✅ Completa | detecta cambios a nivel JSON |
| Detección de new hashes | ✅ Completa | Identifica hashes nuevos y removidos |
| Detección de modified hashes | ✅ Completa | Detecta cambios en hashes existentes |
| Validación epoch_id | ✅ Completa | Rechaza rollbacks |
| Validación timestamp | ✅ Completa | Verifica epoch metadata |
| Validación contra blockchain | ⏳ TODO | Necesita implementación de `hashExistsInBlockchain()` |
| Integración en daemon | ⏳ TODO | Conectar con checkpoint downloader |

## Próximos Pasos

1. **Implementar `hashExistsInBlockchain()`** en ai_checkpoint_validator.cpp
2. **Conectar Validator con checkpoint downloader** del daemon
3. **Integrar con quarantine system** para bloquear fuentes maliciosas
4. **Agregar a CMakeLists.txt** para compilar nuevo módulo
5. **Testing** con múltiples escenarios de ataque
6. **Documentar** en RPC/API para acceso a estado de validación
