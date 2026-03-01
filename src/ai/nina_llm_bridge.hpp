// Copyright (c) 2026, The ninacatcoin Project
//
// LLM Bridge — connects to a local llama-server HTTP endpoint for
// NINA's advisory AI capabilities (mempool analysis, threat assessment,
// ring quality evaluation, etc.)
//
// Architecture:
// - Daemon runs llama-server as a separate process (or systemd service)
// - This bridge sends HTTP POST requests to /completion endpoint
// - All responses are ADVISORY ONLY — never affect consensus
// - For consensus-critical AI, use ai_embedded_model_weights.hpp instead
//
// The bridge gracefully degrades: if llama-server is not running,
// all methods return empty/default results (no crashes, no errors).

#pragma once

#include <string>
#include <cstdint>
#include <sstream>
#include <vector>
#include "cryptonote_config.h"
#include "nina_model_validator.hpp"

// Lightweight HTTP client using system sockets
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

namespace ninacatcoin_ai {

struct LLMMempoolInsight {
    bool        valid = false;
    double      spam_probability = 0.0;
    std::string analysis;
    std::string recommended_action;
};

struct LLMThreatAssessment {
    bool        valid = false;
    std::string threat_level;     // NORMAL, ELEVATED, HIGH, CRITICAL
    std::string analysis;
    double      confidence = 0.0;
};

struct LLMRingQuality {
    bool        valid = false;
    double      quality_score = 0.0;  // 0.0 - 1.0
    std::string analysis;
    std::string recommendation;
};

class NinaLLMBridge {
public:
    static NinaLLMBridge& getInstance() {
        static NinaLLMBridge instance;
        return instance;
    }

    /**
     * Check if llama-server is reachable.
     * Tries a lightweight GET /health request.
     */
    bool is_available() const {
        std::string response = http_get("/health");
        return !response.empty() && response.find("ok") != std::string::npos;
    }

    /**
     * Initialize the bridge. Call once at daemon startup.
     * Validates model if present, checks server connectivity.
     */
    bool initialize(const std::string& data_dir) {
        m_data_dir = data_dir;
        
        // Validate model file (if not placeholder hash)
        auto validation = validate_nina_model(data_dir);
        if (validation.valid) {
            m_model_hash = validation.computed_hash;
            m_model_validated = true;
        } else if (validation.expected_hash == std::string(64, '0')) {
            // Placeholder — model not yet deployed, bridge runs in stub mode
            m_model_validated = false;
        } else {
            // Real hash but validation failed
            m_model_validated = false;
            m_last_error = validation.error;
        }

        // Check server
        m_server_available = is_available();
        return m_model_validated || m_server_available;
    }

    /**
     * Get the validated model hash (empty if not validated).
     */
    std::string get_model_hash() const { return m_model_hash; }
    bool is_model_validated() const { return m_model_validated; }

    /**
     * Analyze mempool state using NINA LLM.
     * Returns advisory insight about spam/congestion.
     */
    LLMMempoolInsight analyze_mempool(
        uint32_t tx_count, uint64_t weight,
        double avg_fee, uint32_t bot_count,
        uint32_t spam_count, uint32_t normal_count,
        double health_score, const std::string& source_info) const
    {
        if (!m_server_available) return {};

        std::ostringstream prompt;
        prompt << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
               << "You are NINA, the embedded AI of NinaCatCoin. "
               << "Analyze this mempool snapshot and assess spam risk.\n"
               << "<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n"
               << "Mempool: " << tx_count << " txs, weight=" << weight
               << ", avg_fee=" << avg_fee << ", bot=" << bot_count
               << ", spam=" << spam_count << ", normal=" << normal_count
               << ", health=" << health_score
               << ". Source: " << source_info
               << "\n<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n";

        std::string response = query_completion(prompt.str(), 256);
        if (response.empty()) return {};

        LLMMempoolInsight insight;
        insight.valid = true;
        insight.analysis = response;
        insight.spam_probability = static_cast<double>(spam_count) / 
                                   std::max(1u, tx_count);
        insight.recommended_action = (insight.spam_probability > 0.5) 
                                     ? "increase_fee_threshold" : "normal";
        return insight;
    }

