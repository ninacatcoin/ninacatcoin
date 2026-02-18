# INSTRUCCIONES PARA SPRINT 1

## Objetivo
Implementar **security_query_tool.cpp** - la herramienta base de consultas P2P

---

## Archivo a Crear

### Ubicación
```
i:\ninacatcoin\tools\security_query_tool.cpp
```

### Tamaño Estimado
- ~600-800 líneas de código
- Compilación: 10-15 segundos

---

## Funciones a Implementar

### 1. SecurityQueryTool::generate_query_id()
```cpp
std::string SecurityQueryTool::generate_query_id()
{
  // Generar UUID v4
  // Retornar como string: "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx"
}
```

### 2. SecurityQueryTool::serialize_query()
```cpp
std::string SecurityQueryTool::serialize_query(const SecurityQuery& query)
{
  // Convertir SecurityQuery a JSON
  // Usar epee::serialization::store_t_to_json
  // Retornar JSON string
}
```

### 3. SecurityQueryTool::deserialize_query()
```cpp
bool SecurityQueryTool::deserialize_query(const std::string& json, SecurityQuery& out_query)
{
  // Parsear JSON
  // Llenar out_query
  // Retornar true/false según éxito
}
```

### 4. SecurityQueryTool::sign_query()
```cpp
crypto::signature SecurityQueryTool::sign_query(
  const SecurityQuery& query, 
  const crypto::secret_key& private_key)
{
  // Serializar query a JSON
  // Firmar JSON usando secret_key
  // Retornar crypto::signature
}
```

### 5. SecurityQueryTool::verify_query_signature()
```cpp
bool SecurityQueryTool::verify_query_signature(
  const SecurityQuery& query,
  const crypto::signature& sig,
  const crypto::public_key& public_key)
{
  // Serializar query a JSON
  // Verificar firma usando public_key
  // Retornar true si es válida
}
```

### 6. SecurityQueryTool::calculate_consensus()
```cpp
ConsensusResult SecurityQueryTool::calculate_consensus(
  const std::vector<SecurityResponse>& responses,
  float threshold_percentage = 0.66f,
  uint64_t min_confirmations = 2)
{
  ConsensusResult result;
  
  // Contar confirmaciones vs negaciones
  for (const auto& r : responses) {
    if (r.also_detected) {
      result.confirmations++;
      result.confirming_nodes.push_back(r.responder_node_id);
    } else {
      result.denials++;
      result.denying_nodes.push_back(r.responder_node_id);
    }
  }
  
  // Calcular porcentaje
  uint64_t total = result.confirmations + result.denials;
  if (total > 0) {
    result.consensus_percentage = (float)result.confirmations / total;
  }
  
  // Determinar si confirmado
  if (result.confirmations >= min_confirmations && 
      result.consensus_percentage >= threshold_percentage) {
    result.is_confirmed = true;
  } else if (result.confirmations == 0) {
    result.is_local = true;
  }
  
  return result;
}
```

### 7. SecurityQueryTool::is_query_expired()
```cpp
bool SecurityQueryTool::is_query_expired(const SecurityQuery& query)
{
  // Obtener tiempo actual
  std::time_t now = std::time(nullptr);
  
  // Calcular diferencia
  uint64_t elapsed = now - query.timestamp;
  
  // Si pasó más de timeout_seconds, está expirado
  return elapsed > query.timeout_seconds;
}
```

### 8. QueryManager::add_query()
```cpp
bool QueryManager::add_query(const SecurityQuery& query)
{
  // Verificar que no existe ya
  if (m_pending_queries.count(query.query_id) > 0) {
    return false;  // Ya existe
  }
  
  // Agregar a mapa
  m_pending_queries[query.query_id] = query;
  m_query_start_times[query.query_id] = std::time(nullptr);
  m_responses[query.query_id] = {};  // Vector vacío
  
  return true;
}
```

### 9. QueryManager::add_response()
```cpp
bool QueryManager::add_response(const SecurityResponse& response)
{
  // Verificar que existe el query
  if (m_pending_queries.count(response.query_id) == 0) {
    return false;  // No existe ese query
  }
  
  // Verificar que no existe la respuesta de este nodo
  auto& responses = m_responses[response.query_id];
  for (const auto& r : responses) {
    if (r.responder_node_id == response.responder_node_id) {
      return false;  // Ya tiene respuesta de este nodo
    }
  }
  
  // Agregar respuesta
  responses.push_back(response);
  return true;
}
```

