# INTEGRACIÓN: IA HASHRATE RECOVERY EN EL DAEMON

## 📍 UBICACIÓN DE INTEGRACIÓN

El monitor de hashrate debe integrarse en el código que procesa bloques nuevos.

### Ubicación Principal: `src/cryptonote_core/blockchain.cpp`

En la función que valida/procesa bloques (blockchain::add_new_block() o similar):

---

## 1. INICIALIZACIÓN (Startup)

**Ubicación:** En la función de inicialización del blockchain
```cpp
// En blockchain.cpp, en la función init() o constructor:

#include "ai/ai_hashrate_recovery_monitor.hpp"

void Blockchain::init()
{
  // ... existing code ...
  
  // IA INTEGRATION: Initialize hashrate recovery monitor
  ia_initialize_hashrate_learning();
  
  MGINFO("IA Hashrate Recovery Monitor initialized");
  
  // ... rest of code ...
}
```

---

## 2. POR CADA BLOQUE NUEVO VALIDADO

**Ubicación:** En la función que procesa un bloque después de validación

```cpp
// En blockchain.cpp, después de que validate_block() retorna true
// y el bloque es aceptado en la cadena principal:

bool Blockchain::add_new_block(const block& bl, block_verification_context& bvc)
{
  // ... existing validation code ...
  
  if (bvc.m_added_to_main_chain) {
    // Bloque fue aceptado en la cadena principal
    
    uint64_t new_height = get_current_blockchain_height();
    uint64_t new_difficulty = get_difficulty_for_next_block();
    
    // Obtener tiempo de resolución del bloque
    uint64_t solve_time = bl.timestamp;
    if (solve_time > m_blocks[new_height - 2].timestamp) {
      solve_time = solve_time - m_blocks[new_height - 2].timestamp;
    } else {
      solve_time = 120; // default if timestamp issue
    }
    
    // Clampear solve_time (protección contra timestamp attacks)
    if (solve_time > 720) {
      solve_time = std::min(solve_time, uint64_t(8640)); // max 2 hours
    } else if (solve_time < 1) {
      solve_time = 1;
    }
    
    // Detectar si EDA se activó
    bool eda_triggered = (solve_time > 720);
    
    // IA INTEGRATION: Learn this block
    if (new_difficulty > 0) {
      ia_learns_difficulty_state(new_height, new_difficulty, solve_time, eda_triggered);
    }
  }
  
  return true;
}
```

---

## 3. ANÁLISIS CADA 60 BLOQUES (Períodico)

**Ubicación:** En un procesamiento a intervalo regular, como:
- Función: `Blockchain::update_persistent_chain_db()`
- O un timer: `Blockchain::evaluate_main_chain_checkpoint()`

```cpp
// Cada 60 bloques (o cada minuto de tiempo real)

static uint64_t last_analysis_height = 0;

bool Blockchain::add_new_block(const block& bl, block_verification_context& bvc)
{
  // ... block processing ...
  
  uint64_t current_height = get_current_blockchain_height();
  
  // IA INTEGRATION: Periodic analysis
  if (current_height - last_analysis_height >= 60) {
    last_analysis_height = current_height;
    
    // Analyze LWMA window
    auto lwma_state = ia_analyze_lwma_window(current_height);
    
    if (lwma_state.window_status == "ADJUSTING") {
      MGWARN("IA: LWMA window adjusting. Avg block time: " 
             << lwma_state.average_solve_time << "s");
    }
    
    // Check for recovery
    uint64_t recovery_blocks = 0;
    double recovery_speed = 0.0;
    if (ia_detect_recovery_in_progress(recovery_blocks, recovery_speed)) {
      MGINFO("IA: Network recovering. ETA: " << recovery_blocks << " blocks");
    }
    
    // Check for anomalies
    double anomaly_percent = 0.0;
    if (ia_detect_hashrate_anomaly(get_difficulty_for_next_block(), anomaly_percent)) {
      MGWARN("IA: Hashrate anomaly detected: " << anomaly_percent << "%");
      
      // Log recommendations
      std::string recommendation = ia_recommend_hashrate_recovery();
      MGWARN(recommendation);
    }
  }
  
  return true;
}
```

---

## 4. LOGGING DETALLADO (cada 100 bloques o evento importante)

**Ubicación:** En función de actualización del status del daemon

```cpp
// Cada 100 bloques o cuando se detecta una anomalía importante

if (current_height % 100 == 0) {
  // IA INTEGRATION: Log full status
  ia_log_hashrate_status();
}

// O cuando se detecta anomalía:
if (ia_detect_hashrate_anomaly(current_difficulty, anomaly_percent)) {
  ia_log_hashrate_status();
}
```

---

## 5. PREDICCIÓN PARA SIGUIENTE DIFICULTAD

**Ubicación:** En la función de cálculo de siguiente dificultad

```cpp
// En blockchain.cpp, función next_difficulty(), después de calcular LWMA:

uint64_t Blockchain::next_difficulty(difficulty_type base_difficulty)
{
  // ... existing LWMA calculation code ...
  
  uint64_t lwma_result = /* calculated */;
  
  // IA INTEGRATION: Get IA prediction for reference
  uint64_t ia_predicted = ia_predict_next_difficulty(base_difficulty);
  
  // Use LWMA as primary, but IA prediction helps with logging
  MGINFO("IA Prediction: " << ia_predicted << ", LWMA Result: " << lwma_result);
  
  return lwma_result;
}
```

