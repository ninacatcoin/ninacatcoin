// Copyright (c) 2026, The ninacatcoin Project
// NINA Learning Module Implementation

#include "nina_learning_module.hpp"
#include "nina_persistent_memory.hpp"
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <ctime>

namespace ninacatcoin_ai {

static NINALearningModule* g_learning_module = nullptr;

NINALearningModule& NINALearningModule::getInstance() {
    if (!g_learning_module) {
        g_learning_module = new NINALearningModule();
    }
    return *g_learning_module;
}

NINALearningModule::NINALearningModule() {
}

NINALearningModule::~NINALearningModule() {
}

bool NINALearningModule::initialize() {
    std::cout << "[NINA Learning] Initializing learning module..." << std::endl;
    std::cout << "[NINA Learning] Using Welford's online algorithm" << std::endl;
    return true;
}

void NINALearningModule::recordMetric(
    const std::string& metric_name,
    double value) {
    
    auto it = metrics.find(metric_name);
    
    if (it != metrics.end()) {
        updateMetricStatistics(metric_name, value);
    } else {
        LearningMetric new_metric;
        new_metric.metric_name = metric_name;
        new_metric.current_value = value;
        new_metric.average_value = value;
        new_metric.variance = 0.0;
        new_metric.sample_count = 1;
        new_metric.min_value = value;
        new_metric.max_value = value;
        new_metric.std_deviation = 0.0;
        new_metric.sample_variance = 0.0;
        
        metrics[metric_name] = new_metric;
    }
}

void NINALearningModule::updateMetricStatistics(
    const std::string& metric_name,
    double value) {
    
    auto it = metrics.find(metric_name);
    if (it == metrics.end()) return;
    
    LearningMetric& metric = it->second;
    metric.current_value = value;
    
    uint64_t n = metric.sample_count;
    metric.sample_count++;
    
    double delta = value - metric.average_value;
    metric.average_value += delta / metric.sample_count;
    
    double delta2 = value - metric.average_value;
    metric.variance += delta * delta2;
    
    metric.min_value = std::min(metric.min_value, value);
    metric.max_value = std::max(metric.max_value, value);
    
    if (metric.sample_count > 1) {
        metric.sample_variance = metric.variance / (metric.sample_count - 1);
        metric.std_deviation = std::sqrt(metric.sample_variance);
    }
}

const LearningMetric* NINALearningModule::getMetricStats(const std::string& metric_name) const {
    auto it = metrics.find(metric_name);
    if (it != metrics.end()) {
        return &it->second;
    }
    return nullptr;
}

bool NINALearningModule::isAnomaly(
    const std::string& metric_name,
    double value) const {
    
    auto it = metrics.find(metric_name);
    if (it == metrics.end() || it->second.sample_count < 3) {
        return false;
    }
    
    const LearningMetric& metric = it->second;
    double upper = metric.average_value + (2.0 * metric.std_deviation);
    double lower = metric.average_value - (2.0 * metric.std_deviation);
    
    return (value > upper) || (value < lower);
}

std::string NINALearningModule::generateLearningReport() const {
    std::stringstream ss;
    ss << "\n====== NINA LEARNING REPORT ======\n";
    ss << "Metrics tracked: " << metrics.size() << "\n";
    
    for (const auto& pair : metrics) {
        const LearningMetric& m = pair.second;
        ss << "  " << m.metric_name << ": avg=" << m.average_value 
           << " std_dev=" << m.std_deviation << " samples=" << m.sample_count << "\n";
    }
    
    ss << "====================================\n\n";
    return ss.str();
}

double NINALearningModule::getNetworkHealthConfidence() const {
    if (metrics.empty()) return 0.5;
    
    int healthy = 0;
    for (const auto& pair : metrics) {
        if (!isAnomaly(pair.first, pair.second.current_value)) {
            healthy++;
        }
    }
    
    return static_cast<double>(healthy) / static_cast<double>(metrics.size());
}

double NINALearningModule::getAttackProbability() const {
    double health = getNetworkHealthConfidence();
    return (1.0 - health) * 0.5;  // Simple heuristic
}

int NINALearningModule::getAnomalyCount() const {
    int count = 0;
    for (const auto& pair : metrics) {
        if (isAnomaly(pair.first, pair.second.current_value)) {
            count++;
        }
    }
    return count;
}

// ============ IMPLEMENTACIÓN DE MÉTODOS DE PERSISTENCIA ============

bool NINALearningModule::persistToLMDB(uint64_t current_height) {
    try {
        std::cout << "[NINA-Learning] === PERSISTENCIA INICIADA ===" << std::endl;
        std::cout << "[NINA-Learning] Guardando " << metrics.size() 
                  << " métricas en LMDB" << std::endl;
        
        // Serializar métricas
        std::stringstream metrics_stream;
        for (const auto& metric : metrics) {
            metrics_stream << metric.second.serialize() << "\n";
        }
        std::cout << "[NINA-Learning] Métricas serializadas ✓" << std::endl;
        
        // Guardar confianza y probabilidades
        std::cout << "[NINA-Learning]   Health Confidence: " << (network_health_confidence * 100.0) << "%" << std::endl;
        std::cout << "[NINA-Learning]   Attack Probability: " << (overall_attack_probability * 100.0) << "%" << std::endl;
        std::cout << "[NINA-Learning]   Anomalía Count: " << anomaly_count << std::endl;
        
        // Llamar a persistencia con serialized metric strings
        std::map<std::string, std::string> serialized_metrics;
        for (const auto& metric : metrics) {
            serialized_metrics[metric.first] = metric.second.serialize();
        }
        daemonize::persist_learning_module_data(
            (void*)&serialized_metrics
        );
        
        // Auditar evento
        daemonize::nina_audit_log(
            current_height,
            "LEARNING_PERSIST",
            std::to_string(metrics.size()) + " metrics, health=" + 
            std::to_string(network_health_confidence)
        );
        
        last_persist_height = current_height;
        std::cout << "[NINA-Learning] === PERSISTENCIA COMPLETADA ===" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cout << "[NINA-Learning] ERROR en persistencia: " << e.what() << std::endl;
        return false;
    }
}

bool NINALearningModule::loadFromLMDB() {
    try {
        std::cout << "[NINA-Learning] === CARGA DE DATOS LMDB INICIADA ===" << std::endl;
        std::cout << "[NINA-Learning] DATA.MDB: ~/.ninacatcoin/nina_state/data.mdb" << std::endl;
        
        // Load serialized metrics from LMDB
        std::string metrics_data;
        if (!daemonize::load_learning_module_data(metrics_data) || metrics_data.empty()) {
            std::cout << "[NINA-Learning] No previous learning data found (first run)" << std::endl;
            return false;
        }

        // Parse each line as a serialized LearningMetric
        std::istringstream stream(metrics_data);
        std::string line;
        size_t loaded = 0;

        while (std::getline(stream, line)) {
            if (line.empty()) continue;

            // Format: metric_name|current_value|average_value|variance|std_deviation|min_value|max_value|sample_variance|sample_count
            std::istringstream ls(line);
            std::string token;
            LearningMetric metric;
            int field = 0;

            while (std::getline(ls, token, '|')) {
                try {
                    switch (field++) {
                        case 0: metric.metric_name = token; break;
                        case 1: metric.current_value = std::stod(token); break;
                        case 2: metric.average_value = std::stod(token); break;
                        case 3: metric.variance = std::stod(token); break;
                        case 4: metric.std_deviation = std::stod(token); break;
                        case 5: metric.min_value = std::stod(token); break;
                        case 6: metric.max_value = std::stod(token); break;
                        case 7: metric.sample_variance = std::stod(token); break;
                        case 8: metric.sample_count = std::stoull(token); break;
                    }
                } catch (...) {}
            }

            if (!metric.metric_name.empty() && field >= 9) {
                metrics[metric.metric_name] = metric;
                loaded++;
            }
        }

        std::cout << "[NINA-Learning] ✓ " << loaded << " métricas restauradas desde LMDB" << std::endl;
        std::cout << "[NINA-Learning] === CARGA COMPLETADA ===" << std::endl;
        
        return loaded > 0;
    } catch (const std::exception& e) {
        std::cout << "[NINA-Learning] Advertencia: No se pudo cargar estado anterior: " << e.what() << std::endl;
        return false;
    }
}

} // namespace ninacatcoin_ai
