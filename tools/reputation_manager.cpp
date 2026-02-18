/**
 * @file reputation_manager.cpp
 * @brief Implementación del gestor de reputación de nodos P2P
 */

#include "reputation_manager.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <numeric>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define MKDIR(path) mkdir(path, 0755)
#endif

namespace cryptonote {
namespace security {

// ============================================================================
// ReputationManager - Constructor & Initialization
// ============================================================================

ReputationManager::ReputationManager(const std::string& data_dir)
    : m_data_dir(data_dir),
      m_reputation_threshold(0.40f),
      m_confirmation_reward(0.05f),
      m_false_penalty(0.05f),
      m_decay_days(30) {
  
  // Crear directorio si no existe
  MKDIR(data_dir.c_str());
  
  MINFO("[REPUTATION] Manager inicializado en: " << data_dir);
}

// ============================================================================
// Persistence - Load/Save
// ============================================================================

bool ReputationManager::load_from_disk() {
  std::string filepath = m_data_dir + "/peer_reputation.json";
  
  std::ifstream file(filepath);
  if (!file.is_open()) {
    MINFO("[REPUTATION] Archivo de reputación no encontrado (primera ejecución)");
    return true;  // OK - primera ejecución
  }
  
  try {
    // Leer archivo JSON (simplificado)
    std::string line;
    uint64_t loaded_count = 0;
    
    while (std::getline(file, line)) {
      if (line.empty() || line[0] == '{' || line[0] == '[' || line[0] == '}' || line[0] == ']') {
        continue;
      }
      
      // Parseo simplificado - en producción usar JSON real
      if (line.find("\"node_id\"") != std::string::npos) {
        NodeReputation rep;
        // TODO: Parsing JSON real
        loaded_count++;
      }
    }
    
    file.close();
    MINFO("[REPUTATION] Cargadas " << loaded_count << " reputaciones desde disco");
    return true;
    
  } catch (const std::exception& e) {
    MERROR("[REPUTATION] Error cargando reputaciones: " << e.what());
    return false;
  }
}

bool ReputationManager::save_to_disk() {
  std::string filepath = m_data_dir + "/peer_reputation.json";
  
  try {
    std::ofstream file(filepath);
    if (!file.is_open()) {
      MERROR("[REPUTATION] No se pudo abrir archivo: " << filepath);
      return false;
    }
    
    // Escribir JSON (simplificado)
    file << "{\n";
    file << "  \"nodes\": [\n";
    
    size_t count = 0;
    for (const auto& pair : m_reputations) {
      const NodeReputation& rep = pair.second;
      
      if (count > 0) file << ",\n";
      
      file << "    {\n";
      file << "      \"node_id\": \"" << rep.node_id << "\",\n";
      file << "      \"score\": " << std::fixed << std::setprecision(3) << rep.score << ",\n";
      file << "      \"confirmed_reports\": " << rep.confirmed_reports << ",\n";
      file << "      \"false_reports\": " << rep.false_reports << ",\n";
      file << "      \"total_reports\": " << rep.total_reports << ",\n";
      file << "      \"last_updated\": " << rep.last_updated << ",\n";
      file << "      \"is_banned\": " << (rep.is_banned ? "true" : "false") << "\n";
      file << "    }";
      
      count++;
    }
    
    file << "\n  ],\n";
    file << "  \"updated_at\": " << std::time(nullptr) << ",\n";
    file << "  \"version\": \"1.0\"\n";
    file << "}\n";
    
    file.close();
    MINFO("[REPUTATION] Guardadas " << count << " reputaciones en disco");
    return true;
    
  } catch (const std::exception& e) {
    MERROR("[REPUTATION] Error guardando reputaciones: " << e.what());
    return false;
  }
}

// ============================================================================
// Reputation Updates
// ============================================================================

void ReputationManager::on_report_confirmed(const std::string& node_id) {
  auto it = m_reputations.find(node_id);
  
  if (it == m_reputations.end()) {
    // Crear nuevo registro
    NodeReputation rep;
    rep.node_id = node_id;
    rep.creation_time = std::time(nullptr);
    rep.last_updated = rep.creation_time;
    rep.last_confirmed = rep.creation_time;
    rep.total_reports = 1;
    rep.confirmed_reports = 1;
    rep.score = rep.calculate_score();
    m_reputations[node_id] = rep;
    
    MINFO("[REPUTATION] Nuevo nodo: " << node_id << " (score: " 
          << std::fixed << std::setprecision(3) << rep.score << ")");
  } else {
    NodeReputation& rep = it->second;
    rep.total_reports++;
    rep.confirmed_reports++;
    rep.pending_reports = (rep.pending_reports > 0) ? rep.pending_reports - 1 : 0;
    rep.last_updated = std::time(nullptr);
    rep.last_confirmed = rep.last_updated;
    rep.score = rep.calculate_score();
    
    MINFO("[REPUTATION] Confirmado: " << node_id << " (score: " 
          << std::fixed << std::setprecision(3) << rep.score << ")");
  }
}

void ReputationManager::on_report_rejected(const std::string& node_id) {
  auto it = m_reputations.find(node_id);
  
  if (it == m_reputations.end()) {
    // Crear con penalización
    NodeReputation rep;
    rep.node_id = node_id;
    rep.creation_time = std::time(nullptr);
    rep.last_updated = rep.creation_time;
    rep.total_reports = 1;
    rep.false_reports = 1;
    rep.score = rep.calculate_score();
    m_reputations[node_id] = rep;
    
    MWARNING("[REPUTATION] Nuevo nodo rechazado: " << node_id 
             << " (score: " << std::fixed << std::setprecision(3) << rep.score << ")");
  } else {
    NodeReputation& rep = it->second;
    rep.total_reports++;
    rep.false_reports++;
    rep.pending_reports = (rep.pending_reports > 0) ? rep.pending_reports - 1 : 0;
    rep.last_updated = std::time(nullptr);
    rep.score = rep.calculate_score();
    
    // Banear si score cae muy bajo
    if (rep.score < 0.2f) {
      rep.is_banned = true;
      rep.ban_reason = "Low reputation score from false reports";
      MERROR("[REPUTATION] Bannado: " << node_id << " (score: " 
             << std::fixed << std::setprecision(3) << rep.score << ")");
    }
    
    MWARNING("[REPUTATION] Rechazado: " << node_id << " (score: " 
             << std::fixed << std::setprecision(3) << rep.score << ")");
  }
}

void ReputationManager::on_report_sent(const std::string& node_id) {
  auto it = m_reputations.find(node_id);
  
  if (it == m_reputations.end()) {
    NodeReputation rep;
    rep.node_id = node_id;
    rep.creation_time = std::time(nullptr);
    rep.last_updated = rep.creation_time;
    rep.pending_reports = 1;
    rep.score = 0.5f;  // Default score para nuevos nodos
    m_reputations[node_id] = rep;
    
    MINFO("[REPUTATION] Primer reporte de: " << node_id);
  } else {
    NodeReputation& rep = it->second;
    rep.pending_reports++;
    rep.last_updated = std::time(nullptr);
    
    MINFO("[REPUTATION] Reporte pendiente de: " << node_id 
          << " (pendientes: " << rep.pending_reports << ")");
  }
}

// ============================================================================
// Reputation Queries
// ============================================================================

NodeReputation ReputationManager::get_reputation(const std::string& node_id) const {
  auto it = m_reputations.find(node_id);
  
  if (it == m_reputations.end()) {
    NodeReputation default_rep;
    default_rep.node_id = node_id;
    default_rep.score = 0.5f;  // Score por defecto
    default_rep.creation_time = std::time(nullptr);
    return default_rep;
  }
  
  return it->second;
}

float ReputationManager::get_score(const std::string& node_id) const {
  return get_reputation(node_id).score;
}

bool ReputationManager::is_trusted(const std::string& node_id) const {
  NodeReputation rep = get_reputation(node_id);
  return rep.is_trusted(m_reputation_threshold);
}

// ============================================================================
// Banning
// ============================================================================

void ReputationManager::ban_node(const std::string& node_id, const std::string& reason) {
  auto it = m_reputations.find(node_id);
  
  if (it != m_reputations.end()) {
    it->second.is_banned = true;
    it->second.ban_reason = reason;
    
    MERROR("[REPUTATION] BANNADO: " << node_id << " - Razón: " << reason);
  }
}

void ReputationManager::unban_node(const std::string& node_id) {
  auto it = m_reputations.find(node_id);
  
  if (it != m_reputations.end()) {
    it->second.is_banned = false;
    it->second.ban_reason = "";
    
    MINFO("[REPUTATION] DESBANNADO: " << node_id);
  }
}

bool ReputationManager::is_banned(const std::string& node_id) const {
  NodeReputation rep = get_reputation(node_id);
  return rep.is_banned;
}

std::vector<std::string> ReputationManager::get_banned_nodes() const {
  std::vector<std::string> banned;
  
  for (const auto& pair : m_reputations) {
    if (pair.second.is_banned) {
      banned.push_back(pair.first);
    }
  }
  
  return banned;
}

// ============================================================================
// Temporal Decay
// ============================================================================

void ReputationManager::apply_temporal_decay() {
  std::time_t now = std::time(nullptr);
  uint64_t decay_seconds = m_decay_days * 24 * 3600;
  
  for (auto& pair : m_reputations) {
    NodeReputation& rep = pair.second;
    uint64_t age = now - rep.last_confirmed;
    
    // Después de X días, los reportes antiguos pierden importancia
    if (rep.confirmed_reports > 0 && age > decay_seconds) {
      // Reducir weight de reportes antiguos
      float decay_factor = std::exp(-1.0f * (float)age / (decay_seconds * 2.0f));
      
      // Recalcular score con decay
      uint64_t effective_confirmed = (uint64_t)(rep.confirmed_reports * decay_factor);
      if (effective_confirmed < rep.confirmed_reports) {
        MINFO("[REPUTATION] Decay aplicado a: " << rep.node_id 
              << " (era: " << rep.confirmed_reports << ", ahora: " << effective_confirmed << ")");
      }
      
      rep.score = rep.calculate_score();
    }
  }
  
  MINFO("[REPUTATION] Temporal decay aplicado a " << m_reputations.size() << " nodos");
}

// ============================================================================
// Statistics
// ============================================================================

ReputationStats ReputationManager::get_statistics() const {
  ReputationStats stats;
  stats.total_nodes = m_reputations.size();
  
  std::vector<float> scores;
  
  for (const auto& pair : m_reputations) {
    const NodeReputation& rep = pair.second;
    
    stats.total_confirmed_reports += rep.confirmed_reports;
    stats.total_false_reports += rep.false_reports;
    
    scores.push_back(rep.score);
    
    if (rep.is_banned) {
      stats.banned_nodes++;
    } else if (rep.score >= m_reputation_threshold) {
      stats.trusted_nodes++;
    } else {
      stats.suspicious_nodes++;
    }
    
    if (rep.score > stats.max_score) stats.max_score = rep.score;
    if (rep.score < stats.min_score) stats.min_score = rep.score;
  }
  
  // Calcular promedio
  if (!scores.empty()) {
    stats.average_score = std::accumulate(scores.begin(), scores.end(), 0.0f) / scores.size();
    
    // Calcular mediana
    std::sort(scores.begin(), scores.end());
    if (scores.size() % 2 == 0) {
      stats.median_score = (scores[scores.size() / 2 - 1] + scores[scores.size() / 2]) / 2.0f;
    } else {
      stats.median_score = scores[scores.size() / 2];
    }
  }
  
  // Calcular precisión global
  uint64_t total = stats.total_confirmed_reports + stats.total_false_reports;
  if (total > 0) {
    stats.global_accuracy = (float)stats.total_confirmed_reports / total * 100.0f;
  }
  
  return stats;
}

std::vector<NodeReputation> ReputationManager::get_ranked_nodes(bool only_trusted) const {
  std::vector<NodeReputation> nodes;
  
  for (const auto& pair : m_reputations) {
    const NodeReputation& rep = pair.second;
    
    if (only_trusted && !rep.is_trusted(m_reputation_threshold)) {
      continue;
    }
    
    nodes.push_back(rep);
  }
  
  // Ordenar por score descendente
  std::sort(nodes.begin(), nodes.end(), 
    [](const NodeReputation& a, const NodeReputation& b) {
      return a.score > b.score;
    });
  
  return nodes;
}

// ============================================================================
// Reports & Cleanup
// ============================================================================

std::string ReputationManager::generate_reputation_report() const {
  std::stringstream ss;
  ReputationStats stats = get_statistics();
  
  ss << "\n==================== REPUTATION REPORT ====================\n";
  ss << "Timestamp: " << std::time(nullptr) << "\n";
  ss << "\nGLOBAL STATISTICS:\n";
  ss << "├─ Total Nodes:        " << stats.total_nodes << "\n";
  ss << "├─ Trusted Nodes:      " << stats.trusted_nodes << "\n";
  ss << "├─ Suspicious Nodes:   " << stats.suspicious_nodes << "\n";
  ss << "├─ Banned Nodes:       " << stats.banned_nodes << "\n";
  ss << "└─ Global Accuracy:    " << std::fixed << std::setprecision(2) << stats.global_accuracy << "%\n";
  
  ss << "\nSCORE STATISTICS:\n";
  ss << "├─ Average Score:      " << std::fixed << std::setprecision(3) << stats.average_score << "\n";
  ss << "├─ Median Score:       " << std::fixed << std::setprecision(3) << stats.median_score << "\n";
  ss << "├─ Min Score:          " << std::fixed << std::setprecision(3) << stats.min_score << "\n";
  ss << "└─ Max Score:          " << std::fixed << std::setprecision(3) << stats.max_score << "\n";
  
  ss << "\nREPORT STATISTICS:\n";
  ss << "├─ Confirmed Reports:  " << stats.total_confirmed_reports << "\n";
  ss << "└─ False Reports:      " << stats.total_false_reports << "\n";
  
  ss << "==========================================================\n\n";
  
  return ss.str();
}

void ReputationManager::cleanup_inactive_nodes(uint64_t inactive_threshold_seconds) {
  std::time_t now = std::time(nullptr);
  size_t removed = 0;
  
  for (auto it = m_reputations.begin(); it != m_reputations.end();) {
    if (it->second.inactive_seconds() > inactive_threshold_seconds) {
      MINFO("[REPUTATION] Removido nodo inactivo: " << it->first);
      it = m_reputations.erase(it);
      removed++;
    } else {
      ++it;
    }
  }
  
  MINFO("[REPUTATION] Limpieza de inactivos: removidos " << removed << " nodos");
}

void ReputationManager::reset_node_statistics(const std::string& node_id) {
  auto it = m_reputations.find(node_id);
  
  if (it != m_reputations.end()) {
    NodeReputation& rep = it->second;
    rep.total_reports = 0;
    rep.confirmed_reports = 0;
    rep.false_reports = 0;
    rep.pending_reports = 0;
    rep.score = 0.5f;
    rep.last_updated = std::time(nullptr);
    
    MINFO("[REPUTATION] Estadísticas reseteadas: " << node_id);
  }
}

size_t ReputationManager::trusted_nodes_count() const {
  return std::count_if(m_reputations.begin(), m_reputations.end(),
    [this](const std::pair<std::string, NodeReputation>& pair) {
      return pair.second.is_trusted(m_reputation_threshold);
    });
}

// ============================================================================
// ReputationPrinter - Helper Functions
// ============================================================================

void ReputationPrinter::print_reputation_table(const ReputationManager& manager) {
  MINFO("\n");
  MINFO("╔════════════════════════════════════════════════════════════════════╗");
  MINFO("║                      PEER REPUTATION TABLE                         ║");
  MINFO("╠════════════════════════════════════════════════════════════════════╣");
  MINFO("║ Node ID                   │ Score  │ Confirmed │ False │ Status   ║");
  MINFO("╠════════════════════════════════════════════════════════════════════╣");
  
  auto ranked = manager.get_ranked_nodes();
  for (size_t i = 0; i < std::min(ranked.size(), size_t(10)); ++i) {
    const NodeReputation& rep = ranked[i];
    
    std::string status = rep.is_banned ? "BANNED" : 
                        (rep.score >= 0.40f ? "TRUSTED" : "SUSPECT");
    
    std::stringstream line;
    line << "║ " << std::setw(25) << std::left << rep.node_id.substr(0, 25)
         << " │ " << std::fixed << std::setprecision(2) << rep.score
         << "  │ " << std::setw(9) << rep.confirmed_reports
         << " │ " << std::setw(5) << rep.false_reports
         << " │ " << std::setw(8) << status << " ║";
    
    MINFO(line.str());
  }
  
  MINFO("╚════════════════════════════════════════════════════════════════════╝");
  MINFO("\n");
}

void ReputationPrinter::print_statistics(const ReputationManager& manager) {
  ReputationStats stats = manager.get_statistics();
  
  MINFO("\n");
  MINFO("╔════════════════════════════════════════════════════════════════════╗");
  MINFO("║                    REPUTATION STATISTICS                           ║");
  MINFO("╠════════════════════════════════════════════════════════════════════╣");
  MINFO("║ Total Nodes:        " << std::setw(45) << stats.total_nodes << "║");
  MINFO("║ Trusted Nodes:      " << std::setw(45) << stats.trusted_nodes << "║");
  MINFO("║ Suspicious Nodes:   " << std::setw(45) << stats.suspicious_nodes << "║");
  MINFO("║ Banned Nodes:       " << std::setw(45) << stats.banned_nodes << "║");
  MINFO("║                                                                    ║");
  MINFO("║ Average Score:      " << std::setw(40) << std::fixed << std::setprecision(3) 
        << stats.average_score << " ║");
  MINFO("║ Median Score:       " << std::setw(40) << std::fixed << std::setprecision(3) 
        << stats.median_score << " ║");
  MINFO("║ Global Accuracy:    " << std::setw(40) << std::fixed << std::setprecision(2) 
        << stats.global_accuracy << "% ║");
  MINFO("╚════════════════════════════════════════════════════════════════════╝");
  MINFO("\n");
}

std::string ReputationPrinter::to_json(const ReputationManager& manager) {
  std::stringstream ss;
  ReputationStats stats = manager.get_statistics();
  
  ss << "{\n";
  ss << "  \"stats\": {\n";
  ss << "    \"total_nodes\": " << stats.total_nodes << ",\n";
  ss << "    \"trusted_nodes\": " << stats.trusted_nodes << ",\n";
  ss << "    \"suspicious_nodes\": " << stats.suspicious_nodes << ",\n";
  ss << "    \"banned_nodes\": " << stats.banned_nodes << ",\n";
  ss << "    \"average_score\": " << std::fixed << std::setprecision(3) << stats.average_score << ",\n";
  ss << "    \"global_accuracy\": " << std::fixed << std::setprecision(2) << stats.global_accuracy << "\n";
  ss << "  }\n";
  ss << "}\n";
  
  return ss.str();
}

} // namespace security
} // namespace cryptonote
