# NINACATCOIN - FASE 2: CONSENSO P2P Y REPUTACIÓN DE NODOS

## Estado Actual: ESTRUCTURA LISTA PARA IMPLEMENTACIÓN

---

## 📁 Estructura de Carpetas Creada

```
ninacatcoin/
├── informacion/
│   └── DESIGN_CONSENSUS_P2P.md       ← Documento de diseño completo
├── tools/
│   ├── security_query_tool.hpp       ← Gestión de queries P2P
│   ├── reputation_manager.hpp        ← Sistema de reputación
│   └── README.md                     ← Guía de uso de herramientas
├── backup/
│   └── checkpoints_BACKUP_20260125_FUNCIONAL.cpp  ← Copia de seguridad
└── ... (resto del proyecto)
```

---

## 🔒 Componentes Implementados

### FASE 1: VALIDACIÓN DE HASH (✅ COMPLETADO)
```
Estado: FUNCIONAL EN PRODUCCIÓN
├─ Detección de hash inválido ✅
├─ Generación de reporte de seguridad ✅
├─ PAUSE MODE indefinido ✅
├─ Reintentos cada 30 segundos ✅
└─ Auto-reparación desde seeds ✅
```

### FASE 2: CONSENSO P2P Y REPUTACIÓN (🚀 EN DISEÑO)
```
Estado: DISEÑO COMPLETADO, LISTO PARA CODIFICAR

Componentes de design:
├─ SecurityQuery/Response structures
├─ ConsensusResult calculation
├─ NodeReputation tracking
├─ ReputationManager persistence
├─ QueryManager lifecycle
└─ Integration points

Características:
├─ Detectar si ataque es LOCAL vs RED
├─ Reputación de nodos (0.0 a 1.0)
├─ Consenso mínimo (2/3)
├─ Decay temporal (olvido de reportes)
├─ Firmas digitales criptográficas
├─ Persistencia en disco (JSON)
└─ PAUSE MODE SIN CAMBIOS ✅
```

---

## 📊 Comparación: Antes vs Después

### ANTES (FASE 1 SOLO)
```
Nodo A detecta ataque
    ↓
Entra en PAUSE MODE
    ↓
Intenta seeds cada 30s
    ↓
Se recupera o espera indefinidamente
    ↓
❌ Otros nodos NO SABEN del ataque
```

### DESPUÉS (FASE 1 + FASE 2)
```
Nodo A detecta ataque
    ↓ [NEW]
Pregunta a peers: "¿Ustedes también lo ven?"
    ↓ [NEW]
Calcula consenso (2/3 confirmaron)
    ↓ [NEW]
Si es ataque RED:
    ├─ Broadcast alert a toda la red
    ├─ Otros nodos lo evitan proactivamente
    └─ Actualiza reputación de nodos
    ↓
Entra en PAUSE MODE (sin cambios)
    ↓
Se recupera con seeds válidos
    ↓
✅ TODA LA RED ESTÁ PROTEGIDA
```

---

## 🛠️ Lo que Falta (Próximos Pasos)

### Sprint 1: Implementación Base
```
[ ] 1. Crear security_query_tool.cpp
      - Funciones de serialización
      - Validación de firmas
      - Cálculo de consenso
      
[ ] 2. Crear reputation_manager.cpp
      - Cálculo de scores
      - Persistencia en JSON
      - Decay temporal
      
[ ] 3. Integrar en checkpoints.h
      - Declarar nuevos miembros
      - Incluir headers de herramientas
```

### Sprint 2: Integración
```
[ ] 4. Modificar load_checkpoints_from_json()
      - Crear y enviar queries
      - Procesar respuestas
      - Actualizar reputación
      
[ ] 5. Agregar handlers P2P
      - Recibir queries
      - Enviar respuestas
      - Recibir alertas
```

### Sprint 3: Testing
```
[ ] 6. Unit tests para security_query_tool
[ ] 7. Unit tests para reputation_manager
[ ] 8. E2E tests con múltiples nodos
[ ] 9. Tests de seguridad
```

### Sprint 4: Deployment
```
[ ] 10. Documentación final
[ ] 11. Configuración por defecto
[ ] 12. Release notes
[ ] 13. Capacitación de usuarios
```

---

## 🔑 Características Clave del Diseño

### 1. Detección Local vs Red
```
Ataque LOCAL (malware en PC):
  - Solo ESTE nodo lo ve
  - Se registra localmente
  - NO se reporta a red
  - Otros nodos SIN IMPACTO

Ataque RED (seed comprometido):
  - MÚLTIPLES nodos lo ven
  - Se reporta automáticamente
  - Toda la red se protege
  - Atacante FALLA
```

### 2. Sistema de Reputación Robusto
```
Nuevo nodo: score = 0.5 (neutral)
Reporte confirmado: +0.05
Reporte falso: -0.05
Threshold para confiar: 0.40
Mínimo posible: 0.10
Máximo posible: 1.00

Prevención de ataques Sybil:
  - Nuevos nodos empiezan sin confianza
  - Deben probar ser honestos primero
  - Reportes falsos = ban automático
```

