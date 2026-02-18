/**
 * @file security_query_tool.hpp
 * @brief Herramienta para gestionar queries de seguridad P2P
 * 
 * Proporciona:
 * - Serialización/Deserialización de SecurityQuery
 * - Validación de firmas digitales
 * - Cálculo de consenso
 * - Gestión de timeouts
 */

#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <memory>
#include <map>
#include "crypto/crypto.h"

namespace cryptonote {
namespace security {

/**
 * @brief Query enviado a otros nodos para verificar problema
 */
struct SecurityQuery {
  std::string query_id;              ///< ID único del query (UUID)
  uint64_t height;                   ///< Altura del checkpoint
  std::string expected_hash;         ///< Hash que esperamos
  std::string reported_hash;         ///< Hash incorrecto que recibimos
  std::string source;                ///< De dónde vino (URL, IP, seed name)
  std::string attack_type;           ///< "invalid_format", "replay", "seed_mismatch"
  std::string reporting_node_id;     ///< ID del nodo que pregunta
  std::time_t timestamp;             ///< Cuándo lo detectamos
  uint64_t timeout_seconds = 10;     ///< Cuánto esperar respuestas
  
  // Para serialización
  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(query_id)
    KV_SERIALIZE(height)
    KV_SERIALIZE(expected_hash)
    KV_SERIALIZE(reported_hash)
    KV_SERIALIZE(source)
    KV_SERIALIZE(attack_type)
    KV_SERIALIZE(reporting_node_id)
    KV_SERIALIZE(timestamp)
    KV_SERIALIZE(timeout_seconds)
  END_KV_SERIALIZE_MAP()
};

/**
 * @brief Respuesta de un nodo a un SecurityQuery
 */
struct SecurityResponse {
  std::string query_id;              ///< ID del query original
  std::string node_id;               ///< ID del nodo que responde (alias: responder_node_id)
  uint64_t height = 0;               ///< Altura del checkpoint consultado
  bool matches_local = false;        ///< ¿El hash del respondedor coincide con el nuestro?
  std::string height_hash;           ///< Hash que reporta el respondedor
  bool also_detected = false;        ///< ¿Ese nodo también ve el problema?
  std::string responder_status;      ///< "healthy", "paused", "error"
  std::time_t timestamp = 0;         ///< Cuándo respondió
  crypto::signature response_sig;    ///< Firma de la respuesta
  
  // Para serialización
  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(query_id)
    KV_SERIALIZE(node_id)
    KV_SERIALIZE(height)
    KV_SERIALIZE(matches_local)
    KV_SERIALIZE(height_hash)
    KV_SERIALIZE(also_detected)
    KV_SERIALIZE(responder_status)
    KV_SERIALIZE(timestamp)
  END_KV_SERIALIZE_MAP()
};

/**
 * @brief Resultado del análisis de consenso
 */
struct ConsensusResult {
  bool is_confirmed = false;         ///< ¿Problema confirmado por consenso?
  bool is_local = false;             ///< ¿Es ataque local (solo en este nodo)?
  
  uint64_t confirmations = 0;        ///< Cuántos nodos confirmaron
  uint64_t denials = 0;              ///< Cuántos dijeron que no
  uint64_t errors = 0;               ///< Cuántos no respondieron
  
  float consensus_percentage = 0.0f; ///< Porcentaje de confirmación
  float confidence = 0.0f;           ///< Confianza en el resultado (0-1)
  
  std::vector<std::string> confirming_nodes;  ///< Nodos que confirmaron
  std::vector<std::string> denying_nodes;     ///< Nodos que negaron
  std::vector<std::string> error_nodes;       ///< Nodos que no respondieron
};

/**
 * @brief Herramienta para gestionar queries de seguridad
 */
class SecurityQueryTool {
public:
  /**
   * @brief Generar ID único para un query
   */
  static std::string generate_query_id();
  
  /**
   * @brief Serializar query a JSON
   */
  static std::string serialize_query(const SecurityQuery& query);
  
  /**
   * @brief Deserializar query desde JSON
   */
  static bool deserialize_query(const std::string& json, SecurityQuery& out_query);
  
  /**
   * @brief Serializar respuesta a JSON
   */
  static std::string serialize_response(const SecurityResponse& response);
  
  /**
   * @brief Deserializar respuesta desde JSON
   */
  static bool deserialize_response(const std::string& json, SecurityResponse& out_response);
  
  /**
   * @brief Firmar un query
   */
  static crypto::signature sign_query(const SecurityQuery& query, 
                                     const crypto::secret_key& private_key);
  
  /**
   * @brief Verificar firma de un query
   */
  static bool verify_query_signature(const SecurityQuery& query,
                                    const crypto::signature& sig,
                                    const crypto::public_key& public_key);
  
  /**
   * @brief Analizar respuestas y calcular consenso
   */
  static ConsensusResult calculate_consensus(
    const std::vector<SecurityResponse>& responses,
    float threshold_percentage = 0.66f,
    uint64_t min_confirmations = 2
  );
  
  /**
   * @brief Verificar si query ha expirado
   */
  static bool is_query_expired(const SecurityQuery& query);
  
  /**
   * @brief Obtener descripción de tipo de ataque
   */
  static std::string get_attack_type_description(const std::string& attack_type);
};

/**
 * @brief Manager de queries en progreso
 */
class QueryManager {
private:
  std::map<std::string, SecurityQuery> m_pending_queries;
  std::map<std::string, std::vector<SecurityResponse>> m_responses;
  std::map<std::string, std::time_t> m_query_start_times;
  
public:
  /**
   * @brief Agregar query a seguimiento
   */
  bool add_query(const SecurityQuery& query);
  
  /**
   * @brief Agregar respuesta a un query
   */
  bool add_response(const SecurityResponse& response);
  
  /**
   * @brief Obtener todas las respuestas de un query
   */
  std::vector<SecurityResponse> get_responses(const std::string& query_id) const;
  
  /**
   * @brief Verificar si query está completo (todas las respuestas recibidas)
   */
  bool is_complete(const std::string& query_id) const;
  
  /**
   * @brief Limpiar queries expirados
   */
  void cleanup_expired_queries();
  
  /**
   * @brief Obtener consenso final de un query
   */
  ConsensusResult get_consensus(const std::string& query_id) const;
  
  /**
   * @brief Contar queries pendientes
   */
  size_t pending_count() const { return m_pending_queries.size(); }
};

} // namespace security
} // namespace cryptonote
