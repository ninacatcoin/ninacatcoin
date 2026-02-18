// ================================================================
// NINACATCOIN IA INTEGRATION REFERENCE GUIDE
// ================================================================
//
// Este archivo contiene ejemplos de código para integrar la IA
// en diferentes partes del daemon de ninacatcoin.
//
// ================================================================

// ================================================================
// 1. INTEGRACIÓN EN P2P NODE (p2p/net_node.h)
// ================================================================

// En el archivo: src/p2p/net_node.h
// Agregar después de los otros includes:

#include "daemon/ia_peer_monitoring.h"

// En la clase t_node, agregar estos métodos:

class t_node {
private:
  // ... existing code ...
  
public:
  // Nueva función para validar peers con IA
  bool validate_peer_with_ia(const peer_id_t& peer_id, const std::string& peer_address) {
    return daemonize::IAPeerMonitoring::on_peer_connected(peer_id, peer_address);
  }
};

// ================================================================
// 2. INTEGRACIÓN EN NET_NODE.CPP (p2p/net_node.cpp)
// ================================================================

// En src/p2p/net_node.cpp, buscar la función que maneja conexiones:
// (Generalmente algo como: on_connection_new() o handle_incoming_connections())

// ANTES:
/*
void t_node::on_peer_connect(const peer_id_t& peer_id, ...) {
  // Aceptar peer directamente
  accept_peer(peer_id);
}
*/

// DESPUÉS (con IA):
void t_node::on_peer_connect(const peer_id_t& peer_id, const std::string& peer_address) {
  LOG_PRINT_L2("Peer " << peer_id << " attempting to connect from " << peer_address);
  
  // NUEVO: Validar con IA primero
  if (!daemonize::IAPeerMonitoring::on_peer_connected(peer_id, peer_address)) {
    LOG_PRINT_L1("Peer " << peer_id << " REJECTED by IA security module");
    reject_connection(peer_id);
    return;
  }
  
  LOG_PRINT_L2("Peer " << peer_id << " ACCEPTED by IA security module");
  accept_peer(peer_id);
}

// Para desconexiones:
void t_node::on_peer_disconnect(const peer_id_t& peer_id) {
  LOG_PRINT_L2("Peer " << peer_id << " disconnecting");
  
  // Notificar a IA
  daemonize::IAPeerMonitoring::on_peer_disconnected(peer_id);
  
  // Proceder con desconexión normal
  close_connection(peer_id);
}

// ================================================================
// 3. INTEGRACIÓN EN CRYPTONOTE PROTOCOL
// ================================================================

// En src/cryptonote_protocol/cryptonote_protocol_handler.h
// Agregar include:

#include "daemon/ia_peer_monitoring.h"

// En src/cryptonote_protocol/cryptonote_protocol_handler.cpp,
// buscar donde se procesan transacciones:

// ANTES:
/*
void handle_transaction_message(const NOTIFY_NEW_TRANSACTIONS::request& r, peer_connection& peer) {
  for (const auto& tx : r.txs) {
    process_transaction(tx);
  }
}
*/

// DESPUÉS (con IA):
void handle_transaction_message(const NOTIFY_NEW_TRANSACTIONS::request& r, peer_connection& peer) {
  for (const auto& tx : r.txs) {
    // NUEVO: Analizar transacción con IA
    std::string tx_id = tools::get_transaction_hash(tx);
    
    if (!daemonize::IAPeerMonitoring::on_transaction_received(tx_id, tx.size())) {
      LOG_PRINT_L1("Transaction " << tx_id << " REJECTED by IA analysis");
      d_req_drop(peer);
      return;
    }
    
    LOG_PRINT_L2("Transaction " << tx_id << " APPROVED by IA, processing normally");
    process_transaction(tx);
  }
}

// ================================================================
// 4. INTEGRACIÓN EN HEARTBEAT DE RED
// ================================================================

// En src/p2p/net_node.cpp, buscar donde está el heartbeat/tick:
// (Generalmente en una función como: run_network_tick() o on_heartbeat())

// ANTES:
/*
void t_node::tick() {
  update_peers();
  perform_cleanup();
}
*/

// DESPUÉS (con IA):
void t_node::tick() {
  update_peers();
  perform_cleanup();
  
  // NUEVO: Llamar al heartbeat de IA cada 30-60 segundos
  static uint64_t last_ia_heartbeat = 0;
  uint64_t now = time(nullptr);
  if (now - last_ia_heartbeat >= 60) {
    daemonize::IAPeerMonitoring::on_network_heartbeat();
    last_ia_heartbeat = now;
  }
}

// ================================================================
// 5. INTEGRACIÓN EN RPC SERVER
// ================================================================

// En src/rpc/core_rpc_server.h, agregar nuevo método de RPC:

#include "daemon/ai_integration.h"

class core_rpc_server {
private:
  // ... existing code ...
  
  // NUEVO: Endpoint RPC para status de IA
  bool on_get_ia_status(const EMPTY_STRUCT& req, GET_IA_STATUS::response& res, connection_context& cctx) {
    res.status = daemonize::IAModuleIntegration::get_ia_status();
    return true;
  }
};

