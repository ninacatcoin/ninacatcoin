// Copyright (c) 2026, The Ninacoin Project
//
// All rights reserved.

#include "daemon/nina_ml_client.hpp"
#include "misc_log_ex.h"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cmath>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
    typedef int SOCKET;
#endif

#undef NINACOIN_DEFAULT_LOG_CATEGORY
#define NINACOIN_DEFAULT_LOG_CATEGORY "nina_ml"

namespace NINA_ML
{

// ============================================================================
// NINAMLClient Implementation
// ============================================================================

NINAMLClient::NINAMLClient()
    : m_socket(INVALID_SOCKET),
      m_host("127.0.0.1"),
      m_port(5556),
      m_connected(false),
      m_shutdown(false)
{
    MINFO("[NINA-ML] Client initialized (target: " << m_host << ":" << m_port << ")");
}

NINAMLClient::~NINAMLClient()
{
    disconnect();
}

bool NINAMLClient::connect(const std::string& host, uint16_t port)
{
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    
    m_host = host;
    m_port = port;
    
    return connectSocket();
}

bool NINAMLClient::connectSocket()
{
    try
    {
#ifdef _WIN32
        static bool wsa_initialized = false;
        if (!wsa_initialized)
        {
            WSADATA wsa_data;
            int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
            if (result != 0)
            {
                MERROR("[NINA-ML] WSAStartup failed: " << result);
                return false;
            }
            wsa_initialized = true;
        }
#endif

        m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (m_socket == INVALID_SOCKET)
        {
            MERROR("[NINA-ML] Failed to create socket");
            return false;
        }

        // Set socket timeout (non-blocking for connection)
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(m_port);
        
        if (inet_pton(AF_INET, m_host.c_str(), &server_addr.sin_addr) <= 0)
        {
            MERROR("[NINA-ML] Invalid address: " << m_host);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        // Connect with timeout
        int connect_result = ::connect(m_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (connect_result == SOCKET_ERROR)
        {
            MDEBUG("[NINA-ML] Connection failed to " << m_host << ":" << m_port);
            closesocket(m_socket);
            m_socket = INVALID_SOCKET;
            return false;
        }

        m_connected.store(true);
        MINFO("[NINA-ML] Connected to ML service at " << m_host << ":" << m_port);
        return true;
    }
    catch (const std::exception& e)
    {
        MERROR("[NINA-ML] Connection exception: " << e.what());
        m_connected.store(false);
        return false;
    }
}

MLResponse NINAMLClient::featureRequest(
    const std::string& tier,
    const std::string& decision_id,
    const std::map<std::string, double>& features,
    int timeout_ms)
{
    MLResponse response;
    response.is_valid = false;
    response.confidence = 0.0;
    response.risk_score = 1.0;

    if (!is_connected() && !connectSocket())
    {
        MTRACE("[NINA-ML] ML service unavailable, using fallback validation");
        return response;
    }

    try
    {
        // Build JSON request
        std::string json_request = buildJsonRequest(tier, decision_id, features);
        
        // Send request
        if (!sendRequest(json_request))
        {
            MWARNING("[NINA-ML] Failed to send request to ML service");
            return response;
        }

        // Receive response with extended timeout
        std::string json_response = receiveResponse(timeout_ms * 2);  // Double timeout for safety
        if (json_response.empty())
        {
            MERROR("[NINA-ML] Empty response from ML service");
            return response;
        }

        // Parse response
        response = parseJsonResponse(json_response);
        if (response.confidence > 0.0) {
            MINFO("[NINA-ML] Got response from ML service: confidence=" << std::fixed << std::setprecision(2) << response.confidence);
        }
        return response;
    }
    catch (const std::exception& e)
    {
        MERROR("[NINA-ML] Feature request exception: " << e.what());
        m_connected.store(false);
        return response;
    }
}

std::string NINAMLClient::buildJsonRequest(
    const std::string& phase,
    const std::string& decision_id,
    const std::map<std::string, double>& features)
{
    std::ostringstream oss;
    oss << "{\"phase\":\"" << phase
        << "\",\"decision_id\":\"" << decision_id
        << "\",\"features\":{";
    
    bool first = true;
    for (const auto& [key, value] : features)
    {
        if (!first) oss << ",";
        oss << "\"" << key << "\":" << std::fixed << std::setprecision(6) << value;
        first = false;
    }
    
    oss << "}}";
    return oss.str();
}

MLResponse NINAMLClient::parseJsonResponse(const std::string& json_response)
{
    MLResponse response;
    response.is_valid = false;
    response.confidence = 0.0;
    response.risk_score = 1.0;
    response.difficulty_multiplier = 1.0;

    try
    {
        // Simple JSON parsing (extract key values)
        // Format: {"confidence": 0.85, "is_valid": true, "risk_score": 0.15, "difficulty_multiplier": 1.02, ...}
        
        size_t conf_pos = json_response.find("\"confidence\":");
        if (conf_pos != std::string::npos)
        {
            size_t val_start = json_response.find(":", conf_pos) + 1;
            size_t val_end = json_response.find(",", val_start);
            if (val_end == std::string::npos)
                val_end = json_response.find("}", val_start);
            
            std::string conf_str = json_response.substr(val_start, val_end - val_start);
            // Trim whitespace
            conf_str.erase(0, conf_str.find_first_not_of(" \t\n\r"));
            conf_str.erase(conf_str.find_last_not_of(" \t\n\r") + 1);
            
            try
            {
                response.confidence = std::stod(conf_str);
            }
            catch (...)
            {
                response.confidence = 0.0;
            }
        }

        size_t valid_pos = json_response.find("\"is_valid\":");
        if (valid_pos != std::string::npos)
        {
            size_t val_start = json_response.find(":", valid_pos) + 1;
            size_t val_end = json_response.find(",", val_start);
            if (val_end == std::string::npos)
                val_end = json_response.find("}", val_start);
            
            std::string valid_str = json_response.substr(val_start, val_end - val_start);
            // Trim whitespace
            valid_str.erase(0, valid_str.find_first_not_of(" \t\n\r"));
            valid_str.erase(valid_str.find_last_not_of(" \t\n\r") + 1);
            
            response.is_valid = (valid_str.find("true") != std::string::npos);
        }

        // NEW: Extract difficulty_multiplier for PHASE 2
        size_t mult_pos = json_response.find("\"difficulty_multiplier\":");
        if (mult_pos != std::string::npos)
        {
            size_t val_start = json_response.find(":", mult_pos) + 1;
            size_t val_end = json_response.find(",", val_start);
            if (val_end == std::string::npos)
                val_end = json_response.find("}", val_start);
            
            std::string mult_str = json_response.substr(val_start, val_end - val_start);
            // Trim whitespace
            mult_str.erase(0, mult_str.find_first_not_of(" \t\n\r"));
            mult_str.erase(mult_str.find_last_not_of(" \t\n\r") + 1);
            
            try
            {
                response.difficulty_multiplier = std::stod(mult_str);
            }
            catch (...)
            {
                response.difficulty_multiplier = 1.0;
            }
        }

        response.risk_score = 1.0 - response.confidence;
        response.model_version = "PHASE_1_BLOCK_VALIDATOR";
        
        return response;
    }
    catch (const std::exception& e)
    {
        MERROR("[NINA-ML] JSON parse exception: " << e.what());
        return response;
    }
}

bool NINAMLClient::sendRequest(const std::string& json_request)
{
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    
    if (m_socket == INVALID_SOCKET)
        return false;

    // Add newline terminator for line-based protocol
    std::string request_with_newline = json_request + "\n";
    
    int sent = send(m_socket, request_with_newline.c_str(), request_with_newline.length(), 0);
    if (sent == SOCKET_ERROR || sent <= 0)
    {
        MERROR("[NINA-ML] Send failed");
        m_connected.store(false);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    return true;
}

std::string NINAMLClient::receiveResponse(int timeout_ms)
{
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    
    if (m_socket == INVALID_SOCKET)
        return "";

    // Set socket receive timeout
#ifdef _WIN32
    DWORD timeout = timeout_ms;
    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) < 0)
    {
        MERROR("[NINA-ML] Failed to set socket timeout");
        return "";
    }
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        MERROR("[NINA-ML] Failed to set socket timeout");
        return "";
    }
#endif

    char buffer[4096];
    memset(buffer, 0, sizeof(buffer));
    
    int received = recv(m_socket, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0)
    {
        if (received == 0)
        {
            MWARNING("[NINA-ML] ML service closed connection");
        }
        else
        {
            MERROR("[NINA-ML] Receive failed");
        }
        m_connected.store(false);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return "";
    }

    buffer[received] = '\0';
    return std::string(buffer);
}

bool NINAMLClient::reconnect()
{
    disconnect();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    return connectSocket();
}

void NINAMLClient::disconnect()
{
    std::lock_guard<std::mutex> lock(m_socket_mutex);
    
    m_shutdown.store(true);
    m_connected.store(false);
    
    if (m_socket != INVALID_SOCKET)
    {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    
    MINFO("[NINA-ML] Disconnected from ML service");
}

bool NINAMLClient::getModelStatus(const std::string& phase)
{
    std::map<std::string, double> health_check;
    health_check["check_type"] = 1.0;  // 1.0 = health check
    
    MLResponse response = featureRequest("HEALTH_CHECK", "health_" + phase, health_check, 50);
    return response.confidence > 0.5;
}

void NINAMLClient::logTrainingEvent(
    const std::string& phase,
    const std::string& outcome,
    const std::map<std::string, double>& features)
{
    try
    {
        std::map<std::string, double> training_event = features;
        training_event["training_mode"] = 1.0;  // Mark as training event
        
        // Non-blocking log (ignore response)
        featureRequest("TRAINING_LOG_" + phase, outcome, training_event, 10);
    }
    catch (const std::exception& e)
    {
        MWARNING("[NINA-ML] Training event logging failed: " << e.what());
    }
}

// ============================================================================
// Convenience Functions
// ============================================================================

MLResponse validateBlock(
    const std::string& block_hash,
    uint64_t timestamp,
    double difficulty,
    const std::string& miner_address,
    int txs_count,
    double network_health,
    uint32_t hash_entropy,
    double miner_reputation)
{
    std::map<std::string, double> features;
    features["timestamp"] = static_cast<double>(timestamp);
    features["difficulty"] = difficulty;
    features["txs_count"] = static_cast<double>(txs_count);
    features["network_health"] = network_health;
    features["hash_entropy"] = static_cast<double>(hash_entropy);
    features["miner_reputation"] = miner_reputation;
    
    NINAMLClient& client = NINAMLClient::get_instance();
    return client.featureRequest("PHASE_1_BLOCK_VALIDATE", block_hash, features, 500);
}

double suggestDifficultyAdjustment(
    double current_difficulty,
    uint64_t last_block_time,
    const std::vector<uint64_t>& recent_block_times,
    double current_hashrate,
    double hashrate_trend_percent,
    uint64_t target_block_time)
{
    if (recent_block_times.empty())
        return 1.0;  // No adjustment

    // Build complete features for PHASE 2
    std::map<std::string, double> features;
    features["difficulty"] = current_difficulty;
    features["block_time"] = static_cast<double>(last_block_time);
    features["hashrate"] = current_hashrate;
    features["hashrate_trend"] = hashrate_trend_percent;
    features["target_block_time"] = static_cast<double>(target_block_time);
    
    // Also include average for reference
    uint64_t total_time = 0;
    for (uint64_t t : recent_block_times)
        total_time += t;
    double avg_time = static_cast<double>(total_time) / recent_block_times.size();
    features["avg_block_time"] = avg_time;
    
    NINAMLClient& client = NINAMLClient::get_instance();
    MLResponse response = client.featureRequest(
        "PHASE_2_DIFFICULTY_OPTIMIZE",
        "difficulty_suggest",
        features,
        100
    );
    
    // Return difficulty multiplier from PHASE 2
    return response.difficulty_multiplier;
}

double analyzePeerSybil(
    const std::string& peer_id,
    int connection_count,
    uint64_t handshake_timestamp,
    double behavior_score)
{
    std::map<std::string, double> features;
    features["connection_count"] = static_cast<double>(connection_count);
    features["handshake_timestamp"] = static_cast<double>(handshake_timestamp);
    features["behavior_score"] = behavior_score;
    
    NINAMLClient& client = NINAMLClient::get_instance();
    MLResponse response = client.featureRequest(
        "PHASE_3_SYBIL_DETECT",
        peer_id,
        features,
        100
    );
    
    return response.risk_score;  // Risk score (higher = more suspicious)
}

double optimizeGasPricing(
    double current_supply,
    int transaction_count,
    double base_price)
{
    std::map<std::string, double> features;
    features["current_supply"] = current_supply;
    features["transaction_count"] = static_cast<double>(transaction_count);
    features["base_price"] = base_price;
    
    NINAMLClient& client = NINAMLClient::get_instance();
    MLResponse response = client.featureRequest(
        "PHASE_4_GAS_OPTIMIZE",
        "gas_price_optimize",
        features,
        100
    );
    
    // Return adjusted price based on confidence
    return base_price * (1.0 + (response.confidence - 0.5));
}

} // namespace NINA_ML
