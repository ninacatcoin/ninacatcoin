# NINA Checkpoint Validator - Arquitectura Visual

## 🏗️ Arquitectura General del Sistema

```
┌─────────────────────────────────────────────────────────────────────┐
│                        NINA CHECKPOINT VALIDATOR                     │
│                     (Sistema de Donación Avanzada)                   │
└─────────────────────────────────────────────────────────────────────┘
                                    │
                ┌───────────────────┼───────────────────┐
                │                   │                   │
                ▼                   ▼                   ▼
        ┌──────────────┐    ┌──────────────┐   ┌──────────────┐
        │   HTTP       │    │   Seed       │   │   Local      │
        │   Download   │    │   Nodes      │   │   Cache      │
        └──────────────┘    └──────────────┘   └──────────────┘
                            │
                            ▼
                    ┌─────────────────┐
                    │  checkpoints.   │
                    │      json       │
                    └─────────────────┘
                            │
                            ▼
            ╔═══════════════════════════════════╗
            ║  CheckpointValidator::validate    ║
            ║  CheckpointFile()                 ║
            ╚═══════════════════════════════════╝
                            │
                ┌───────────┼───────────┐
                │           │           │
                ▼           ▼           ▼
        ┌──────────┐ ┌──────────┐ ┌──────────┐
        │ Compare  │ │ Validate │ │ Validate │
        │ with     │ │ Epoch ID │ │ Hashes   │
        │ Previous │ │          │ │ vs       │
        │          │ │          │ │ Blockchain
        └──────────┘ └──────────┘ └──────────┘
                            │
                ┌───────────┼───────────┐
                ▼           ▼           ▼
         [3 VALID       4 ATTACKS    ERRORS
          STATES]       DETECTED]     CASES]
````

## 📊 Flujo de Decisión Completo

```
                     ┌─────────────────────┐
                     │ Descarga checkpoints │
                     └──────────┬──────────┘
                                │
                                ▼
                    ┌─────────────────────┐
                    │ ¿Primer descarga?   │
                    └────┬────────────┬───┘
                         │            │
                       NO│            │YES
                         │            └─────────────┐
                         │                          │
                         ▼                          ▼
                ┌─────────────────────┐     ┌──────────────────┐
                │ ¿Archivo idéntico?  │     │ Validar metadata │
                └────┬────────────┬───┘     │ y guardar estado │
                     │            │         └────────┬─────────┘
                   YES│           │NO              │
                     │            │                ▼
                     │            │        ┌──────────────────┐
                     │            │        │ ✅ VALID         │
                     │            │        │    IDENTICAL     │
                     │            │        └──────────────────┘
                     │            │
                     │            ▼
                     │  ┌─────────────────────────┐
                     │  │ ¿epoch_id disminuyó?    │
                     │  └────┬────────────────┬───┘
                     │       │                │
                     │      SI│               │NO
                     │       │                │
                     │       ▼                │
                     │  ┌──────────────────┐  │
                     │  │ 🚨 ATTACK        │  │
                     │  │ EPOCH_ROLLBACK   │  │
                     │  └──────────────────┘  │
                     │                        │
                     │                        ▼
                     │              ┌─────────────────────────┐
                     │              │ ¿epoch_id aumentó?      │
                     │              └────┬────────────────┬───┘
                     │                   │                │
                     │                  SI│               │NO
                     │                   │                │
                     │                   │                ▼
                     │                   │     ┌────────────────────┐
                     │                   │     │ ¿Cambios en hashes?│
                     │                   │     └────┬───────────┬───┘
                     │                   │          │           │
                     │                   │         SI│           │NO
                     │                   │          │           │
                     │                   │          ▼           ▼
                     │                   │      🚨 ATTACK      ✅ VALID
                     │                   │      TAMPERING      UNCHANGED
                     │                   │
                     │                   ▼
                     │  ┌─────────────────────────────┐
                     │  │ ¿Sullo nuevos hashes?       │
                     │  └────┬────────────────────┬───┘
                     │       │                    │
                     │      SI│                   │NO
                     │       │                    │
                     │       ▼                    ▼
       ┌─────────┐   │  ┌─────────────────────┐  ANOMALIA
       │ ✅      ├───┘  │ Validar contra      │
       │ VALID   │      │ blockchain (data.   │
       │ IDENTICAL      │ mdb)                │
       └─────────┘      └────┬────────────┬───┘
                             │            │
                          VALID           │INVALID
                             │            │
                             ▼            ▼
                    ┌──────────────────┐  🚨 ATTACK
                    │ ✅ VALID_NEW_    │  INVALID_
                    │    EPOCH         │  HASHES
                    └──────────────────┘
