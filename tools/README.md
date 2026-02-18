# Tools - Herramientas de Consenso y Reputación

## Descripción General

Este directorio contiene las herramientas necesarias para implementar el sistema de **Consenso P2P** y **Reputación de Nodos** en ninacatcoin.

---

## 1. security_query_tool.hpp

### Propósito
Gestionar la comunicación P2P para queries de seguridad entre nodos.

### Componentes Principales

#### SecurityQuery
```cpp
struct SecurityQuery {
  std::string query_id;      // ID único (UUID)
  uint64_t height;           // Altura del checkpoint problemático
  std::string expected_hash; // Hash que esperamos
  std::string reported_hash; // Hash incorrecto recibido
  std::string source;        // De dónde vino el hash falso
  std::string attack_type;   // Tipo: "invalid_format", "replay", "seed_mismatch"
};
```

#### SecurityResponse
```cpp
struct SecurityResponse {
  std::string query_id;      // Responde a cuál query
  std::string responder_node_id;
  bool also_detected;        // ¿Ese nodo también lo ve?
  std::string responder_hash; // Qué hash tiene
};
```

#### ConsensusResult
```cpp
struct ConsensusResult {
  bool is_confirmed;         // ¿Problema confirmado?
  bool is_local;             // ¿Es ataque local?
  uint64_t confirmations;    // Cuántos confirmaron
  uint64_t denials;          // Cuántos negaron
  float consensus_percentage; // % de confirmación
};
```

### Funciones Principales

1. **generate_query_id()** - Crear ID único para query
2. **serialize_query()** / **deserialize_query()** - Convertir JSON
3. **sign_query()** / **verify_query_signature()** - Firmas digitales
4. **calculate_consensus()** - Analizar respuestas
5. **QueryManager** - Gestionar queries en progreso

### Uso Típico

```cpp
// 1. Crear query
SecurityQuery q = {
  .query_id = SecurityQueryTool::generate_query_id(),
  .height = 30,
  .expected_hash = "expected...",
  .reported_hash = "AAAABBBB...GGGG",  // Inválido
  .source = "https://malicious-cdn.com",
  .attack_type = "invalid_format"
};

// 2. Enviar a peers
std::string json = SecurityQueryTool::serialize_query(q);
send_to_peers(json);

// 3. Recibir respuestas
manager.add_response(response);

// 4. Calcular consenso
ConsensusResult result = manager.get_consensus(query_id);
if (result.is_confirmed) {
  // Aplicar protecciones
}
```

---

## 2. reputation_manager.hpp

### Propósito
Mantener y gestionar la reputación de todos los nodos en la red local.

### Componentes Principales

#### NodeReputation
```cpp
struct NodeReputation {
  std::string node_id;       // Identificador del nodo
  float score = 0.5f;        // Score 0.0-1.0
  uint64_t confirmed_reports;  // Reportes confirmados
  uint64_t false_reports;      // Reportes rechazados
  bool is_banned;            // ¿Bannado?
};
```

Score se calcula: `(confirmed / total) × 0.9 + 0.1`

#### ReputationStats
```cpp
struct ReputationStats {
  uint64_t total_nodes;
  uint64_t trusted_nodes;    // Score >= threshold
  float average_score;
  float global_accuracy;     // % reportes confirmados
};
```

### Funciones Principales

1. **load_from_disk()** / **save_to_disk()** - Persistencia
2. **on_report_confirmed(node_id)** - Aumentar reputación
3. **on_report_rejected(node_id)** - Disminuir reputación
4. **is_trusted(node_id)** - ¿Es confiable?
5. **ban_node()** / **unban_node()** - Gestión de bans
6. **apply_temporal_decay()** - Olvido de reportes antiguos
7. **get_statistics()** - Estadísticas globales
8. **get_ranked_nodes()** - Nodos ordenados

### Uso Típico

```cpp
ReputationManager rep("~/.ninacatcoin/testnet");

// Cargar al iniciar
rep.load_from_disk();

// Cuando un reporte se confirma
rep.on_report_confirmed("node_abc123");

// Cuando un reporte es rechazado
rep.on_report_rejected("node_def456");

// Verificar si es confiable
if (rep.is_trusted(peer_id)) {
  accept_alert(alert);
} else {
  ignore_alert(alert);
}

// Banear nodo malicioso
rep.ban_node("node_evil", "Too many false reports");

// Guardar cambios
rep.save_to_disk();

// Obtener estadísticas
ReputationStats stats = rep.get_statistics();
LOG_PRINT_L0("Red health: " << stats.average_score * 100 << "%");
```

---

## 3. Archivos de Configuración

Estos archivos se crean/actualizan automáticamente:

### peer_reputation.json
```json
{
  "nodes": [
    {
      "node_id": "node_abc123",
      "score": 0.95,
      "confirmed_reports": 95,
      "false_reports": 5,
      "is_banned": false
    }
  ],
  "updated_at": 1769371978,
  "version": "1.0"
}
```

