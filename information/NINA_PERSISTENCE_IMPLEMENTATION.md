# NINA Persistence Engine - Implementación Real

## Ubicación del Código

Los archivos de persistencia están en `src/daemon/` (NO en `src/ai/`):

| Archivo | Líneas | Descripción |
|---------|--------|-------------|
| `src/daemon/nina_persistence_engine.hpp` | 122 | Interfaz y estructuras de datos |
| `src/daemon/nina_persistence_engine.cpp` | 634 | Implementación del motor |
| `src/daemon/nina_persistence_api.hpp` | 261 | API wrapper (header-only, inline) |

**Total: 3 archivos, ~1,017 líneas**

---

## Motor de Almacenamiento

**NINA usa `std::fstream` (archivos planos en disco). NO usa LMDB, SQLite, ni ninguna base de datos.**

### Formato de datos
- Texto plano delimitado por pipes (`|`), un registro por línea
- Los vectores internos se serializan delimitados por comas (`,`)
- Los archivos tienen extensión `.json` pero **NO contienen JSON real** — es formato pipe-delimited

### Archivos que genera (relativos al `db_path` pasado en `initialize()`)
| Archivo | Contenido |
|---------|-----------|
| `{db_path}/decisions.json` | Registros de decisiones de validación |
| `{db_path}/proposals.json` | Propuestas de gobernanza |
| `{db_path}/patterns.json` | Patrones de aprendizaje |
| `{db_path}/escalations.json` | Escalaciones a humanos |

### Patrones de I/O
- **Escritura**: Append-only (`std::ios::app`)
- **Lectura**: Scan lineal completo — carga todos los registros en memoria y filtra
- **Updates**: Appends una copia modificada (genera duplicados en el archivo)
- **Creación de directorio**: `system("mkdir -p {path}")`

---

## Estructuras de Datos

### 4 structs principales (en `nina_persistence_engine.hpp`)

1. **DecisionRecord** — Decisiones de validación
   - block_height, action_taken, confidence_score, evidence, timestamp

2. **GovernanceProposalRecord** — Propuestas de gobernanza
   - votes, status, constitutionality

3. **LearningPatternRecord** — Patrones aprendidos
   - attack patterns, difficulty trends, occurrence_count

4. **EscalationRecord** — Escalaciones a humanos
   - urgency_level, options, resolved status

---

## Clase Principal: NINaPersistenceEngine

**Namespace**: `ninacatcoin_ai`
**Patrón**: Todos los métodos son `static`
**Log category**: `"NINA-PERSISTENCE"`

### 26 métodos públicos organizados por TIER:

**Init/Shutdown:**
- `initialize(path)` — crea directorio con `mkdir -p`
- `shutdown()`

**TIER 4 - Decisions:**
- `save_decision_record`, `get_decision_record`
- `get_all_decisions`, `get_decisions_by_height`

**TIER 3 - Governance:**
- `save_governance_proposal`, `get_governance_proposal`
- `get_all_proposals`, `update_proposal_status`

**TIER 5 - Learning:**
- `save_learning_pattern`, `get_learning_pattern`
- `get_all_patterns`, `get_patterns_by_type`

**TIER 6 - Escalations:**
- `save_escalation`, `get_escalation`
- `get_all_escalations`, `get_pending_escalations`, `resolve_escalation`

**Analytics:**
- `get_decision_statistics`, `get_learning_progress`, `get_system_audit_trail`

**Maintenance:**
- `clear_old_records` — **stub vacío, retorna true sin hacer nada**

### 8 métodos privados: serialización/deserialización de cada struct

---

## API Wrapper: NINAPersistenceAPI

**Header-only** en `nina_persistence_api.hpp`, métodos inline `static`.

Genera IDs automáticos con patrón `PREFIX_CONTEXT_timestamp`:
- `"BLOCK_{height}_{timestamp}"` para decisiones de bloque
- `"CHECKPOINT_{height}_{timestamp}"` para validación de checkpoint
- `"TX_{tx_id_16chars}_{timestamp}"` para transacciones
- `"PROP_{type}_{timestamp}"` para propuestas
- `"PATTERN_ATTACK_{timestamp}"` para patrones
- `"ESC_{timestamp}"` para escalaciones

---

## Limitaciones Conocidas

1. **Sin protección de concurrencia** — No hay mutex ni file locking
2. **Updates generan duplicados** — Append de copia modificada sin eliminar la original
3. **Limpieza no implementada** — `clear_old_records()` es stub
4. **No es JSON real** — Extensión `.json` engañosa, formato es pipe-delimited
5. **Scan lineal** — Todas las búsquedas por ID cargan todo el archivo en memoria
6. **NO escribe en data.mdb** — No tiene relación con LMDB de Ninacatcoin

---

*Último update: Verificado contra código fuente real*