    /**
     * Assess network threat level using NINA LLM.
     */
    LLMThreatAssessment assess_threat(
        double hashrate_change_pct, uint32_t orphan_rate,
        uint32_t suspicious_peers, uint32_t total_peers) const
    {
        if (!m_server_available) return {};

        std::ostringstream prompt;
        prompt << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
               << "You are NINA. Assess the network threat level: "
               << "NORMAL, ELEVATED, HIGH, or CRITICAL.\n"
               << "<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n"
               << "Hashrate change: " << hashrate_change_pct << "%, "
               << "orphan_rate=" << orphan_rate << ", "
               << "suspicious_peers=" << suspicious_peers << "/" << total_peers
               << "\n<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n";

        std::string response = query_completion(prompt.str(), 256);
        if (response.empty()) return {};

        LLMThreatAssessment assessment;
        assessment.valid = true;
        assessment.analysis = response;
        // Parse threat level from response
        if (response.find("CRITICAL") != std::string::npos) {
            assessment.threat_level = "CRITICAL";
            assessment.confidence = 0.9;
        } else if (response.find("HIGH") != std::string::npos) {
            assessment.threat_level = "HIGH";
            assessment.confidence = 0.8;
        } else if (response.find("ELEVATED") != std::string::npos) {
            assessment.threat_level = "ELEVATED";
            assessment.confidence = 0.7;
        } else {
            assessment.threat_level = "NORMAL";
            assessment.confidence = 0.9;
        }
        return assessment;
    }

    /**
     * Evaluate ring signature quality for a transaction.
     */
    LLMRingQuality evaluate_ring_quality(
        uint32_t ring_size, uint32_t decoy_age_median_hours,
        double temporal_clustering_score, bool has_recent_outputs) const
    {
        if (!m_server_available) return {};

        std::ostringstream prompt;
        prompt << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
               << "You are NINA. Evaluate ring signature quality (0.0-1.0). "
               << "Consider decoy selection, temporal distribution, and privacy.\n"
               << "<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n"
               << "Ring size=" << ring_size 
               << ", decoy_age_median=" << decoy_age_median_hours << "h, "
               << "temporal_clustering=" << temporal_clustering_score
               << ", recent_outputs=" << (has_recent_outputs ? "yes" : "no")
               << "\n<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n";

        std::string response = query_completion(prompt.str(), 256);
        if (response.empty()) return {};

        LLMRingQuality quality;
        quality.valid = true;
        quality.analysis = response;
        // Default quality based on ring size
        quality.quality_score = std::min(1.0, static_cast<double>(ring_size) / 31.0);
        return quality;
    }

    /**
     * General-purpose query to NINA (for RPC/diagnostic use).
     */
    std::string query(const std::string& user_message) const {
        if (!m_server_available) return "[NINA offline]";

        std::ostringstream prompt;
        prompt << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
               << "You are NINA, the AI of NinaCatCoin blockchain. "
               << "Answer concisely and technically.\n"
               << "<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n"
               << user_message
               << "\n<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n";

        return query_completion(prompt.str(), 512);
    }

    std::string enhance_mempool_report(
        const std::string& base_report, double health_score) const
    {
        if (!m_server_available) return base_report;

        std::ostringstream prompt;
        prompt << "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
               << "You are NINA. Enhance this mempool report with insights.\n"
               << "<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n"
               << "Health: " << health_score << "\nReport:\n" << base_report
               << "\n<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n";

        std::string enhanced = query_completion(prompt.str(), 512);
        return enhanced.empty() ? base_report : enhanced;
    }

private:
    NinaLLMBridge() = default;
    NinaLLMBridge(const NinaLLMBridge&) = delete;
    NinaLLMBridge& operator=(const NinaLLMBridge&) = delete;

    std::string m_data_dir;
    std::string m_model_hash;
    std::string m_last_error;
    bool m_model_validated = false;
    mutable bool m_server_available = false;