```

## 🎯 Estado de Validación - Árbol de Decisión

```
CheckpointValidator.validateCheckpointFile()
│
├─ VALID ✅
│  ├─ VALID_IDENTICAL
│  │  └─ Transactional: Archivo completo igual
│  │
│  ├─ VALID_NEW_EPOCH
│  │  ├─ epoch_id incrementó
│  │  ├─ Nuevos hashes encontrados
│  │  └─ Nuevos hashes validados en blockchain
│  │
│  └─ VALID_EPOCH_UNCHANGED
│     ├─ epoch_id igual al anterior
│     ├─ Tiempo dentro límite aceptable (<2 horas)
│     └─ No hay cambios en hashes
│
├─ ATTACK 🚨
│  ├─ ATTACK_EPOCH_ROLLBACK
│  │  └─ epoch_id < previous_epoch_id  [INMEDIATO QUARANTINE]
│  │
│  ├─ ATTACK_INVALID_HASHES
│  │  ├─ Nuevos hashes NO en blockchain
│  │  └─ Detecta: Hashes falsos inyectados [IMMEDIATAMENTE QUARANTINE]
│  │
│  ├─ ATTACK_MODIFIED_HASHES
│  │  ├─ Hash existente cambió de valor
│  │  └─ Detecta: height X: abc123→xyz789 [INMEDIATO QUARANTINE]
│  │
│  └─ ATTACK_EPOCH_TAMPERING
│     ├─ metadata epoch inconsistente
│     ├─ epoch_id ≠ generated_at_ts
│     └─ Datos corrompidos [INMEDIATO QUARANTINE]
│
└─ ERROR ❌
   ├─ ERROR_PARSE_FAILED
   ├─ ERROR_NO_PREVIOUS_STATE
   └─ ERROR_BLOCKCHAIN_ACCESS
```

## 📈 Timeline de Operación Normal

```
Tiempo →
│
3:00 AM ├─ 📥 Descarga: epoch 1771376400 (nuevos 30 hashes)
│       │  ✅ VALID_NEW_EPOCH: Validado contra blockchain
│
3:10 AM ├─ 📥 Descarga: epoch 1771376400 (MISMO archivo)
│       │  ✅ VALID_IDENTICAL: Verificación legítima
│
3:20 AM ├─ 📥 Descarga: epoch 1771376400 (MISMO archivo)
│       │  ✅ VALID_IDENTICAL: Verificación legítima
│
3:30 AM ├─ 📥 Descarga: epoch 1771376400 (MISMO archivo)
│       │  ✅ VALID_IDENTICAL: Verificación legítima
│
4:00 AM ├─ 📥 Descarga: epoch 1771376404 (nuevos 30 hashes)
│       │  ✅ VALID_NEW_EPOCH: Validado contra blockchain
│
4:10 AM ├─ 📥 Descarga: epoch 1771376404 (MISMO archivo)
│       │  ✅ VALID_IDENTICAL: Verificación legítima
```

## 📉 Timeline de Ataque Detectado

```
Tiempo →
│
4:00 AM ├─ 📥 Descarga: epoch 1771376404 (nuevos 30 hashes)
│       │  ✅ VALID_NEW_EPOCH: Validado
│
4:10 AM ├─ 📥 Descarga: https://attacker.com/checkpoints.json
│       │  CONTENIDO: epoch 1771376404
│       │              altura 9000: hash abc→xyz (MODIFICADO)
│       │
│       │  NINA VALIDA:
│       │  1. ¿Idéntico? NO
│       │  2. ¿epoch disminuyó? NO
│       │  3. ¿Nuevas cambios? SÍ
│       │  4. ¿Hashes modificados?
│       │     SÍ → altura 9000: abc123→xyz789
│       │
│       │  🚨 ATTACK_MODIFIED_HASHES DETECTED
│       │  🚨 Quarantine: https://attacker.com
│       │  🚨 Fallback: Seed nodes
```

## 🔄 Integración con Sistemas Existentes

```
                    ┌─────────────────┐
                    │  HTTP Downloader│
                    └────────┬────────┘
                             │
                             ▼
                ┌────────────────────────┐
                │ JSON Parser Library    │
                └────────────┬───────────┘
                             │
                             ▼
        ╔═══════════════════════════════════════╗
        ║    CheckpointValidator:validate()     ║
        ╚═══════════════════════════════════════╝
                             │
               ┌─────────────┼─────────────┐
               │             │             │
               ▼             ▼             ▼
        ┌────────────┐  ┌──────────┐  ┌─────────────┐
        │ Blockchain │  │ Quarantine│  │ Fallback to │
        │   Database │  │   System  │  │ Seed Nodes  │
        │ (data.mdb) │  │           │  │             │
        └────────────┘  └──────────┘  └─────────────┘