### consensus_stats.json
```json
{
  "total_queries": 1234,
  "confirmed_attacks": 12,
  "local_only_attacks": 45,
  "average_consensus_time_ms": 3500
}
```

---

## 4. Integración con checkpoints.cpp

Dentro de `load_checkpoints_from_json()`:

```cpp
// Cuando detectas hash inválido:
if (hash_format_error) {
  // 1. Crear query
  SecurityQuery q = create_query(error_height, error_hash);
  
  // 2. Enviar a peers
  bool consensus_found = query_manager.send_and_wait(q);
  
  // 3. Analizar resultado
  ConsensusResult result = query_manager.get_consensus(q.query_id);
  
  // 4. Actualizar reputación
  for (const auto& node : result.confirming_nodes) {
    reputation_manager.on_report_confirmed(node);
  }
  for (const auto& node : result.denying_nodes) {
    reputation_manager.on_report_rejected(node);
  }
  
  // 5. Decidir qué hacer
  if (result.is_confirmed) {
    BROADCAST_ALERT(q);  // Reportar a red
  } else if (result.is_local) {
    LOG_LOCALLY(q);      // Solo guardar localmente
  }
}
```

---

## 5. Threshold y Configuración

**Valores por defecto:**

```ini
# En ninacatcoin.conf
consensus-peer-count = 3           # Cuántos peers consultar
consensus-threshold = 0.66         # % para confirmar
reputation-threshold = 0.40        # Score mínimo para confiar
reputation-decay-days = 30         # Olvido de reportes
confirmation-reward = 0.05         # Cuánto suma un confirmado
false-penalty = 0.05               # Cuánto resta un falso
```

---

## 6. Seguridad Criptográfica

### Firmas Digitales
```cpp
// Al enviar query
crypto::signature sig = SecurityQueryTool::sign_query(q, my_private_key);

// Al recibir
if (!SecurityQueryTool::verify_query_signature(q, sig, sender_public_key)) {
  REJECT("Invalid signature");
}
```

### Prevención de Replay
```cpp
// Cada query y respuesta tiene:
- query_id (único)
- timestamp
- nonce (aleatorio)
```

---

## 7. Testing

### Unit Tests Necesarios

```cpp
// test_security_query_tool.cpp
TEST(SecurityQueryTool, GenerateUniqueIds)
TEST(SecurityQueryTool, SerializeDeserialize)
TEST(SecurityQueryTool, VerifySignatures)
TEST(SecurityQueryTool, CalculateConsensus_Confirmed)
TEST(SecurityQueryTool, CalculateConsensus_Local)

// test_reputation_manager.cpp
TEST(ReputationManager, LoadSaveFromDisk)
TEST(ReputationManager, UpdateOnConfirmed)
TEST(ReputationManager, UpdateOnRejected)
TEST(ReputationManager, ApplyTemporalDecay)
TEST(ReputationManager, BanNode)
```

---

## 8. Logging y Debugging

### Niveles de LOG

```cpp
// INFO: Queries normales
MINFO("Query sent to " << peer_count << " peers");
MINFO("Consensus reached: " << confirmations << "/" << total);

// WARNING: Anomalías
MWARNING("Query timeout from peer: " << peer_id);
MWARNING("Low reputation node reporting: " << node_id);

// ERROR: Fallos críticos
MERROR("Failed to verify signature");
MERROR("Reputation load failed");

// DEBUG: Información detallada
LOG_ERROR("Query details: " << query.to_string());
LOG_ERROR("Response details: " << response.to_string());
```

---

## 9. Roadmap de Implementación

### Sprint 1: Core Structures
- [ ] Crear security_query_tool.cpp
- [ ] Implementar serialización/deserialización
- [ ] Tests básicos

### Sprint 2: Consensus Logic
- [ ] Implementar calculate_consensus()
- [ ] Integrar QueryManager
- [ ] Tests de consenso

### Sprint 3: Reputation System
- [ ] Crear reputation_manager.cpp
- [ ] Persistencia en disco
- [ ] Decay temporal

### Sprint 4: Integration
- [ ] Integrar en checkpoints.cpp
- [ ] E2E tests
- [ ] Documentación completa

---

## 10. FAQ

**P: ¿Debo compilar estas herramientas por separado?**
R: No, se compilan como parte de libcryptonote_core durante `make`.

**P: ¿Cómo se manejan los timeouts?**
R: Los queries esperan máximo 10 segundos. Después se considera "sin respuesta".

**P: ¿Se persisten las reputaciones?**
R: Sí, se guardan en `peer_reputation.json` automáticamente.

**P: ¿Puedo modificar los umbrales en tiempo de ejecución?**
R: Sí, vía configuración en `ninacatcoin.conf`.

---

**Estado: EN DESARROLLO**
**Última actualización: 25-01-2026**