### 10. QueryManager::cleanup_expired_queries()
```cpp
void QueryManager::cleanup_expired_queries()
{
  // Iterar sobre queries pendientes
  for (auto it = m_pending_queries.begin(); it != m_pending_queries.end(); ) {
    if (SecurityQueryTool::is_query_expired(it->second)) {
      // Eliminar del mapa
      m_query_start_times.erase(it->first);
      m_responses.erase(it->first);
      it = m_pending_queries.erase(it);
    } else {
      ++it;
    }
  }
}
```

---

## Includes Necesarios

```cpp
#include "tools/security_query_tool.hpp"
#include "crypto/crypto.h"
#include "storages/portable_storage_template_helper.h"
#include "string_tools.h"
#include "misc_log_ex.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/lexical_cast.hpp>
#include <ctime>
#include <algorithm>
```

---

## Namespace

```cpp
namespace cryptonote {
namespace security {

// TODO: Implementar funciones aquí

} // namespace security
} // namespace cryptonote
```

---

## Testing Necesario

### Unit Tests a Crear

```cpp
// test_security_query_tool.cpp

TEST(SecurityQueryTool, GenerateUniqueIds) {
  auto id1 = SecurityQueryTool::generate_query_id();
  auto id2 = SecurityQueryTool::generate_query_id();
  ASSERT_NE(id1, id2);  // IDs deben ser únicos
}

TEST(SecurityQueryTool, SerializeDeserialize) {
  SecurityQuery original = {...};
  std::string json = SecurityQueryTool::serialize_query(original);
  
  SecurityQuery deserialized;
  bool success = SecurityQueryTool::deserialize_query(json, deserialized);
  
  ASSERT_TRUE(success);
  ASSERT_EQ(original.query_id, deserialized.query_id);
}

TEST(SecurityQueryTool, ConsensusConfirmed) {
  std::vector<SecurityResponse> responses = {
    {.also_detected = true},
    {.also_detected = true},
    {.also_detected = false}
  };
  
  ConsensusResult result = SecurityQueryTool::calculate_consensus(responses);
  
  ASSERT_TRUE(result.is_confirmed);
  ASSERT_EQ(result.confirmations, 2);
}

TEST(SecurityQueryTool, ConsensusLocal) {
  std::vector<SecurityResponse> responses = {
    {.also_detected = false},
    {.also_detected = false}
  };
  
  ConsensusResult result = SecurityQueryTool::calculate_consensus(responses);
  
  ASSERT_TRUE(result.is_local);
  ASSERT_EQ(result.confirmations, 0);
}

TEST(QueryManager, AddAndRetrieve) {
  QueryManager mgr;
  
  SecurityQuery q = {...};
  bool added = mgr.add_query(q);
  
  ASSERT_TRUE(added);
  auto stored = mgr.get_responses(q.query_id);
  ASSERT_TRUE(stored.empty());
}
```

---

## Validación

Antes de completar Sprint 1:

```
[ ] Compilación sin errores
[ ] Compilación sin warnings
[ ] Todos los tests pasan
[ ] Sin memory leaks (valgrind)
[ ] Documentación de funciones completa
[ ] Ejemplos de uso en comentarios
[ ] Performance: <1ms por operación
[ ] Thread-safe en todo (mutex si es necesario)
```

---

## Próximas Fases

**Después de completar security_query_tool.cpp:**

1. **Crear reputation_manager.cpp** (Sprint 2)
2. **Integrar en checkpoints.cpp** (Sprint 3)
3. **Handlers P2P** (Sprint 4)
4. **Testing E2E** (Sprint 5)

---

## ¿Quieres que implemente security_query_tool.cpp ahora?

Contesta con:
- [ ] **SÍ** - Comienza implementación ahora
- [ ] **NO** - Reviso/ajusto diseño primero
- [ ] **CAMBIOS** - Especifica qué cambiar

---

**ESTADO:** Esperando instrucciones para comenzar Sprint 1 ✅