---

## 6. ESTIMACIÓN DE HASHRATE PARA LOGS/UI

**Ubicación:** En función que reporta estadísticas de red

```cpp
// En función que genera reporte de red/daemon status:

double network_hashrate = ia_estimate_network_hashrate(
    get_difficulty_for_next_block());

MGINFO("Estimated Network Hashrate: " 
       << std::fixed << std::setprecision(2) 
       << network_hashrate << " KH/s");
```

---

## ARCHIVOS A INCLUIR EN CMakeLists.txt

Agregar a `src/CMakeLists.txt`:

```cmake
set(cryptonote_core_sources
  # ... existing files ...
  cryptonote_core/blockchain.cpp
  # ... existing files ...
  ai/ai_hashrate_recovery_monitor.cpp
  ai/ai_checkpoint_monitor.cpp        # previous phase
)

set(cryptonote_core_headers
  # ... existing files ...
  ai/ai_hashrate_recovery_monitor.hpp
  ai/ai_checkpoint_monitor.hpp        # previous phase
)
```

---

## EJEMPLO COMPLETO: Integración Minimal

```cpp
// En blockchain.cpp

#include "ai/ai_hashrate_recovery_monitor.hpp"

// ============ INITIALIZATION ============
void Blockchain::init() {
  // ... existing code ...
  ia_initialize_hashrate_learning();
}

// ============ PER BLOCK ============
bool Blockchain::add_new_block(const block& bl, block_verification_context& bvc) {
  // ... existing validation ...
  
  if (bvc.m_added_to_main_chain) {
    uint64_t height = get_current_blockchain_height();
    uint64_t difficulty = get_difficulty_for_next_block();
    uint64_t solve_time = /* calculate from timestamps */;
    
    // IA learns
    ia_learns_difficulty_state(height, difficulty, solve_time, false);
  }
  
  // Periodic analysis every 60 blocks
  if (height % 60 == 0) {
    auto lwma = ia_analyze_lwma_window(height);
    uint64_t recovery_blocks = 0;
    double recovery_speed = 0.0;
    bool recovering = ia_detect_recovery_in_progress(recovery_blocks, recovery_speed);
    
    if (recovering) {
      MGINFO("Network recovering. ETA: " << recovery_blocks << " blocks");
    }
  }
  
  // Detailed logging every 100 blocks
  if (height % 100 == 0) {
    ia_log_hashrate_status();
  }
  
  return true;
}
```

---

## TESTING

Para verificar que está funcionando:

```bash
# En los logs, deberías ver:
# 1. Mensaje de inicialización:
grep "IA Hashrate Recovery Monitor initialized" debug.log

# 2. Aprendizaje de bloques:
grep "IA Hashrate Learning" debug.log

# 3. Eventos importantes:
grep "EDA Event" debug.log
grep "Recovery in progress" debug.log

# 4. Cada 100 bloques, reporte completo:
grep "IA HASHRATE RECOVERY MONITOR STATUS" debug.log
```

---

## NOTAS DE INTEGRACIÓN

### Performance
- Cada llamada a `ia_learns_difficulty_state()` es O(1) - muy rápido
- `ia_analyze_lwma_window()` es O(60) - analiza ventana de 60 bloques
- `ia_log_hashrate_status()` es O(n) pero solo se llama condicional

### Memory
- Guarda histórico de últimos 200 estados de dificultad
- ~16KB de overhead para la IA

### Thread Safety
- Si blockchain.cpp usa multi-threading, necesita mutex para g_hashrate_knowledge
- Agregar en header si es necesario:
  ```cpp
  static std::mutex g_hashrate_mutex;
  // ... en cada función crítica:
  std::lock_guard<std::mutex> lock(g_hashrate_mutex);
  ```

### Debugging
- Para resetear la IA (en testing):
  ```cpp
  ia_reset_hashrate_learning();
  ```

---

## CHECKLIST DE INTEGRACIÓN

- [ ] Incluir header: `#include "ai/ai_hashrate_recovery_monitor.hpp"`
- [ ] Agregar ai_hashrate_recovery_monitor.cpp a CMakeLists.txt
- [ ] Llamar ia_initialize_hashrate_learning() en init()
- [ ] Llamar ia_learns_difficulty_state() cada bloque válido
- [ ] Llamar ia_analyze_lwma_window() cada 60 bloques
- [ ] Llamar ia_log_hashrate_status() cada 100 bloques
- [ ] Compilar y verificar sin errores
- [ ] Ejecutar nodo y monitorear logs
- [ ] Verificar mensajes "IA" en debug.log
- [ ] Probar con cambios de hashrate dinámicos
- [ ] Verificar EDA detection funciona

---

## SOPORTE

Contactar a la IA si hay preguntas sobre:
- Cómo interpretar los logs
- Cuándo ajustar parámetros
- Cómo extender el sistema
