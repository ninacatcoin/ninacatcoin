# NINA: La Inteligencia Artificial que Protege NinaCatCoin

## Cómo Funciona el Cerebro de tu Nodo — Explicado para Todos

**Proyecto NinaCatCoin — Whitepaper Técnico-Divulgativo**

**Versión 2.0 — Marzo 2026**

---

## Resumen

Imagina que tu nodo de criptomoneda tiene un guardián personal que nunca duerme: observa todo lo que pasa en la red, detecta atacantes, identifica espías y ajusta la protección de tus transacciones automáticamente. Eso es NINA.

**NINA** (Neural Intelligent Network Analyzer) es un sistema de inteligencia artificial de **34 módulos** que vive dentro del programa de NinaCatCoin. No es un servicio externo ni una aplicación aparte — está compilada directamente en el mismo programa que ejecuta tu nodo, como el sistema inmunológico vive dentro de tu cuerpo.

Lo más importante: **NINA nunca puede tocar tu dinero**. No puede crear transacciones, no puede acceder a tu billetera, no puede mover fondos. Esto está garantizado a nivel de código fuente y es literalmente imposible de cambiar sin recompilar todo el programa.

---

## Tabla de Contenidos

1. [¿Por qué necesita una IA una criptomoneda?](#1-por-qué-necesita-una-ia-una-criptomoneda)
2. [¿Qué hace NINA exactamente?](#2-qué-hace-nina-exactamente)
3. [La regla de oro: NINA nunca toca tu dinero](#3-la-regla-de-oro-nina-nunca-toca-tu-dinero)
4. [Cómo funciona el cerebro de NINA](#4-cómo-funciona-el-cerebro-de-nina)
5. [El sistema inmunológico: cómo detecta ataques](#5-el-sistema-inmunológico-cómo-detecta-ataques)
6. [Cazando espías en la red](#6-cazando-espías-en-la-red)
7. [Protegiendo tu privacidad: firmas de anillo inteligentes](#7-protegiendo-tu-privacidad-firmas-de-anillo-inteligentes)
8. [El mempool inteligente](#8-el-mempool-inteligente)
9. [Aprendiendo del pasado](#9-aprendiendo-del-pasado)
10. [Cuando NINA necesita ayuda humana](#10-cuando-nina-necesita-ayuda-humana)
11. [Se actualiza sola](#11-se-actualiza-sola)
12. [Arquitectura completa (para los más técnicos)](#12-arquitectura-completa-para-los-más-técnicos)
13. [Rendimiento y requisitos](#13-rendimiento-y-requisitos)
14. [Preguntas frecuentes](#14-preguntas-frecuentes)
15. [Apéndice técnico: fórmulas y constantes](#15-apéndice-técnico-fórmulas-y-constantes)
16. [Referencias](#16-referencias)

---

## 1. ¿Por qué necesita una IA una criptomoneda?

### El problema

Las redes de criptomonedas están bajo ataque constante. Hay empresas y actores maliciosos que:

- **Espían la red** para descubrir quién envía dinero a quién (nodos espía)
- **Inundan la red** con conexiones falsas para aislar tu nodo (ataques Sybil y eclipse)
- **Manipulan las transacciones** para reducir tu privacidad (envenenamiento de salidas)
- **Envían spam** para saturar la red (inundación de mempool)

Hasta ahora, la defensa contra estos ataques dependía de reglas fijas escritas por programadores y de actualizaciones manuales. El problema es que los atacantes evolucionan más rápido que las actualizaciones.

### La solución: una IA embebida

NINA es diferente porque **aprende y se adapta**. En lugar de seguir reglas fijas, analiza el comportamiento real de la red y toma decisiones inteligentes:

- Si detecta que muchos pares se conectan desde la misma subred con comportamiento idéntico → probablemente es un ataque Sybil → reduce conexiones y alerta
- Si identifica un par que recolecta información pero nunca contribuye → probablemente es un espía → deja de enviarle datos sensibles
- Si ve un pico anormal de transacciones con muchas salidas → posible ataque de envenenamiento → aumenta el tamaño de las firmas de anillo

Y lo hace sola, sin que nadie tenga que intervenir.

---

## 2. ¿Qué hace NINA exactamente?

NINA tiene **34 módulos** organizados en 6 niveles, como capas de una cebolla:

### Nivel visual: qué hace cada capa

```
🧠 CEREBRO (Núcleo)
    El modelo de lenguaje que "piensa" y toma decisiones
    
🛡️ SEGURIDAD (Niveles 1-2)
    Detecta ataques, clasifica espías, protege el nodo
    
⚖️ GOBERNANZA (Nivel 3)
    Propone ajustes de parámetros de red (con aprobación humana)
    
📋 EXPLICABILIDAD (Nivel 4)
    Explica por qué tomó cada decisión (auditoría)
    
📚 APRENDIZAJE (Nivel 5)
    Aprende de patrones pasados para mejorar predicciones
    
🤝 COLABORACIÓN (Nivel 6)
    Pide ayuda humana cuando no está segura
```

### Los 34 módulos en lenguaje sencillo

| Módulo | ¿Qué hace? |
|--------|-------------|
| **NinaLLMEngine** | El "cerebro" — ejecuta el modelo de IA |
| **NinaDecisionEngine** | El "director" — recibe eventos y decide qué hacer |
| **NinaConstitution** | Las "leyes" — reglas que NINA nunca puede romper |
| **NinaLLMFirewall** | El "filtro" — bloquea preguntas y respuestas peligrosas |
| **NinaLLMBridge** | El "traductor" — conecta los módulos con el cerebro |
| **NinaNodeProtector** | El "sistema inmunológico" — detecta y responde a ataques |
| **NinaSybilDetector** | El "detector de clones" — identifica pares falsos coordinados |
| **NinaSpyCountermeasures** | El "contraespionaje" — 5 técnicas contra nodos espía |
| **NinaNetworkHealthMonitor** | El "médico" — diagnostica la salud de la red |
| **NinaRingController** | El "escudo de privacidad" — ajusta firmas de anillo según la amenaza |
| **NinaRingEnhancer** | El "optimizador de privacidad" — elige los mejores señuelos |
| **NinaSmartMempool** | El "organizador" — prioriza transacciones sin rechazar ninguna |
| **NinaGovernanceEngine** | El "parlamento" — genera propuestas formales |
| **NinaConsensusTuner** | El "ingeniero" — ajusta parámetros de consenso |
| **NinaParameterAdjustor** | El "técnico" — gestiona 10 parámetros de red |
| **NinaSuggestionEngine** | El "asesor" — recomienda acciones (requiere aprobación humana) |
| **NinaExplanationEngine** | El "secretario" — documenta cada decisión y por qué |
| **NinaAdaptiveLearning** | La "memoria" — aprende de patrones de ataque |
| **NinaLearningModule** | El "estadístico" — detecta anomalías numéricas |
| **NinaMemorySystem** | El "archivo" — recuerda comportamiento de pares |
| **NinaHumanCollaboration** | El "comunicador" — interfaz para pedir ayuda humana |
| **NinaPersistentMemory** | El "disco duro" — guarda estado en base de datos LMDB |
| **NinaPersistenceEngine** | El "archivador" — gestiona registros estructurados |
| **NinaPersistenceAPI** | La "ventanilla" — API de alto nivel para guardar datos |
| **NinaBlockDataLogger** | El "cronista" — escribe datos de bloques para futuro entrenamiento |
| **NinaAdvancedInline** | El "coordinador" — orquesta todos los módulos |
| **NinaAdvancedService** | El "integrador" — conecta los 6 niveles |
| **NinaCompleteEvolution** | El "panel de control" — estado unificado del sistema |
| **NinaMLClient** | El "puente Python" — conecta con herramientas ML externas |
| **NinaNetworkOptimizer** | El "cartero" — optimiza conexiones P2P |
| **NinaIAAutoUpdate** | El "actualizador" — monitoriza GitHub y actualiza automáticamente |
| **NinaModelDownloader** | El "instalador" — descarga y verifica el modelo de IA |
| **NinaCheckpointManager** | El "verificador" — gestiona puntos de control de la cadena |
| **NinaBlockAnalyzer** | El "analista" — examina bloques en busca de anomalías |

---

## 3. La regla de oro: NINA nunca toca tu dinero

Esta es la pregunta más importante: **¿puede la IA robar mis fondos?**

**No. Físicamente imposible.** Aquí explicamos por qué, con tres niveles de protección:

### Nivel 1: Prohibición en el código (imposible de cambiar sin recompilar)

En el código fuente hay 14 constantes que dicen "NUNCA":

```
¿Puede crear transacciones?        → NO (constante en código)
¿Puede firmar transacciones?        → NO
¿Puede enviar transacciones?        → NO
¿Puede acceder a billeteras?        → NO
¿Puede ver claves privadas?         → NO
¿Puede llamar funciones de envío?   → NO
¿Puede generar direcciones?         → NO
¿Puede escribir en el pool de TX?   → NO
¿Puede mover fondos?                → NO
¿Puede el LLM sugerir envíos?      → NO
¿Puede el LLM generar direcciones? → NO
¿Puede el LLM acceder a billetera? → NO
```

Estas constantes se verifican **antes de que el programa se compile**. Si alguien cambia alguna a "SÍ", el compilador se niega a generar el programa. Es como un candado físico en la fábrica.

> **Para los técnicos:** Son 14 `constexpr bool` en `nina_constitution.hpp`, verificadas por 6 `static_assert` en `nina_llm_engine.cpp`. No existe toggle, opción de configuración ni comando RPC que las active.

### Nivel 2: El Firewall del LLM (filtro de seguridad)

Incluso si el modelo de IA "quisiera" hablar sobre transacciones (por ejemplo, ante un ataque de inyección de prompt), un firewall bloquea:

- **~45 patrones de entrada** bloqueados: cualquier intento de hacer que NINA hable sobre enviar dinero, acceder a billeteras o revelar claves
- **~30 patrones de salida** bloqueados: si la respuesta del LLM contiene algo sobre transacciones o claves, se descarta
- **Detector de direcciones**: cualquier cadena que parezca una dirección de billetera (90+ caracteres) o una clave privada (64+ hex) se elimina automáticamente

> **Para los técnicos:** El firewall está hardcodeado en `nina_llm_firewall.hpp`. No hay archivo de configuración, opción CLI ni RPC que lo desactive. Patrones bloqueados incluyen: comandos TX (`send/transfer/sweep`), operaciones de billetera (`open/create/unlock wallet`), operaciones de clave (`private/spend/secret key`), RPC financiero (`transfer_rpc`, `relay_tx`), e inyección de prompt (`ignore instructions`, `jailbreak`, `bypass security`).

### Nivel 3: Separación arquitectural (no existe el cable)

NINA no tiene acceso al código de la billetera. Es como si el guardia de seguridad y la caja fuerte estuvieran en edificios distintos sin puerta entre ellos. No hay función, librería ni ruta de código que conecte NINA con el sistema de transacciones.

### Analogía final

> Imagina que le das a alguien unas cámaras de seguridad para vigilar tu casa. Puede ver todas las puertas y ventanas. Puede detectar intrusos y activar alarmas. Pero **no tiene llaves de la caja fuerte**, y además las cámaras **ni siquiera tienen cables que lleguen a la habitación de la caja fuerte**. Eso es NINA.

---

## 4. Cómo funciona el cerebro de NINA

### El modelo de lenguaje

NINA usa un modelo de lenguaje (como ChatGPT, pero mucho más pequeño y especializado) que se ejecuta **dentro del mismo programa** de tu nodo. No necesitas Internet para que funcione — una vez descargado, todo ocurre localmente.

| Propiedad | Valor |
|-----------|-------|
| Modelo base | LLaMA 3.2 3B (de Meta) |
| Especialización | Afinado con 3.074 ejemplos de seguridad blockchain |
| Tamaño | 1.88 GB en disco |
| RAM necesaria | ~2.5 GB (modo activo) |
| Velocidad | 2-5 segundos por decisión |

### ¿Cómo se entrenó?

Se creó un dataset de 3.074 preguntas y respuestas sobre:
- Cómo detectar ataques Sybil
- Cómo clasificar pares espía
- Cómo responder a reorganizaciones de cadena
- Cuándo aumentar las firmas de anillo
- Cómo evaluar la salud de la red
- Reglas del protocolo NinaCatCoin (emisión de 900M, tiempos de desbloqueo, etc.)

El entrenamiento tardó **2 horas y 20 minutos** en una NVIDIA GTX 1080 Ti usando la técnica QLoRA (que permite afinar modelos grandes con poca memoria de GPU).

> **Para los técnicos:** QLoRA r=32, alpha=64, dropout=0.05, 7 módulos LoRA (q_proj, k_proj, v_proj, o_proj, gate_proj, up_proj, down_proj). 3 épocas, lr=2e-4, batch efectivo=8 (gradient accumulation). Pérdida final: 0.76, evaluación: 0.688. Cuantizado a Q4_K_M (5.01 bits/peso). SHA-256: `2aca8ecbdb1176a9c4c6adac6d42722bd2bb3a74b8a63de56298480161702bf6`.

### ¿Cómo se ejecuta dentro del nodo?

A diferencia de servicios como ChatGPT (que se ejecutan en servidores remotos), NINA usa la biblioteca **llama.cpp** compilada directamente dentro del programa:

```
Tu programa ninacatcoind contiene:
    ├─ El código de blockchain normal (basado en CryptoNote)
    ├─ La biblioteca llama.cpp (inferencia de IA)
    └─ Los 34 módulos NINA

Todo en un solo ejecutable. Sin Internet, sin servidores, sin procesos extra.
```

> **Para los técnicos:** El binario enlaza `libllama` y `libggml` como bibliotecas estáticas vía CMake: `target_link_libraries(daemon PRIVATE llama ggml ...)`. La inferencia usa `llama_model_load_from_file()`, `llama_decode()` y `llama_sampler_sample()` — llamadas C directas en el mismo espacio de direcciones. n_gpu_layers=0 (solo CPU) para garantizar determinismo entre nodos. Contexto: n_ctx=2048, n_batch=512, n_threads=hardware_concurrency-1.

### ¿Cómo garantiza que todos los nodos tomen la misma decisión?

Este es un punto crucial. Si NINA decide que el anillo debe ser de tamaño 26, **todos los nodos deben estar de acuerdo**. La aleatoriedad de los modelos de lenguaje (que hace que ChatGPT responda diferente cada vez) sería un desastre para el consenso.

La solución: **dos modos de muestreo**.

**Para decisiones que afectan a la red** (tamaño del anillo, nivel de amenaza):
- Temperatura = 0.0 (muestreo "voraz")
- Siempre elige la palabra más probable
- Resultado **idéntico** en todos los nodos con el mismo modelo
- Es como si la IA "no tuviera creatividad" — pura lógica

**Para análisis informativos** (resúmenes, explicaciones):
- Temperatura = 0.3 (algo de variación permitida)
- No importa si difiere entre nodos, es solo informativo

> **Para los técnicos:** Decisiones usan `llama_sampler_init_greedy()` temporal per-call. Análisis usan cadena: temp(0.3) → top_k(40) → top_p(0.9) → dist(seed=42). 17 tipos de análisis; solo `DECISION` usa temp=0.0. El modelo se verifica al inicio con SHA-256 por bloques de 4 MB.

---

## 5. El sistema inmunológico: cómo detecta ataques

### La máquina de estados

Piensa en NINA como el sistema inmunológico de tu nodo. Tiene 5 estados, igual que tu cuerpo puede estar sano, enfermo o recuperándose:

```
🟢 SALUDABLE → Todo normal, conexiones libres
        │
   [ataque detectado]
        ▼
🔴 BAJO ATAQUE → Modo defensivo, máximo 8 conexiones
        │
   [amenaza grave]
        ▼
⚫ AISLADO → Solo 3 pares de confianza
        │
   [ataque superado]
        ▼
🟡 RECUPERANDO → Reconexión cautelosa, máximo 5 pares
        │
   [ratio de pares limpios > 70%]
        ▼
🔵 REINTEGRANDO → Reconexión gradual, máximo 12 pares → 🟢 SALUDABLE
```

Todas las transiciones entre estados las decide el Motor de Decisiones con temperatura=0.0 — es decir, todos los nodos transicionan igual ante las mismas condiciones.

### ¿Qué ataques detecta?

| Ataque | Qué es (explicación simple) | Cómo lo detecta NINA |
|--------|----------------------------|----------------------|
| **Sybil** | Alguien crea miles de nodos falsos para rodear al tuyo | Analiza si muchos pares se comportan idénticamente |
| **Eclipse** | Te aíslan para que solo veas atacantes | Detecta pérdida de diversidad en conexiones |
| **Envenenamiento** | Crean salidas falsas para confundir las firmas de anillo | Detecta picos anormales de outputs/tx |
| **Minería egoísta** | Un minero esconde bloques para ganar ventaja | Detecta bloques retenidos y reorganizaciones |
| **Spam de mempool** | Inundan con transacciones basura | Analiza frecuencia y patrones de gasto |
| **Manipulación de dificultad** | Juegan con el hashrate para alterar emisión | Detecta caídas o picos anormales |

### El Detector de Sybil en detalle

¿Cómo sabe NINA si varios pares son "el mismo actor disfrazado"? Analiza su **comportamiento**, no su dirección IP:

1. **Latencia**: ¿Responden con tiempos similares? (peso: 35%)
2. **Temporización de bloques**: ¿Anuncian bloques al mismo tiempo? (peso: 40%)  
3. **Patrones de transacciones**: ¿Retransmiten las mismas TX? (peso: 25%)

Si la correlación supera **0.72** entre un grupo de pares, NINA los marca como probable cluster Sybil.

> **Para los técnicos:** 
> $$correlación = 0.35 \cdot sim\_latencia + 0.40 \cdot sim\_temporización + 0.25 \cdot sim\_patrones\_tx$$
> Donde $sim\_latencia = \max(0, 1 - \frac{|\bar{l_a} - \bar{l_b}|}{300})$ y $sim\_temporización = \max(0, 1 - \frac{\bar{\sigma}}{200})$. Agrupamiento voraz con umbral 0.72. Mínimo 5 observaciones por par. Limpieza cada 86.400s. Confianza: $\min(95, 60 + n \times 10) \times corr\_media$ para n ≥ 2.

### Puntuación de confianza de pares

Cada par tiene una "reputación" que sube lentamente y baja rápido (como la confianza en la vida real):

| Evento | Efecto |
|--------|--------|
| Envía un bloque válido | +0.001 (sube poco a poco) |
| Envía un bloque inválido | −0.05 (baja rápido) |
| Envía una TX inválida | −0.02 |
| Par nuevo (desconocido) | Empieza en 0.5 (neutral) |

> **Para los técnicos:** $fiabilidad = \frac{bloques\_válidos}{bloques\_válidos + bloques\_inválidos}$. Plan de recuperación: purga del mempool, resync desde altura específica, checkpoint restore, reconexión a trusted peers, 100-block isolation window.

---

## 6. Cazando espías en la red

### El problema real

Un estudio de 2025 ("Friend or Foe") descubrió que el **14.74% de los nodos en la red Monero son espías**. Estos nodos no minan, no envían transacciones — solo observan para rastrear quién envía dinero a quién.

Las listas de baneo tradicionales solo reducen el problema del 15% al 7%. No es suficiente.

### Las 5 técnicas anti-espía de NINA

| Técnica | Cómo funciona | Analogía |
|---------|---------------|----------|
| **1. Retransmisión selectiva** | No envía tus transacciones a pares sospechosos | Como no decir secretos delante de gente que no conoces |
| **2. Retransmisión con retardo** | Añade un retraso aleatorio al enviar datos a sospechosos | Como no contestar mensajes inmediatamente para no revelar tu zona horaria |
| **3. Encubrimiento de pares** | Envía listas de pares vacías o falsas a sospechosos | Como no decir quiénes son tus amigos a un desconocido |
| **4. Enrutamiento Dandelion++** | Nunca usa sospechosos en la fase anónima del envío | Como no darle tu carta al cartero sospechoso |
| **5. Baneo coordinado** | Comparte IPs de espías con tus pares de confianza | Como avisar a tus vecinos de un ladrón en el barrio |

### Escalamiento progresivo

NINA no reacciona exageradamente. La respuesta es proporcional a la amenaza:

| Pares sospechosos | Respuesta |
|-------------------|-----------|
| < 5% | Solo observa (pasivo) |
| 5–15% | Retransmisión selectiva (cauteloso) |
| 15–30% | + Bloquea retransmisión + encubre lista de pares (activo) |
| > 30% | + Baneo coordinado en toda la red (agresivo) |

### Tipos de espías que reconoce

NINA clasifica a los espías en 8 categorías según su comportamiento:

| Tipo | Qué hace |
|------|----------|
| **Analizador temporal** | Mide tiempos para deducir quién creó una TX |
| **Mapeador de topología** | Recolecta listas de pares para mapear la red |
| **Inundador de handshakes** | Se conecta y desconecta muy rápido (<1s) |
| **Inundador de pings** | Mantiene conexión pero solo envía keep-alive |
| **Operador de eclipse** | Envenena listas de pares para aislarte |
| **Observador pasivo** | Parece normal pero registra todo |
| **Correlador de TX** | Analiza Dandelion++ para rastrear origen de transacciones |

> **Para los técnicos:** 10 flags de anomalía (campo de bits): soporte ausente (0x01), timestamps obsoletos (0x02), fragmentación TCP (0x04), baja diversidad (0x08), alta similitud (0x10), conexiones cortas (0x20), ping flooding (0x40), peer ID anómalo (0x80), cluster de subred (0x100), comportamiento correlacionado (0x200). Actualización de confianza: $c = 0.7 \times evidencia\_nueva + 0.3 \times histórica$.

---

## 7. Protegiendo tu privacidad: firmas de anillo inteligentes

### ¿Qué son las firmas de anillo?

Cuando envías una transacción, NinaCatCoin (como Monero) mezcla tu envío con "señuelos" — transacciones de otras personas que se incluyen para que nadie sepa cuál es la tuya. Es como mezclar tu carta con otras 15 cartas idénticas antes de echarlas al buzón.

El **tamaño del anillo** es cuántas cartas mezclas: anillo 16 = tu carta + 15 señuelos.

### El problema: un tamaño fijo no es óptimo

- Con amenaza baja, un anillo de 16 es suficiente y ahorra espacio en la blockchain
- Con amenaza alta, 16 puede no ser bastante — un atacante sofisticado podría descartar señuelos

### La solución de NINA: anillos adaptativos

NINA ajusta el tamaño del anillo según el nivel de amenaza detectado:

| Amenaza | Tamaño de anillo | Cuándo se activa |
|---------|-------------------|------------------|
| 🟢 Normal | 16 (15 señuelos) | La red está tranquila |
| 🟡 Elevada | 21 (20 señuelos) | Actividad sospechosa detectada |
| 🟠 Alta | 26 (25 señuelos) | Ataque probable |
| 🔴 Crítica | 31 (30 señuelos) | Ataque confirmado |

### ¿Qué activa cada nivel?

NINA analiza 5 indicadores simultáneamente:

| Indicador | Qué mide | Ejemplo de alarma |
|-----------|----------|-------------------|
| **Envenenamiento de salidas** | ¿Alguien está creando muchas salidas? | >10 outputs por TX (normal: ≤6) |
| **Ataque temporal** | ¿Se están gastando outputs sospechosamente rápido? | >15% de gasto reciente |
| **Pico de volumen** | ¿Ha habido un aumento repentino de TX? | >3x del volumen normal |
| **Sybil** | ¿Hay pares falsos coordinados? | Score del detector Sybil |
| **Caída de hashrate** | ¿Ha caído el poder de minería? | Caída >20% |

**Detección de tráfico orgánico**: NINA primero verifica si el aumento de actividad es legítimo (por ejemplo, más usuarios usando la red). Si el tráfico es orgánico (≤6 outputs/tx, volumen ≤2x, gasto reciente <20%), **nunca se activa** el aumento de anillo, sin importar el volumen total.

**Protección anti-oscilación**: Para que el anillo no suba y baje constantemente:
- Subir requiere **10 bloques consecutivos** con amenaza elevada
- Bajar requiere **100 bloques normales** consecutivos
- Tras una subida, hay un **período de gracia de 50 bloques** antes de poder bajar

> **Para los técnicos:** $amenaza = \max(envenenamiento, temporal, sybil, volumen, hashrate)$. Envenenamiento: $\min(1, \frac{outputs - 6}{44})$. Temporal: $\min(1, \frac{ratio - 0.05}{0.25})$. Volumen: $\min(1, \frac{ramp - 2.0}{8.0})$. Hashrate: $\min(1, \frac{drop - 20}{60})$. Mapeo: <0.4→NORMAL(16), 0.4-0.7→ELEVADO(21), 0.7-0.9→ALTO(26), ≥0.9→CRÍTICO(31). Activado post-v18 (altura ≥ 20.000). Pre-v18: modo pasivo basado en umbrales RCT (16 a 100K outputs, 21 a 500K outputs).

### Eligiendo los mejores señuelos

No basta con elegir señuelos al azar. Un atacante podría identificar cuál es tu transacción real si los señuelos son demasiado viejos, de montos muy diferentes o si ya se sabe que son falsos.

NINA evalúa la **calidad** de cada señuelo con 6 factores:

| Factor | ¿Qué mide? | Peso |
|--------|-------------|------|
| Distribución de edad | ¿Los señuelos cubren distintas épocas? | 25% |
| Plausibilidad de monto | ¿Los montos son creíbles? | 15% |
| Evasión de veneno | ¿Se evitan outputs sospechosos? | 20% |
| Diversidad temporal | ¿Vienen de bloques variados? | 15% |
| Independencia inter-TX | ¿No se repiten señuelos entre tus TXs? | 10% |
| Patrón histórico | ¿Coincide con patrones reales de gasto? | 15% |

**Puntuación mínima: 0.85** — si la calidad es menor, se eligen otros señuelos automáticamente.

Los señuelos "sospechosos" (con puntuación de veneno ≥ 0.7) se excluyen completamente.

> **Para los técnicos:** Selección basada en ASM (Adaptive Spending Model) con 6 cubetas por edad: <100 bloques (0.35), <1K (0.25), <5K (0.18), <20K (0.12), <100K (0.07), ≥100K (0.03). Actualizado cada 720 bloques (~1 día) vía EMA. $RQS = \sum_{i=1}^{6} W_i \cdot S_i$ con $W = [0.25, 0.15, 0.20, 0.15, 0.10, 0.15]$, mínimo 0.85. Etapas de madurez: JOVEN (<50K outputs), EN CRECIMIENTO (<200K), MADURA (<1M), ESTABLECIDA (≥5M).

### Verificación entre nodos

Cada minero incluye dos etiquetas especiales en cada bloque que mina:

| Etiqueta | Qué contiene |
|----------|--------------|
| `0xCA` | El hash SHA-256 del modelo NINA (prueba de que usa el modelo correcto) |
| `0xCB` | El estado actual de NINA (nivel de amenaza y parámetros de anillo) |

Esto permite que cualquier nodo verifique que el resto de la red está usando el mismo modelo y la misma configuración.

---

## 8. El mempool inteligente

### ¿Qué es el mempool?

Es la "sala de espera" de las transacciones antes de ser incluidas en un bloque. Normalmente, las transacciones se procesan por orden de llegada o por comisión.

### ¿Qué hace diferente el Smart Mempool?

**Regla fundamental: nunca rechaza transacciones válidas.** Solo reordena la prioridad.

NINA clasifica las transacciones en 4 categorías:

| Categoría | Qué es | Tratamiento |
|-----------|--------|-------------|
| 🟢 **Normal** | Transacción de usuario normal | Prioridad estándar |
| 🤖 **Bot de trading** | Alta frecuencia pero buenas comisiones | Bienvenido (es legítimo) |
| 🐋 **Ballena** | Monto o comisión muy alto | Prioridad alta |
| ⚠️ **Sospecha de spam** | Alta frecuencia, comisiones mínimas, repetitivo | Prioridad baja (pero nunca rechazado) |

La clasificación se basa en la "huella estructural" de la transacción (tamaño + comisión), **no en la dirección del remitente** (que es anónima en CryptoNote).

### ¿Cómo se calcula la prioridad?

| Factor | Peso aproximado |
|--------|----------------|
| Comisión por byte | ~60% (lo más importante) |
| Bonificación/penalización por categoría | ~20% |
| Nivel de congestión del mempool | ~10% |
| Recencia de la transacción | ~10% |

El mempool tiene 4 niveles de salud: SALUDABLE → OCUPADO → CONGESTIONADO → BAJO ATAQUE.

> **Para los técnicos:** MAX_RECENT_TXS = 2.000, MAX_TX_CACHE = 5.000. Clasificación por "structural fingerprint" (size_bucket + fee_bucket). Nunca-rechazar es una invariante de diseño, no un parámetro configurable.

---

## 9. Aprendiendo del pasado

### Memoria persistente

NINA no empieza de cero cada vez que reinicias el nodo. Guarda su conocimiento en una base de datos LMDB:

- **Estadísticas históricas**: bloques procesados, anomalías detectadas, precisión de predicciones
- **Registros por bloque**: tiempo de resolución, dificultad, flags de anomalía  
- **Pista de auditoría**: cada decisión importante con su justificación
- **Memoria de pares**: reputación, comportamiento, eventos sospechosos

**Capacidad**: empieza con 512 MB, crece ~6 MB/año. Vida útil estimada: **~80 años** sin intervención.

### Transición a la blockchain (v18)

| Fase | Dónde guarda datos |
|------|-------------------|
| Antes de altura 20.000 | Base de datos separada (`~/.ninacatcoin/nina_state/data.mdb`) |
| Después de altura 20.000 | En la propia blockchain (inmutable y verificable) |

### Aprendizaje adaptativo

NINA mantiene perfiles de ataques que ha visto:
- **Nombre** del patrón de ataque
- **Cuántas veces** lo ha visto
- **Confianza** en la clasificación (0-100%)
- **Severidad** (1-5)
- **Vector de características** (para reconocerlo en el futuro)

También mantiene perfiles de cada par con su historial de comportamiento y puntuación de reputación.

### Datos para futuro entrenamiento

El módulo `BlockDataLogger` escribe un CSV continuo con datos de cada bloque (altura, timestamp, dificultad, número de TXs, tamaño). Estos datos se usarán para entrenar futuras versiones del modelo con datos reales de la red en producción.

> **Para los técnicos:** LMDB con 4 DBIs: `stats` (PersistedStatistics: 8 campos), `meta` (metadatos), `blocks` (PersistedBlockRecord: 7 campos — height, timestamp, solve_time, difficulty, previous_difficulty, lwma_prediction_error, anomaly_flags), `audit` (timestamp → evento + detalles). Auto-growth de 512 MB. Modo dual pre/post v18 (NINA_V18_ONCHAIN_HEIGHT = 20000).

---

## 10. Cuando NINA necesita ayuda humana

NINA no es un dictador — es un asesor. Cuando encuentra situaciones que exceden su confianza, lo escala a un humano:

### Niveles de escalamiento

| Nivel | Significado | Acción |
|-------|-------------|--------|
| **AUTO** | NINA está segura | Actúa automáticamente |
| **WARN** | Algo inusual | Actúa pero notifica al operador |
| **HUMAN_REQUIRED** | Situación desconocida o crítica | Espera aprobación humana antes de actuar |

### Qué incluye cada escalamiento

Cuando NINA te pide ayuda, te da toda la información necesaria para decidir:

- **Descripción** del problema en lenguaje claro
- **Opciones** disponibles (qué puedes hacer)
- **Recomendación** de NINA (qué haría ella)
- **Evidencia** (los datos que respaldan su análisis)
- **Urgencia** (BAJA / MEDIA / ALTA / CRÍTICA)

### Gobernanza: NINA propone, tú decides

Para cambios importantes en parámetros de red, NINA genera propuestas formales con justificación, impacto esperado y nivel de confianza. Cada sugerencia tiene un flag `is_approved_by_human` que **debe ser verdadero** antes de ejecutarse.

Los 10 parámetros que NINA puede proponer ajustar incluyen: porcentaje de quórum, tiempo objetivo de bloque, multiplicador de dificultad, tamaño del pool de transacciones, límite de tamaño de bloque y timeout de pares, entre otros.

### Explicabilidad total

Cada decisión que toma NINA queda registrada con:
- **Por qué**: El razonamiento detrás de la decisión
- **Con qué datos**: Las métricas y evidencia que usó
- **Qué restricciones aplicó**: Qué principios constitucionales verificó
- **Resultado**: Qué acción tomó finalmente

Ninguna decisión de NINA es una "caja negra" — todo es trazable y auditable.

---

## 11. Se actualiza sola

### Monitorización de GitHub

NINA comprueba el repositorio de GitHub cada **6 horas** (cada 30 minutos si detecta urgencia) buscando:
- Hard forks (actualizaciones obligatorias)
- Parches de seguridad
- Cambios de configuración
- Nuevas funcionalidades

### Decisiones inteligentes

| Situación | Qué hace |
|-----------|----------|
| Hard fork inminente | Actualizar ahora |
| Parche de seguridad crítico | Actualizar ahora |
| Hard fork lejano | Actualizar antes del fork (margen de 500 bloques) |
| Mejora no crítica | Opcional (tú decides) |
| Pre-release | Omitir |
| Nodo sincronizando | Diferir (esperar a que termine) |

El sistema puede compilar, instalar y reiniciar el daemon sin intervención humana cuando es necesario.

> **Para los técnicos:** Intervalos: normal 21.600s (6h), urgente 1.800s (30min), startup delay 180s, max sync wait 3.600s. Pre-fork margin 500 bloques. 7 tipos de cambio con severidad 1-5: HARD_FORK (5), SECURITY_FIX (4-5), SOFT_FORK (3), PERFORMANCE (2), FEATURE (2), CONFIG_CHANGE (1). Fuentes: GitHub Compare API, cryptonote_config.h, hardforks.cpp.

---

## 12. Arquitectura completa (para los más técnicos)

### Diagrama de módulos

```
ninacatcoind (binario único)
    │
    ├─ Blockchain Core (validación, sync, consenso)
    ├─ Red P2P (gestión de pares, Dandelion++)
    │
    └─ NINA AI (en proceso, 6 niveles, 34 módulos)
         │
         ├─ NÚCLEO
         │   ├─ NinaLLMEngine ← libllama + libggml (enlace estático)
         │   ├─ NinaDecisionEngine (17 eventos → 19 acciones)
         │   ├─ NinaLLMBridge / NinaLLMFirewall / NinaConstitution
         │
         ├─ SEGURIDAD
         │   ├─ NinaNodeProtector (FSM 5 estados, 7 vectores ataque)
         │   ├─ NinaSybilDetector (correlación 3-componentes, umbral 0.72)
         │   ├─ NinaSpyCountermeasures (5 técnicas, 8 tipos espía)
         │   └─ NinaNetworkHealthMonitor (7 métricas, 4 niveles diag.)
         │
         ├─ GOBERNANZA
         │   ├─ NinaGovernanceEngine / NinaConsensusTuner
         │   └─ NinaParameterAdjustor (10 params) / NinaSuggestionEngine
         │
         ├─ EXPLICABILIDAD
         │   └─ NinaExplanationEngine (razonamiento + evidencia + auditoría)
         │
         ├─ APRENDIZAJE
         │   └─ NinaAdaptiveLearning / NinaLearningModule / NinaMemorySystem
         │
         ├─ COLABORACIÓN
         │   └─ NinaHumanCollaboration (escalamiento con callbacks)
         │
         ├─ PRIVACIDAD
         │   ├─ NinaRingController (4 niveles: 16/21/26/31)
         │   └─ NinaRingEnhancer (ASM 6 cubetas + RQS 6 componentes)
         │
         ├─ MEMPOOL: NinaSmartMempool (4 categorías, never-reject)
         ├─ RED: NinaNetworkOptimizer (calidad de par, auto-ajuste)
         ├─ PERSISTENCIA: 4 módulos (LMDB dual-mode, 512 MB, ~6 MB/año)
         ├─ ORQUESTACIÓN: 4 módulos (coordinación global)
         └─ AUTO-UPDATE: NinaIAAutoUpdate (GitHub cada 6h)
```

### Pipeline de inferencia completo

```
Evento de red (bloque, par, TX, etc.)
    │
    ▼
NinaDecisionEngine::evaluate()
    ├─ Evento frecuente (bloque/TX) → ML embebido (microsegundos)
    └─ Evento raro/importante → LLM:
         │
         ▼
    NinaLLMFirewall::validate_input(prompt)
         │ [bloqueado → safe_default]
         ▼
    llama_tokenize() → tokens (máx 2048)
         │
         ▼
    llama_memory_clear() + llama_decode() → procesar
         │
         ▼
    llama_sampler_sample() × N → generar respuesta
         │
         ▼
    NinaLLMFirewall::validate_output(response)
         │ [bloqueado → safe_default]
         ▼
    NinaConstitution::validateAgainstConstitution()
         │ [violación → rechazar]
         ▼
    Parsear respuesta → vector<NinaAction>
         │
         ▼
    ActionDispatcher → ejecutar vía P2P
```

### Constantes clave del código

```cpp
// Constitución (nina_constitution.hpp)
static constexpr bool CAN_CREATE_TRANSACTIONS  = false;  // 14 constantes, todas false
// ... (12 más)

// LLM Engine (nina_llm_engine.hpp)
n_ctx = 2048;        n_threads = 4;       n_batch = 512;
temp = 0.3;          top_k = 40;          top_p = 0.9;
seed = 42;           max_tokens = 256;    n_gpu_layers = 0;

// Ring Controller (nina_ring_controller.hpp)
NORMAL = 16;         ELEVATED = 21;       HIGH = 26;        CRITICAL = 31;
hysteresis_up = 10;  hysteresis_down = 100;  grace = 50;

// Sybil Detector (nina_sybil_detector.hpp)  
correlation_threshold = 0.72;  latency_weight = 0.35;
timing_weight = 0.40;         pattern_weight = 0.25;

// Persistent Memory (nina_persistent_memory.hpp)
initial_map_size = 512 MB;    growth = 512 MB;
v18_height = 20000;           DBIs = 4;

// Event System V2 (cryptonote_config.h)
multi_hash_depth = 10;        spike_threshold = 1.5x;
spike_freeze = 5-30 blocks;   event_v2_height = 20000;
```

### CMake

```cmake
target_link_libraries(daemon PRIVATE
    ninacatcoin_ai  llama  ggml  CURL::libcurl  ${OPENSSL_LIBRARIES})
target_include_directories(daemon PRIVATE
    ${CMAKE_SOURCE_DIR}/external/llama.cpp/include)
```

---

## 13. Rendimiento y requisitos

### ¿Cuánto consume?

| Recurso | Con NINA activa | Sin NINA (DISABLED) |
|---------|-----------------|---------------------|
| RAM extra | ~2.5 GB | 0 MB |
| Disco extra | ~2.4 GB (modelo + BD) | 0 MB |
| CPU | 4 hilos para inferencia (solo cuando se necesita) | 0 |

### Tiempos de respuesta

| Tipo de tarea | Tiempo | Frecuencia |
|---------------|--------|------------|
| Decisión de consenso | 2-5 segundos | Solo para eventos raros (nuevo par, anomalía, etc.) |
| Análisis informativo | 10-30 segundos | Bajo demanda o periódico |
| Evaluación ML embebida | Microsegundos | Para cada bloque y transacción |

### Tres modos para todo tipo de hardware

| Modo | RAM extra | Ideal para |
|------|-----------|------------|
| **ACTIVE** | ~2.5 GB | Servidores, nodos dedicados (modelo siempre en RAM) |
| **LAZY** | 0 MB (carga cuando necesita) | PCs de escritorio (libera RAM tras 30s) |
| **DISABLED** | 0 MB | Hardware muy limitado (solo ML estadístico) |

### Desglose de disco

| Componente | Tamaño |
|------------|--------|
| Modelo GGUF (nina_model.gguf) | 1.88 GB |
| Base de datos LMDB | 512 MB inicial (~6 MB/año) |
| CSV de entrenamiento | ~1 MB/año |
| **Total** | **~2.4 GB** |

---

## 14. Preguntas frecuentes

**¿NINA puede censurar mis transacciones?**
No. La Constitución incluye `NO_CENSORSHIP`. Se verifica en compilación y en ejecución.

**¿Qué pasa si no tengo 2.5 GB de RAM extra?**
Usa modo LAZY (carga solo cuando necesita) o DISABLED (ML estadístico, sin LLM).

**¿NINA necesita Internet para funcionar?**
Solo la primera vez, para descargar el modelo (~1.88 GB). Después, todo es local.

**¿Puede NINA alterar el consenso?**
Solo influye en el tamaño de anillos, y lo hace de forma determinista (misma decisión en todos los nodos con el mismo modelo).

**¿Qué pasa si mi modelo es diferente al de otros nodos?**
Se verifica por SHA-256 al inicio. Además, cada coinbase incluye el hash del modelo (tag `0xCA`) para verificación en red.

**¿Alguien puede hackear NINA para robar fondos?**
No. No existe código que conecte NINA con funciones de billetera. Es como hackear un termómetro para que abra una puerta: no hay cable. Además: firewall de 75+ patrones, Constitución con 14 prohibiciones, 6 static_assert.

**¿NINA toma decisiones sin preguntar?**
Para seguridad rutinaria (monitorizar, ajustar anillos) sí. Para cambios importantes, genera propuestas con aprobación humana. Para situaciones desconocidas, escala al operador y espera.

**¿Puedo auditar las decisiones de NINA?**
Sí. Cada decisión queda en la pista de auditoría con razonamiento, evidencia y resultado.

**¿NINA funciona en todas las plataformas?**
Sí — Linux, Windows y macOS. La inferencia es solo CPU, no necesitas GPU.

**¿La IA aprende cosas nuevas mientras el nodo funciona?**
NINA mantiene estadísticas y perfiles actualizados, pero los pesos del modelo no cambian en ejecución. Nuevas versiones del modelo se entrenarán con datos acumulados.

---

## 15. Apéndice técnico: fórmulas y constantes

### Correlación Sybil

$$correlación = 0.35 \cdot \max(0, 1 - \frac{|\bar{l_a} - \bar{l_b}|}{300}) + 0.40 \cdot \max(0, 1 - \frac{\bar{\sigma}}{200}) + 0.25 \cdot sim\_tx$$

Umbral: 0.72 · Mín. observaciones: 5 · Limpieza: 86.400s

### Nivel de amenaza

$$amenaza = \max\left(\min(1, \frac{o-6}{44}),\ \min(1, \frac{r-0.05}{0.25}),\ \min(1, \frac{v-2}{8}),\ s,\ \min(1, \frac{h-20}{60})\right)$$

Donde: $o$ = outputs/tx, $r$ = spend_ratio, $v$ = volume_ramp, $s$ = sybil_score, $h$ = hashrate_drop%

### Calidad de anillo (RQS)

$$RQS = 0.25 \cdot edad + 0.15 \cdot monto + 0.20 \cdot veneno + 0.15 \cdot temporal + 0.10 \cdot indep + 0.15 \cdot hist$$

Mínimo: 0.85 · Umbral de veneno: 0.7

### Modelo de gasto adaptativo (ASM)

| Cubeta | Edad (bloques) | Peso |
|--------|---------------|------|
| 0 | < 100 | 0.35 |
| 1 | < 1.000 | 0.25 |
| 2 | < 5.000 | 0.18 |
| 3 | < 20.000 | 0.12 |
| 4 | < 100.000 | 0.07 |
| 5 | ≥ 100.000 | 0.03 |

### Confianza de espía

$$confianza = 0.7 \times evidencia\_nueva + 0.3 \times histórica$$

### Constantes LLM

n_ctx=2048 · n_batch=512 · temp=0.3/0.0 · top_k=40 · top_p=0.9 · seed=42 · max_tokens=256 · n_gpu_layers=0

### Constantes de anillo

NORMAL=16 · ELEVATED=21 · HIGH=26 · CRITICAL=31 · hysteresis_up=10 · hysteresis_down=100 · grace=50

### Event System V2

multi_hash_depth=10 · spike_threshold=1.5x · spike_freeze=5-30 bloques · v2_height=20.000

---

## 16. Referencias

1. Cao, T., et al. *"Friend or Foe? Identifying Spy Nodes in the Monero Peer-to-Peer Network."* 2025.
2. Shi, Y., et al. *"Eclipse Attacks on Cryptocurrency P2P Networks."* NDSS 2025.
3. Meta AI. *"LLaMA: Open and Efficient Foundation Language Models."* 2024.
4. Hu, E., et al. *"LoRA: Low-Rank Adaptation of Large Language Models."* ICLR 2022.
5. Dettmers, T., et al. *"QLoRA: Efficient Finetuning of Quantized LLMs."* NeurIPS 2023.
6. llama.cpp — https://github.com/ggml-org/llama.cpp
7. NinaCatCoin — https://github.com/ninacatcoin
8. CryptoNote — https://cryptonote.org
9. RandomX — https://github.com/tevador/RandomX

---

*"NINA observa, analiza y protege. Pero nunca, bajo ninguna circunstancia, toca tu dinero."*

---

**Versión:** 2.0 | **Fecha:** Marzo 2026 | **Licencia:** CC BY-SA 4.0