```

## 🛡️ Matriz de Detección

```
HOJA DE TRUCOS: ¿Qué detecta?

┌─────────────────────────────────────────────────────────────────┐
│ ESCENARIO                    │ INDICADOR        │ RESULTADO      │
├─────────────────────────────────────────────────────────────────┤
│ Normal polling (10-30 min)   │ Archivo idéntico │ ✅ ACCEPT      │
│ Nueva época (cambio válido)  │ epoch↑ hashes en │ ✅ ACCEPT      │
│                              │ blockchain       │                │
│ Hash modificado              │ height X cambio  │ 🚨 BLOCK       │
│ Hash falso inyectado         │ Hash no en BC    │ 🚨 BLOCK       │
│ Rollback (epoch↓)            │ epoch_id < prev  │ 🚨 BLOCK       │
│ Metadata corrupta            │ Inconsistencia   │ 🚨 BLOCK       │
└─────────────────────────────────────────────────────────────────┘
```

## 📊 Estructura de Clases

```
┌──────────────────────────────────────┐
│   CheckpointValidator                │
├──────────────────────────────────────┤
│ Properties:                          │
│  - last_valid_checkpoint: Json::Value
│  - last_valid_epoch_id: uint64_t     │
│  - blockchain_db: void*              │
│  - last_error: string                │
│                                      │
│ Public Methods:                      │
│  + getInstance()                     │
│  + initialize()                      │
│  + validateCheckpointFile()      [+]
│  + getValidationReport()             │
│  + getLastError()                    │
│  + setBlockchainRef()                │
│  + updateKnownGoodCheckpoint()       │
└──────────────────────────────────────┘
        │
        └─── CheckpointChanges
             ├─ previous_epoch_id
             ├─ current_epoch_id
             ├─ new_hashes[]
             ├─ modified_hashes[]
             ├─ removed_hashes[]
             ├─ is_identical
             ├─ is_new_epoch
             └─ time_since_last_epoch
```

## 🎯 Casos de Uso Visuales

### Caso 1: Polling Normal
```
          NODO A              NODO B              HOSTING
                              │                      │
                              └──────────────────────┘
                                  GET checkpoints.json
                              ←────────────────────────
                        [epoch 1771376400, 300 hashes]
                              │
                    ┌─────────┴─────────┐
                    │                   │
                    ▼                   ▼
              Cache local        NINA Validator
              check ✅            validates ✅
              IDENTICAL           IDENTICAL
                    │                   │
                    └─────────┬─────────┘
                              │
                        Accept & apply
```

### Caso 2: Ataque Detectado
```
          NODO                ATTACKER
          │                      │
          └──────────────────────┘
             GET checkpoints.json
          ←────────────────────────
    [epoch 1771376400, 299 hashes]
    [height 9000: hash MODIFIED]
          │
          ▼
    NINA Validator
    ├─ Comparar: ❌ NOT IDENTICAL
    ├─ Validar epoch: ✅ OK (no rollback)
    ├─ Detectar cambios: ✅ FOUND
    ├─ Hash modificado? ✅ YES
    │  └─ height 9000: abc→xyz
    │
    🚨 ATTACK DETECTED
         │
         ├─ Quarantine attacker.com
         ├─ Switch to seed nodes
         └─ Log incident
```

## ⏱️ Diagrama de Timeouts

```
Tiempo desde última época
│
120 min ├──────────────────────────────────────
        │ 🚨 EMERGENCIA
        │ (Más de 2 horas, algo muy mal)
        │
70 min  ├──────────────────────────────────────
        │ 🚨 CRÍTICO
        │ (Posible que las semillas estén offline)
        │
30 min  ├──────────────────────────────────────
        │ ⚠️ ADVERTENCIA
        │ (Checkpoint es viejo, acercándose a límite)
        │
0 min   ├──────────────────────────────────────
        │ ✅ NORMAL
        │ (Reciente)
        │
        └─────────────────────────────────────→
              Descarga normal cada 10-30 min
```

## 🔗 Integración de Componentes

```
                    DAEMON CORE
                    │
        ┌───────────┼───────────┐
        │           │           │
        ▼           ▼           ▼
    Blockchain  Checkpoint  Checkpoint
    Storage     Monitor     Validator ← NINA
    (data.mdb)  (exists)    (NUEVO)
        │           │           │
        └───────────┼───────────┘
                    │
              ┌─────┴─────┐
              │           │
              ▼           ▼
        Quarantine    Seed
        System        Nodes
```

---

Este diagrama visual muestra cómo el **CheckpointValidator** se integra como centro de control de integridad, validando cada descarga contra la blockchain y reaccionando automáticamente ante ataques.
