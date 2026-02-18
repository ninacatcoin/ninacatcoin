# 📋 Cambios Aplicados al Diseño - Quarantine Temporal

**Fecha:** 25 de enero de 2026  
**Cambio:** Protección para nodos bajo ataque selectivo  
**Versión:** Diseño v1.1

---

## ✅ Cambios Realizados

### 1. Nueva Sección 2.5: "Detección de Nodo Bajo Ataque y Quarantine Temporal"

**Localización:** `DESIGN_CONSENSUS_P2P.md` - Después de 2.4, antes de 3.

**Contenido:**
- ✅ Problema identificado (nodo bueno bajo ataque selectivo pierde reputación)
- ✅ Solución: Quarantine temporal (1-6 horas)
- ✅ Indicadores de ataque selectivo (5+ reportes misma fuente)
- ✅ Estructura de datos (`QuarantinedNode`)
- ✅ Lógica de detección automática
- ✅ Activación/desactivación de quarantine
- ✅ **Aviso en terminal** (formato destacado con bordes ASCII)
- ✅ Protección de reputación durante quarantine
- ✅ Salida limpia con restauración de reputación
- ✅ Configuración en `ninacatcoin.conf`

---

## 📊 Características del Aviso Terminal

```
╔══════════════════════════════════════════════════════════════╗
║              ⚠️  ATAQUE DETECTADO - QUARANTINE  ⚠️              ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  Tu nodo está siendo ATACADO SELECTIVAMENTE                 ║
║                                                              ║
║  Fuente de ataque:  seed1.ejemplo.com                       ║
║  Tipo:              Checkpoints corruptos (LOCAL)            ║
║  Estado:            EN CUARENTENA TEMPORAL                   ║
║  Duración:          3h 45m 23s                              ║
║                                                              ║
║  Acción automática:                                          ║
║  • Tus reportes NO afectarán tu reputación                   ║
║  • Continuarás intentando recuperar datos válidos            ║
║  • Cuando se confirme el ataque, +reputación                 ║
║                                                              ║
║  Recomendación:                                              ║
║  • Verifica tu conexión a internet                           ║
║  • Verifica que no haya malware en tu PC                     ║
║  • Considera usar una VPN si usas conexión pública           ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
```

---

## 🔧 Parámetros de Configuración (Nuevos)

```ini
quarantine-enabled = true                 # Activar/desactivar
quarantine-min-duration = 3600            # 1 hora mínimo
quarantine-max-duration = 21600           # 6 horas máximo
quarantine-attack-threshold = 5           # Cuántos reportes activan quarantine
quarantine-time-window = 3600             # Período a evaluar (1 hora)
quarantine-source-ratio = 0.80            # % mínimo misma fuente
quarantine-hash-ratio = 0.80              # % mínimo mismo hash incorrecto
```

---

## 🛡️ Protecciones Implementadas

| Protección | Antes | Después |
|-----------|-------|---------|
| **Nodo bajo ataque pierde reputación** | ✗ Vulnerable | ✅ Protegido con Quarantine |
| **Aviso al usuario** | ✗ No había | ✅ Terminal + Logging |
| **Recuperación de reputación** | ✗ Permanente | ✅ Automática al salir |
| **Recompensa si se confirma** | ✗ No | ✅ +0.2 reputación |
| **Durabilidad** | - | ✅ Guardado en JSON |

---

## 📈 Escenarios de Uso

### Escenario 1: Nodo Bueno Bajo Ataque LOCAL

```
Tiempo 0: Nodo A conecta a seed1 comprometida (solo para A)
├─ Recibe checkpoints corruptos
├─ Detecta problema
└─ Genera reporte

Tiempo 5m: Nodo A genera 5 reportes
├─ Sistema detecta patrón: 5+ reportes, misma fuente
├─ Otros nodos confirman: seed1 responde bien
├─ CONCLUSIÓN: Nodo A bajo ataque selectivo
└─ ACCIÓN: Quarantine por 1-6h

Tiempo 6h: Termina quarantine
├─ Reputación restaurada
├─ Si ataque se confirma en red: +0.2 reputación
├─ Aviso limpiado
└─ Nodo regresa a normal (más fuerte)
```

### Escenario 2: Nodo Malicioso Falso Positivo

```
Tiempo 0: Nodo Malicioso reporta falsamente seed1
├─ Otros confirman: seed1 OK
├─ Reputación malicioso: -0.05
└─ Siguiente reporte será ignorado (rep < 0.40)

(No llega a 5 reportes porque será ignorado rápido)
```

---

## 📝 Checklist de Integración

- [ ] Leer cambios en sección 2.5
- [ ] Revisar indicadores de ataque selectivo
- [ ] Entender duración 1-6h configurable
- [ ] Ver estructura QuarantinedNode
- [ ] Notar persistencia en JSON
- [ ] Revisar aviso terminal formateado
- [ ] Entender que reputación se restaura
- [ ] Ver parámetros en ninacatcoin.conf
- [ ] Confirmar que PAUSE MODE sigue igual

---

## 🚀 Listo para Sprint 1

El diseño está **completo y validado**. Cambios integrados sin conflictos.

**Próximo paso:** Implementar `security_query_tool.cpp` siguiendo `SPRINT_1_INSTRUCTIONS.md`

---

## 📞 Referencia Rápida

**Si tienes dudas sobre:**
- Quarantine → Ver sección 2.5
- Configuración → Ver sección 9
- FAQ → Ver sección 12
- Escenarios → Ver este documento

---

**Status:** ✅ DISEÑO COMPLETO Y VALIDADO  
**Versión:** 1.1  
**Listo para:** SPRINT 1 IMPLEMENTATION