// En src/rpc/core_rpc_server.cpp, registrar el nuevo método:

void core_rpc_server::init_http_bindings() {
  // ... existing bindings ...
  
  // NUEVO: Registrar endpoint GET_IA_STATUS
  HTTP_HANDLER("get_ia_status", on_get_ia_status);
}

// En el archivo de definiciones de RPC (generalmente rpc_types.h o similar):

struct GET_IA_STATUS {
  struct request {
    // Empty request
  };
  
  struct response {
    std::string status;  // JSON string con status de IA
    std::string status_string() const { return status; }
  };
};

// ================================================================
// 6. INTEGRACIÓN EN COMMAND LINE INTERFACE
// ================================================================

// En src/daemon/command_line_args.h, agregar nuevo argumento:

namespace daemon_args {
  struct arg_ia_enabled_t {
    const char *name = "enable-ia";
    const char *description = "Enable IA Security Module (default: true)";
    const bool default_value = true;
  };
  constexpr static arg_ia_enabled_t arg_ia_enabled{};
}

// ================================================================
// 7. MANEJO DE ERRORES DE IA
// ================================================================

// Si la IA falla durante runtime (no en startup):

try {
  // ... código normal ...
  
  if (!daemonize::IAPeerMonitoring::on_peer_connected(peer_id, address)) {
    // Manejo de fallo de validación
    MERROR("IA peer validation FAILED for peer " << peer_id);
    reject_peer();
  }
}
catch (const std::exception& e) {
  // Manejar excepciones de IA
  MERROR("Exception in IA integration: " << e.what());
  
  // Continuar sin IA si hay error (fallback seguro)
  // Pero registrar en logs para auditoría
  LOG_ERROR("IA module temporary unavailable, continuing without IA validation");
}

// ================================================================
// 8. LOGGING DE OPERACIONES DE IA
// ================================================================

// Para mejor debugging, usar estos niveles de log:

// Errores críticos:
MERROR("[IA] Critical error: " << error_msg);

// Advertencias:
MWARNING("[IA] Warning: " << warning_msg);

// Información general:
MINFO("[IA] Operation successful: " << info_msg);

// Debug detallado:
MDEBUG("[IA] Detailed debug: " << debug_msg);

// Trace (mas detallado):
MTRACE("[IA] Trace: " << trace_msg);

// ================================================================
// 9. TESTING DE INTEGRACIÓN IA
// ================================================================

// Test básico en tests/unit_tests/

#include "daemon/ai_integration.h"
#include "daemon/ia_peer_monitoring.h"

TEST(IAIntegration, peer_validation) {
  // Test que un peer válido sea aceptado
  EXPECT_TRUE(
    daemonize::IAPeerMonitoring::on_peer_connected("peer_test_1", "127.0.0.1")
  );
  
  // Test que un peer en blacklist sea rechazado
  // (después de agregarlo a blacklist)
  EXPECT_FALSE(
    daemonize::IAPeerMonitoring::on_peer_connected("peer_blacklisted", "192.168.1.1")
  );
}

TEST(IAIntegration, transaction_analysis) {
  // Test que una transacción válida sea aprobada
  EXPECT_TRUE(
    daemonize::IAPeerMonitoring::on_transaction_received("tx_test_1", 512)
  );
  
  // Test de transacción grande/anómala
  EXPECT_TRUE(
    daemonize::IAPeerMonitoring::on_transaction_received("tx_anomaly", 1000000)
  );
}

// ================================================================
// CHECKLIST DE INTEGRACIÓN
// ================================================================

/*
☐ Agregar #include "daemon/ia_peer_monitoring.h" en archivos relevantes
☐ Integrar on_peer_connected() en p2p/net_node.cpp
☐ Integrar on_peer_disconnected() en p2p/net_node.cpp
☐ Integrar on_transaction_received() en cryptonote_protocol_handler.cpp
☐ Integrar on_network_heartbeat() en el tick loop
☐ Agregar RPC endpoint para get_ia_status()
☐ Agregar command line argument --enable-ia
☐ Crear tests unitarios para integración de IA
☐ Actualizar documentación de usuario
☐ Compilar y verificar no hay errores
☐ Ejecutar tests de integración
☐ Hacer load testing para verificar rendimiento
☐ Documentar cualquier problema encontrado
☐ Someter a revisión de seguridad
*/

// ================================================================
// NOTAS IMPORTANTES
// ================================================================

/*
1. La IA ya está inicializada en daemon.cpp antes de t_internals
2. Estos ejemplos asumen que IAModule::getInstance() está disponible
3. Todos los métodos son thread-safe
4. Los ErroresOS no deben detener el daemon (fallback seguro)
5. Logging es crítico para auditoría y debugging
6. Cada integración debe ser testeada de forma independiente
7. La IA no bloquea operaciones críticas del daemon
8. Performance es importante - mantén integración ligera
9. Documentar cambios para próximas personas que mantengan código
10. Considerar retrocompatibilidad si este es código crítico
*/

// ================================================================
// FIN DE REFERENCIA DE INTEGRACIÓN
// ================================================================
