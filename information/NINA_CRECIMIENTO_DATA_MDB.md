# ANÁLISIS: Almacenamiento Persistente de NINA

## NINA tiene su propia base de datos LMDB

NINA almacena su memoria de forma persistente en su propia base de datos LMDB,
**separada** de la blockchain principal. La base de datos se encuentra en:

```
~/.ninacatcoin/nina_state/data.mdb
```

### Implementación

**Archivo fuente**: `src/daemon/nina_persistent_memory.hpp` (665 líneas)
**Clase principal**: `NINAPersistenceManager` (singleton)
**Motor**: LMDB (lmdb.h) — ACID-compliant, crash-safe
**Tamaño máximo mapeado**: 1 GB inicial (auto-crece +1 GB al llenarse)

### 4 tablas (named DBIs) en la base de datos

```
~/.ninacatcoin/nina_state/data.mdb
├── nina_stats   — key "current" → PersistedStatistics serializado
├── nina_meta    — key "current" → "height|timestamp"
├── nina_blocks  — key = height (8 bytes BE) → PersistedBlockRecord serializado
└── nina_audit   — key = timestamp (8 bytes BE) → "height|event|details"
```

### Qué guarda NINA

**PersistedBlockRecord** (por cada bloque procesado):
- height, timestamp, solve_time
- difficulty, previous_difficulty
- lwma_prediction_error
- anomaly_flags

**PersistedStatistics** (estadísticas globales):
- total_blocks_processed
- total_anomalies_detected
- total_attacks_predicted
- session_count
- average_prediction_accuracy
- peer_reputation_average
- network_health_average
- last_persistence_time

**Audit trail**: log de eventos con height, tipo y detalles

### Funciones principales

| Función | Descripción |
|---------|-------------|
| `persist_nina_state()` | Guarda estado completo (transacción ACID) |
| `load_nina_state()` | Recupera estado al iniciar daemon |
| `persist_block_record()` | Guarda un solo bloque |
| `load_recent_blocks(n)` | Carga últimos N bloques (cursor backward) |
| `log_Nina_audit_trail()` | Agrega entrada al audit log |
| `get_db_size_kb()` | Tamaño actual de la DB en KB |

### Funciones globales de conveniencia

| Función | Descripción |
|---------|-------------|
| `nina_load_persistent_state()` | Carga estado desde LMDB al iniciar |
| `nina_save_persistent_state()` | Guarda estado, incrementa session_count |
| `nina_audit_log()` | Escribe al audit trail |
| `persist_memory_system_data()` | Persiste datos del sistema de memoria |
| `persist_learning_module_data()` | Persiste datos del módulo de aprendizaje |
| `persist_constitution_data()` | Persiste evaluaciones constitucionales |

### Comportamiento de persistencia

- **Al iniciar**: `load_nina_state()` restaura toda la memoria previa
- **Cada 30 bloques (~1 hora)**: `nina_save_persistent_state()` guarda estado completo
- **Al apagar daemon**: guardado final en el destructor `~t_daemon()`
- **Al recibir eventos**: `nina_audit_log()` registra en audit trail
- **Entre reinicios**: La memoria **SÍ persiste** — NINA recuerda todo
- **Si hay crash**: Pierde máximo ~1 hora de memoria (30 bloques)

### Impacto en disco

```
Almacenamiento de NINA:
├── Base de datos: ~/.ninacatcoin/nina_state/data.mdb
│   ├── Map size máximo: 64 MB
│   ├── Tamaño real: depende de bloques procesados
│   └── Crecimiento: ~100 bytes por bloque procesado
│
├── Separada de la blockchain:
│   └── data.mdb de la blockchain (LMDB de Monero) → NO modificada por NINA
│
└── Crecimiento estimado:
    ├── Por día (720 bloques): ~70 KB
    ├── Por año (~263,000 bloques): ~25 MB
    ├─ Map size inicial: 1 GB (~40 años)
    └─ Auto-grow: +1 GB cuando se llena (ilimitado)
```

### Diferencia con la blockchain

| Aspecto | Blockchain (data.mdb) | NINA (nina_state/data.mdb) |
|---------|----------------------|---------------------------|
| Ubicación | ~/.ninacatcoin/lmdb/ | ~/.ninacatcoin/nina_state/ |
| Contenido | Bloques, transacciones | Memoria IA, estadísticas |
| Modificado por NINA | NO (read-only) | SÍ (su propia DB) |
| Tamaño típico | Varios GB | < 64 MB |
| Motor | LMDB | LMDB |

### Resumen

| Métrica | Valor |
|---------|-------|
| Motor de almacenamiento | LMDB (data.mdb propio) |
| Ubicación | ~/.ninacatcoin/nina_state/ |
| Persistencia entre reinicios | SÍ — NINA recuerda todo |
| Impacto en blockchain data.mdb | Ninguno (DB separada) |
| Map size inicial | 1 GB (~40 años de memoria) |
| Auto-grow | +1 GB al llenarse (automático) |
| Crecimiento anual estimado | ~25 MB |

---

*Verificado contra `src/daemon/nina_persistent_memory.hpp` (665 líneas)*
*Última actualización: 20 de febrero de 2026*