### 3. Consenso Distribuido Seguro
```
Regla: ≥2 nodos confirmando = CONSENSO
       (>66% de respuestas positivas)

Ejemplo:
  - 3 nodos responden: ≥2 confirmando ✅
  - 5 nodos responden: ≥3 confirmando ✅
  - 1 nodo reporta solo: LOCAL ❌
  - Nodo con mal reputation: IGNORADO ❌
```

### 4. Protección Criptográfica
```
Cada query/respuesta:
  - Tiene firma digital
  - Se verifica autenticidad
  - Previene spoofing
  - Imposible falsificar sin claves privadas

Prevención de replay:
  - ID único por query
  - Timestamp
  - Nonce aleatorio
```

---

## 📝 Archivos de Configuración

Nuevo en `ninacatcoin.conf`:

```ini
# Consenso P2P
consensus-peer-count = 3           # Cuántos peers consultar
consensus-threshold = 0.66         # Porcentaje para confirmar
consensus-timeout = 10             # Segundos para esperar respuesta

# Reputación
reputation-threshold = 0.40        # Score mínimo para confiar
reputation-decay-days = 30         # Olvido de reportes antiguos
reputation-update-factor = 0.1     # Cambio por reporte

# Directorios
reputation-data-dir = ~/.ninacatcoin/testnet/reputation/
consensus-log-dir = ~/.ninacatcoin/testnet/consensus_logs/
```

---

## 📚 Documentación Generada

### En carpeta `informacion/`:
1. **DESIGN_CONSENSUS_P2P.md** (14 secciones)
   - Visión general
   - Componentes principales
   - Algoritmos detallados
   - Flujos de ejemplo
   - Configuración
   - FAQ

### En carpeta `tools/`:
1. **security_query_tool.hpp** 
   - SecurityQuery / Response structs
   - QueryManager class
   - Funciones de consenso

2. **reputation_manager.hpp**
   - NodeReputation struct
   - ReputationManager class
   - ReputationStats

3. **README.md**
   - Guía de uso
   - Ejemplos de código
   - Testing
   - Integration guide

---

## 🔄 Flujo de Trabajo Recomendado

### Semana 1: Infraestructura
```
Día 1-2:  Crear .cpp files (implementación)
Día 3-4:  Agregar serialización
Día 5:    Tests básicos
```

### Semana 2: Lógica de Consenso
```
Día 1-2:  Implementar calculate_consensus()
Día 3-4:  QueryManager lifecycle
Día 5:    Tests de consenso
```

### Semana 3: Reputación
```
Día 1-2:  Sistema de scores
Día 3-4:  Persistencia en JSON
Día 5:    Decay temporal
```

### Semana 4: Integración
```
Día 1-2:  Integrar en checkpoints.cpp
Día 3-4:  Handlers P2P
Día 5:    E2E testing
```

---

## ✅ Checklist de Validación

Antes de deployment a producción:

```
[ ] Todos los tests pasan
[ ] Sin memory leaks (valgrind)
[ ] Sin race conditions (threadsan)
[ ] Compilación sin warnings
[ ] Documentación completa
[ ] Ejemplos de configuración
[ ] Guía de troubleshooting
[ ] Performance: <100ms por query
[ ] Network: Funciona con peers offline
[ ] Reputación: Persiste correctamente
[ ] Consenso: Rechaza falsos positivos
```

---

## 🎯 Objetivos de la FASE 2

| Objetivo | Estado | Beneficio |
|----------|--------|----------|
| Detectar ataques locales vs red | ✅ Diseño | Menos falsos positivos |
| Sistema de reputación | ✅ Diseño | Confianza en reportes |
| Consenso distribuido | ✅ Diseño | Inmunidad a nodos malos |
| Protección criptográfica | ✅ Diseño | Anti-spoofing |
| Persistencia de reputación | ✅ Diseño | Memoria institucional |
| Decay temporal | ✅ Diseño | Olvido de errores pasados |

---

## 🚀 Pasos Siguientes (USER ACTION)

Cuando estés listo para comenzar:

1. **Confirmar diseño**
   - Revisar DESIGN_CONSENSUS_P2P.md
   - Preguntar si hay cambios

2. **Iniciar Implementación (Sprint 1)**
   - Crear security_query_tool.cpp
   - Crear reputation_manager.cpp
   - Tests unitarios

3. **Integración (Sprint 2)**
   - Modificar checkpoints.cpp
   - Agregar handlers P2P
   - E2E testing

---

## 📞 Soporte y Contacto

Cualquier pregunta sobre el diseño, envía mensaje en este formato:

```
[PREGUNTA CONSENSO P2P]

Sección: [nombre de la sección en DESIGN_CONSENSUS_P2P.md]
Duda: [tu pregunta específica]
Contexto: [información adicional]
```

---

**ESTADO ACTUAL: ✅ FASE 1 FUNCIONAL + FASE 2 DISEÑADO**

**PRÓXIMO HITO: Implementación de Sprint 1 (security_query_tool.cpp)**

**FECHA: 25 de enero de 2026**
