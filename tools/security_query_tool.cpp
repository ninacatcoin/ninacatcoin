// Copyright (c) 2026 NinaCatCoin
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php

#include "security_query_tool.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace ninacatcoin {
namespace security {

// ============================================================================
// QueryManager Implementation
// ============================================================================

QueryManager::QueryManager()
    : m_timeout_seconds(30),
      m_max_pending_queries(100),
      m_query_expiry_hours(24) {}

QueryManager::~QueryManager() {
  clear_expired_queries();
}

std::string QueryManager::generate_query_id() {
  // Generar UUID v4 simple usando timestamp + random
  auto now = std::chrono::high_resolution_clock::now();
  auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();
  
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  
  // Usar timestamp como base
  ss << std::setw(12) << timestamp;
  ss << "-";
  
  // Agregar 4 caracteres aleatorios
  for (int i = 0; i < 4; ++i) {
    ss << std::setw(1) << dis(gen);
  }
  
  return ss.str();
}

SecurityQuery QueryManager::create_query(uint64_t height,
                                        const std::string& expected_hash,
                                        const std::string& reported_hash,
                                        const std::string& source,
                                        const std::string& attack_type) {
  SecurityQuery query;
  query.query_id = generate_query_id();
  query.height = height;
  query.expected_hash = expected_hash;
  query.reported_hash = reported_hash;
  query.source = source;
  query.attack_type = attack_type;
  query.timestamp = std::time(nullptr);
  query.state = QueryState::PENDING;
  
  return query;
}

bool QueryManager::add_pending_query(const SecurityQuery& query) {
  if (m_pending_queries.size() >= m_max_pending_queries) {
    MWARNING("QueryManager: Maximum pending queries reached");
    return false;
  }
  
  m_pending_queries[query.query_id] = query;
  m_query_responses[query.query_id] = std::vector<SecurityResponse>();
  
  MINFO("QueryManager: Added query " << query.query_id 
        << " for height " << query.height);
  
  return true;
}

void QueryManager::add_response(const SecurityResponse& response) {
  auto it = m_pending_queries.find(response.query_id);
  if (it == m_pending_queries.end()) {
    MWARNING("QueryManager: Response for unknown query " << response.query_id);
    return;
  }
  
  m_query_responses[response.query_id].push_back(response);
  
  MINFO("QueryManager: Added response from " << response.responder_node_id
        << " to query " << response.query_id);
}

SecurityQuery* QueryManager::get_pending_query(const std::string& query_id) {
  auto it = m_pending_queries.find(query_id);
  if (it == m_pending_queries.end()) {
    return nullptr;
  }
  return &it->second;
}

std::vector<SecurityResponse> QueryManager::get_responses(
    const std::string& query_id) const {
  auto it = m_query_responses.find(query_id);
  if (it == m_query_responses.end()) {
    return std::vector<SecurityResponse>();
  }
  return it->second;
}

void QueryManager::clear_expired_queries() {
  std::time_t now = std::time(nullptr);
  std::time_t expiry_threshold = now - (m_query_expiry_hours * 3600);
  
  for (auto it = m_pending_queries.begin(); it != m_pending_queries.end();) {
    if (it->second.timestamp < expiry_threshold) {
      std::string query_id = it->first;
      it = m_pending_queries.erase(it);
      m_query_responses.erase(query_id);
      MINFO("QueryManager: Expired query " << query_id);
    } else {
      ++it;
    }
  }
}

size_t QueryManager::get_pending_query_count() const {
  return m_pending_queries.size();
}

// ============================================================================
// Consensus Calculation
// ============================================================================

ConsensusResult calculate_consensus(
    const SecurityQuery& query,
    const std::vector<SecurityResponse>& responses) {
  
  ConsensusResult result;
  result.query_id = query.query_id;
  result.total_responses = responses.size();
  
  if (responses.empty()) {
    result.consensus_type = ConsensusType::NO_RESPONSE;
    result.is_confirmed = false;
    return result;
  }
  
  // Contar respuestas afirmativas
  uint64_t confirmed_count = 0;
  float average_reputation = 0.0f;
  
  for (const auto& response : responses) {
    if (response.also_detected) {
      confirmed_count++;
    }
    average_reputation += response.responder_reputation;
  }
  
  result.confirmed_responses = confirmed_count;
  result.average_reputation = average_reputation / responses.size();
  
  // Calcular porcentaje
  float confirmation_ratio = confirmed_count / (float)responses.size();
  result.confirmation_percentage = confirmation_ratio * 100.0f;
  
  // Determinar tipo de consenso
  // Regla: Mínimo 2 confirmaciones Y >= 66% de confirmación
  if (confirmed_count >= 2 && confirmation_ratio >= 0.66f) {
    result.consensus_type = ConsensusType::NETWORK_ATTACK_CONFIRMED;
    result.is_confirmed = true;
  } else if (confirmed_count >= 1 && confirmation_ratio >= 0.50f) {
    result.consensus_type = ConsensusType::MARGINAL_CONFIRMATION;
    result.is_confirmed = false;
  } else if (confirmed_count == 0) {
    result.consensus_type = ConsensusType::LOCAL_ATTACK;
    result.is_confirmed = false;
  } else {
    result.consensus_type = ConsensusType::INCONCLUSIVE;
    result.is_confirmed = false;
  }
  
  result.timestamp = std::time(nullptr);
  
  return result;
}

// ============================================================================
// Response Validation
// ============================================================================

bool validate_response_signature(const SecurityResponse& response,
                                 const crypto::public_key& pubkey) {
  // Implementación simplificada - en producción usar ED25519 real
  // Por ahora retornar true si el response tiene datos válidos
  
  if (response.query_id.empty() || 
      response.responder_node_id.empty() ||
      response.responder_hash.empty()) {
    MERROR("[SECURITY] Invalid response signature - missing data");
    return false;
  }
  
  // TODO: Implementar verificación de firma ED25519 real
  // signature_result = crypto::check_signature(
  //   response.signature,
  //   response.serialize(),
  //   pubkey
  // );
  
  return true;
}

bool is_response_valid(const SecurityResponse& response,
                       const SecurityQuery& original_query) {
  
  // Validar que el response corresponde al query
  if (response.query_id != original_query.query_id) {
    MWARNING("[SECURITY] Response query_id mismatch");
    return false;
  }
  
  // Validar que la respuesta es reciente
  std::time_t now = std::time(nullptr);
  if (now - response.response_time > 300) {  // 5 minutos de tolerancia
    MWARNING("[SECURITY] Response too old");
    return false;
  }
  
  // Validar que el nodo respondedor tiene datos válidos
  if (response.responder_hash.empty()) {
    MWARNING("[SECURITY] Response has empty hash");
    return false;
  }
  
  return true;
}

// ============================================================================
// Serialization / Deserialization
// ============================================================================

std::string serialize_query(const SecurityQuery& query) {
  // Formato: query_id|height|expected_hash|reported_hash|source|attack_type|timestamp
  std::stringstream ss;
  ss << query.query_id << "|"
     << query.height << "|"
     << query.expected_hash << "|"
     << query.reported_hash << "|"
     << query.source << "|"
     << query.attack_type << "|"
     << query.timestamp;
  
  return ss.str();
}

SecurityQuery deserialize_query(const std::string& data) {
  SecurityQuery query;
  
  // Parsing simplificado
  std::stringstream ss(data);
  std::string token;
  std::vector<std::string> parts;
  
  while (std::getline(ss, token, '|')) {
    parts.push_back(token);
  }
  
  if (parts.size() >= 7) {
    query.query_id = parts[0];
    query.height = std::stoull(parts[1]);
    query.expected_hash = parts[2];
    query.reported_hash = parts[3];
    query.source = parts[4];
    query.attack_type = parts[5];
    query.timestamp = std::stol(parts[6]);
  }
  
  return query;
}

std::string serialize_response(const SecurityResponse& response) {
  // Formato: query_id|responder_node_id|also_detected|responder_hash|response_time|reputation
  std::stringstream ss;
  ss << response.query_id << "|"
     << response.responder_node_id << "|"
     << (response.also_detected ? "1" : "0") << "|"
     << response.responder_hash << "|"
     << response.response_time << "|"
     << response.responder_reputation;
  
  return ss.str();
}

SecurityResponse deserialize_response(const std::string& data) {
  SecurityResponse response;
  
  std::stringstream ss(data);
  std::string token;
  std::vector<std::string> parts;
  
  while (std::getline(ss, token, '|')) {
    parts.push_back(token);
  }
  
  if (parts.size() >= 6) {
    response.query_id = parts[0];
    response.responder_node_id = parts[1];
    response.also_detected = (parts[2] == "1");
    response.responder_hash = parts[3];
    response.response_time = std::stol(parts[4]);
    response.responder_reputation = std::stof(parts[5]);
  }
  
  return response;
}

// ============================================================================
// Consensus Result Functions
// ============================================================================

std::string get_consensus_description(const ConsensusResult& result) {
  switch (result.consensus_type) {
    case ConsensusType::NETWORK_ATTACK_CONFIRMED:
      return "NETWORK ATTACK CONFIRMED (>=66% peers affected)";
    case ConsensusType::MARGINAL_CONFIRMATION:
      return "MARGINAL - Some peers affected, need verification";
    case ConsensusType::LOCAL_ATTACK:
      return "LOCAL ATTACK - Only this node affected";
    case ConsensusType::INCONCLUSIVE:
      return "INCONCLUSIVE - Mixed responses, needs investigation";
    case ConsensusType::NO_RESPONSE:
      return "NO RESPONSE - Cannot determine, peers unreachable";
    default:
      return "UNKNOWN CONSENSUS TYPE";
  }
}

void log_consensus_result(const ConsensusResult& result) {
  MERROR("[CONSENSUS] ====================================");
  MERROR("[CONSENSUS] Query ID: " << result.query_id);
  MERROR("[CONSENSUS] Total Responses: " << result.total_responses);
  MERROR("[CONSENSUS] Confirmed: " << result.confirmed_responses);
  MERROR("[CONSENSUS] Confirmation %: " << std::fixed << std::setprecision(2) 
         << result.confirmation_percentage << "%");
  MERROR("[CONSENSUS] Avg Reputation: " << std::fixed << std::setprecision(3)
         << result.average_reputation);
  MERROR("[CONSENSUS] Type: " << get_consensus_description(result));
  MERROR("[CONSENSUS] Status: " << (result.is_confirmed ? "CONFIRMED" : "NOT CONFIRMED"));
  MERROR("[CONSENSUS] ====================================");
}

// ============================================================================
// Network Communication Functions
// ============================================================================

bool send_query_to_peer(const SecurityQuery& query,
                       const std::string& peer_address,
                       const std::string& peer_port) {
  // Implementación simplificada de envío
  // En producción: usar conexión P2P real o RPC
  
  MINFO("[NETWORK] Sending query " << query.query_id 
        << " to peer " << peer_address << ":" << peer_port);
  
  // Simulación: marcar como enviado
  // En real: hacer socket connection
  
  return true;
}

bool wait_for_responses(const std::string& query_id,
                       uint64_t timeout_seconds,
                       uint64_t min_responses) {
  
  std::time_t deadline = std::time(nullptr) + timeout_seconds;
  
  while (std::time(nullptr) < deadline) {
    // Verificar si tenemos suficientes respuestas
    // En producción: conectado a sistema de respuestas P2P real
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  
  MINFO("[NETWORK] Response timeout for query " << query_id);
  return false;
}

// ============================================================================
// Report Generation
// ============================================================================

std::string generate_security_report(const ConsensusResult& result,
                                    const SecurityQuery& query) {
  std::stringstream report;
  
  report << "=== SECURITY CONSENSUS REPORT ===\n";
  report << "Query ID: " << result.query_id << "\n";
  report << "Height: " << query.height << "\n";
  report << "Attack Source: " << query.source << "\n";
  report << "Attack Type: " << query.attack_type << "\n";
  report << "\n";
  report << "Responses Received: " << result.total_responses << "\n";
  report << "Confirmed: " << result.confirmed_responses << "\n";
  report << "Confirmation Ratio: " << std::fixed << std::setprecision(2) 
         << result.confirmation_percentage << "%\n";
  report << "Average Peer Reputation: " << std::fixed << std::setprecision(3)
         << result.average_reputation << "\n";
  report << "\n";
  report << "Consensus Type: " << get_consensus_description(result) << "\n";
  report << "Status: " << (result.is_confirmed ? "CONFIRMED" : "NOT CONFIRMED") << "\n";
  report << "Timestamp: " << result.timestamp << "\n";
  report << "===================================\n";
  
  return report.str();
}

// ============================================================================
// Utility Functions
// ============================================================================

bool is_valid_hash_format(const std::string& hash) {
  // Hash debe ser 64 caracteres hexadecimales
  if (hash.length() != 64) {
    return false;
  }
  
  for (char c : hash) {
    if (!isxdigit(c)) {
      return false;
    }
  }
  
  return true;
}

bool is_valid_node_id(const std::string& node_id) {
  // Node ID debe estar entre 1 y 128 caracteres
  if (node_id.length() < 1 || node_id.length() > 128) {
    return false;
  }
  
  // Permitir alphanuméricas y guiones/puntos
  for (char c : node_id) {
    if (!isalnum(c) && c != '-' && c != '.' && c != '_') {
      return false;
    }
  }
  
  return true;
}

std::string format_timestamp(std::time_t timestamp) {
  char buffer[80];
  struct tm* timeinfo = localtime(&timestamp);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  return std::string(buffer);
}

uint64_t get_current_timestamp() {
  return std::time(nullptr);
}

std::string format_duration_seconds(uint64_t seconds) {
  uint64_t hours = seconds / 3600;
  uint64_t minutes = (seconds % 3600) / 60;
  uint64_t secs = seconds % 60;
  
  std::stringstream ss;
  if (hours > 0) {
    ss << hours << "h ";
  }
  ss << minutes << "m " << secs << "s";
  
  return ss.str();
}

} // namespace security
} // namespace ninacatcoin
