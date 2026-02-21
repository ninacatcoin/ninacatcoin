// Copyright (c) 2026, The ninacatcoin Project
// NINA Learning Module Header

#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include <sstream>

namespace ninacatcoin_ai {

struct LearningMetric {
    std::string metric_name;
    double current_value = 0.0;
    double average_value = 0.0;
    double variance = 0.0;
    double std_deviation = 0.0;
    double min_value = 0.0;
    double max_value = 0.0;
    double sample_variance = 0.0;
    uint64_t sample_count = 0;
    
    // Serialización para persistencia en LMDB
    std::string serialize() const {
        std::stringstream ss;
        ss << metric_name << "|" << current_value << "|" << average_value << "|"
           << variance << "|" << std_deviation << "|" << min_value << "|"
           << max_value << "|" << sample_variance << "|" << sample_count;
        return ss.str();
    }
};

class NINALearningModule {
public:
    static NINALearningModule& getInstance();
    
    bool initialize();
    void recordMetric(const std::string& metric_name, double value);
    
    bool isAnomaly(const std::string& metric_name, double value) const;
    double getNetworkHealthConfidence() const;
    double getAttackProbability() const;
    void updateMetricStatistics(const std::string& metric_name, double value);
    
    const LearningMetric* getMetricStats(const std::string& metric_name) const;
    std::string generateLearningReport() const;
    int getAnomalyCount() const;
    
    // ============ MÉTODOS DE PERSISTENCIA ============
    // Guardar estado en LMDB (~/.ninacatcoin/lmdb/data.mdb)
    bool persistToLMDB(uint64_t current_height = 0);
    // Cargar estado desde LMDB en startup
    bool loadFromLMDB();
    // Obtener estadísticas de persistencia
    uint64_t getLastPersistHeight() const { return last_persist_height; }
    uint64_t getMetricCount() const { return metrics.size(); }
    
private:
    NINALearningModule();
    ~NINALearningModule();
    
    std::map<std::string, LearningMetric> metrics;
    double network_health_confidence = 0.5;
    double overall_attack_probability = 0.0;
    int anomaly_count = 0;
    
    // Tracking de persistencia
    uint64_t last_persist_height = 0;
};

} // namespace ninacatcoin_ai
