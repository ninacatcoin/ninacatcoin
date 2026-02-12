/**
 * @file reputation_manager.hpp
 * @brief Gestor de reputación de nodos P2P
 * 
 * Proporciona:
 * - Cálculo y almacenamiento de puntuación de reputación
 * - Persistencia en disco
 * - Decay temporal (olvido de reportes antiguos)
 * - Estadísticas y análisis
 */

#pragma once

#include <string>
#include <map>
#include <vector>
#include <ctime>
#include <memory>
#include "crypto/crypto.h"

namespace cryptonote {
namespace security {

/**
 * @brief Registro de reputación de un nodo
 */
struct NodeReputation {
  std::string node_id;               ///< Identificador único del nodo
  float score = 0.5f;                ///< Score de reputación (0.0 a 1.0)
  
  uint64_t total_reports = 0;        ///< Reportes totales enviados
  uint64_t confirmed_reports = 0;    ///< Reportes confirmados por consenso
  uint64_t false_reports = 0;        ///< Reportes rechazados
  uint64_t pending_reports = 0;      ///< Reportes en evaluación
  
  std::time_t last_updated = 0;      ///< Última actualización
  std::time_t last_confirmed = 0;    ///< Último reporte confirmado
  std::time_t creation_time = 0;     ///< Cuándo se creó este registro
  
  bool is_banned = false;            ///< ¿Nodo bannado?
  std::string ban_reason = "";       ///< Razón del ban
  
  // Para serialización JSON
  BEGIN_KV_SERIALIZE_MAP()
    KV_SERIALIZE(node_id)
    KV_SERIALIZE(score)
    KV_SERIALIZE(total_reports)
    KV_SERIALIZE(confirmed_reports)
    KV_SERIALIZE(false_reports)
    KV_SERIALIZE(pending_reports)
    KV_SERIALIZE(last_updated)
    KV_SERIALIZE(last_confirmed)
    KV_SERIALIZE(creation_time)
    KV_SERIALIZE(is_banned)
    KV_SERIALIZE(ban_reason)
  END_KV_SERIALIZE_MAP()
  
  /**
   * @brief Calcular score de reputación
   * 
   * Fórmula: (confirmed / total) × 0.9 + 0.1
   * 
   * Esto asegura que:
   * - Nodos nuevos comienzan con 0.5
   * - Máximo posible es 1.0
   * - Mínimo posible es 0.1 (nunca 0)
   */
  float calculate_score() {
    if (total_reports == 0) return 0.5f;
    float ratio = (float)confirmed_reports / total_reports;
    return ratio * 0.9f + 0.1f;
  }
  
  /**
   * @brief ¿Es este nodo confiable?
   */
  bool is_trusted(float threshold = 0.40f) const {
    return !is_banned && score >= threshold;
  }
  
  /**
   * @brief ¿Cuánto tiempo sin actividad?
   */
  uint64_t inactive_seconds() const {
    return std::time(nullptr) - last_updated;
  }
};

/**
 * @brief Estadísticas globales de reputación
 */
struct ReputationStats {
  uint64_t total_nodes = 0;
  uint64_t trusted_nodes = 0;       // Score >= threshold
  uint64_t suspicious_nodes = 0;    // Score < threshold
  uint64_t banned_nodes = 0;
  
  float average_score = 0.0f;
  float median_score = 0.0f;
  float min_score = 1.0f;
  float max_score = 0.0f;
  
  uint64_t total_confirmed_reports = 0;
  uint64_t total_false_reports = 0;
  float global_accuracy = 0.0f;      // Qué % de reportes son confirmados
};

/**
 * @brief Gestor central de reputación de nodos
 */
class ReputationManager {
private:
  std::map<std::string, NodeReputation> m_reputations;
  std::string m_data_dir;
  
  // Configuración
  float m_reputation_threshold = 0.40f;      // Score mínimo para confiar
  float m_confirmation_reward = 0.05f;       // Cuánto suma un reporte confirmado
  float m_false_penalty = 0.05f;             // Cuánto resta un reporte falso
  uint64_t m_decay_days = 30;                // Olvido de reportes antiguos
  
public:
  ReputationManager(const std::string& data_dir);
  
  /**
   * @brief Cargar reputaciones desde disco
   */
  bool load_from_disk();
  
  /**
   * @brief Guardar reputaciones en disco
   */
  bool save_to_disk();
  
  /**
   * @brief Actualizar reputación cuando reporte es confirmado
   */
  void on_report_confirmed(const std::string& node_id);
  
  /**
   * @brief Actualizar reputación cuando reporte es rechazado
   */
  void on_report_rejected(const std::string& node_id);
  
  /**
   * @brief Registrar nuevo reporte enviado
   */
  void on_report_sent(const std::string& node_id);
  
  /**
   * @brief Obtener reputación de un nodo
   */
  NodeReputation get_reputation(const std::string& node_id) const;
  
  /**
   * @brief Obtener score de un nodo
   */
  float get_score(const std::string& node_id) const;
  
  /**
   * @brief ¿Es nodo confiable?
   */
  bool is_trusted(const std::string& node_id) const;
  
  /**
   * @brief Banear un nodo
   */
  void ban_node(const std::string& node_id, const std::string& reason);
  
  /**
   * @brief Desbanear un nodo
   */
  void unban_node(const std::string& node_id);
  
  /**
   * @brief ¿Está bannado un nodo?
   */
  bool is_banned(const std::string& node_id) const;
  
  /**
   * @brief Obtener lista de nodos bannados
   */
  std::vector<std::string> get_banned_nodes() const;
  
  /**
   * @brief Aplicar decay temporal
   * 
   * Después de X días, los reportes antiguos pierden importancia
   */
  void apply_temporal_decay();
  
  /**
   * @brief Obtener estadísticas globales
   */
  ReputationStats get_statistics() const;
  
  /**
   * @brief Listar nodos ordenados por reputación
   */
  std::vector<NodeReputation> get_ranked_nodes(bool only_trusted = false) const;
  
  /**
   * @brief Obtener reporte HTML de reputaciones
   */
  std::string generate_reputation_report() const;
  
  /**
   * @brief Limpiar nodos inactivos
   */
  void cleanup_inactive_nodes(uint64_t inactive_threshold_seconds);
  
  /**
   * @brief Resetear estadísticas de un nodo
   */
  void reset_node_statistics(const std::string& node_id);
  
  /**
   * @brief Contar nodos confiables
   */
  size_t trusted_nodes_count() const;
  
  /**
   * @brief Contar nodos totales
   */
  size_t total_nodes_count() const { return m_reputations.size(); }
  
  // Configuradores
  void set_reputation_threshold(float threshold) { m_reputation_threshold = threshold; }
  void set_confirmation_reward(float reward) { m_confirmation_reward = reward; }
  void set_false_penalty(float penalty) { m_false_penalty = penalty; }
  void set_decay_days(uint64_t days) { m_decay_days = days; }
};

/**
 * @brief Helper para generar salida de reputación
 */
class ReputationPrinter {
public:
  /**
   * @brief Imprimir tabla de reputaciones en LOG
   */
  static void print_reputation_table(const ReputationManager& manager);
  
  /**
   * @brief Imprimir estadísticas en LOG
   */
  static void print_statistics(const ReputationManager& manager);
  
  /**
   * @brief Generar JSON con reputaciones
   */
  static std::string to_json(const ReputationManager& manager);
};

} // namespace security
} // namespace cryptonote