    /**
     * Send a completion request to llama-server.
     * Uses raw sockets for minimal dependencies.
     */
    std::string query_completion(const std::string& prompt, int max_tokens) const {
        // Build JSON body
        std::string escaped_prompt = json_escape(prompt);
        std::ostringstream body;
        body << "{\"prompt\":\"" << escaped_prompt << "\","
             << "\"n_predict\":" << max_tokens << ","
             << "\"temperature\":" << NINA_LLM_TEMP << ","
             << "\"stop\":[\"<|eot_id|>\",\"<|end_of_text|>\"]}";

        std::string response = http_post("/completion", body.str());
        if (response.empty()) {
            m_server_available = false;
            return "";
        }

        // Extract "content" field from JSON response
        return extract_json_string(response, "content");
    }

    /**
     * Minimal HTTP GET request.
     */
    std::string http_get(const std::string& path) const {
        return http_request("GET", path, "");
    }

    /**
     * Minimal HTTP POST request with JSON body.
     */
    std::string http_post(const std::string& path, const std::string& json_body) const {
        return http_request("POST", path, json_body);
    }

    /**
     * Raw socket HTTP request to llama-server.
     * Timeout: 30 seconds for inference requests.
     */
    std::string http_request(const std::string& method, 
                             const std::string& path,
                             const std::string& body) const {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return "";
#endif
        int sock = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
        if (sock < 0) return "";

        // Set timeout (30s)
        struct timeval tv;
        tv.tv_sec = 30;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, 
                   reinterpret_cast<const char*>(&tv), sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv));

        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(NINA_LLAMA_SERVER_PORT);
        inet_pton(AF_INET, NINA_LLAMA_SERVER_HOST, &server_addr.sin_addr);

        if (connect(sock, reinterpret_cast<struct sockaddr*>(&server_addr), 
                    sizeof(server_addr)) < 0) {
#ifdef _WIN32
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
            return "";
        }

        // Build HTTP request
        std::ostringstream req;
        req << method << " " << path << " HTTP/1.1\r\n"
            << "Host: " << NINA_LLAMA_SERVER_HOST << ":" << NINA_LLAMA_SERVER_PORT << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n"
            << "\r\n"
            << body;

        std::string request = req.str();
        send(sock, request.c_str(), static_cast<int>(request.size()), 0);

        // Read response
        std::string response;
        char buf[4096];
        int n;
        while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0) {
            buf[n] = '\0';
            response += buf;
        }

#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif

        // Strip HTTP headers (find \r\n\r\n)
        auto header_end = response.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            return response.substr(header_end + 4);
        }
        return response;
    }

    /**
     * Escape string for JSON.
     */
    static std::string json_escape(const std::string& s) {
        std::string result;
        result.reserve(s.size() + 16);
        for (char c : s) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c;
            }
        }
        return result;
    }

    /**
     * Extract a string value from a JSON object by key.
     * Minimal parser — works for flat llama-server responses.
     */
    static std::string extract_json_string(const std::string& json, const std::string& key) {
        std::string search = "\"" + key + "\"";
        auto pos = json.find(search);
        if (pos == std::string::npos) return "";

        // Find the colon after the key
        pos = json.find(':', pos + search.size());
        if (pos == std::string::npos) return "";

        // Find opening quote of value
        pos = json.find('"', pos + 1);
        if (pos == std::string::npos) return "";
        pos++; // skip opening quote

        // Collect until unescaped closing quote
        std::string value;
        bool escaped = false;
        for (size_t i = pos; i < json.size(); i++) {
            if (escaped) {
                switch (json[i]) {
                    case 'n': value += '\n'; break;
                    case 'r': value += '\r'; break;
                    case 't': value += '\t'; break;
                    case '"': value += '"'; break;
                    case '\\': value += '\\'; break;
                    default: value += json[i];
                }
                escaped = false;
            } else if (json[i] == '\\') {
                escaped = true;
            } else if (json[i] == '"') {
                break; // end of value
            } else {
                value += json[i];
            }
        }
        return value;
    }
};

} // namespace ninacatcoin_ai
