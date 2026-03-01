// Copyright (c) 2026, The ninacatcoin Project
//
// All rights reserved.
//
// nina_llm_engine_stub.cpp — Stub implementations for NinaLLMEngine
//
// PURPOSE: Provides no-op implementations of NinaLLMEngine methods that are
// referenced from blockchain.cpp and nina_learning_module.cpp inside
// libcryptonote_core.a. These stubs satisfy the linker for targets that
// don't link the real daemon code (blockchain utilities, wallet tools, etc.).
//
// When building the daemon (ninacatcoind), the real implementations from
// src/daemon/nina_llm_engine.cpp are compiled directly into the executable
// and the linker uses those instead of these stubs (standard UNIX static
// archive behavior: archive members are only pulled in to resolve otherwise-
// undefined symbols).
//
// IMPORTANT: Keep the signatures in sync with nina_llm_engine.hpp.

#include "daemon/nina_llm_engine.hpp"

namespace ninacatcoin_ai {

// The real implementations live in src/daemon/nina_llm_engine.cpp.
// These stubs are only reached by non-daemon targets (blockchain utils, etc.).

void NinaLLMEngine::shutdown()
{
    // No-op in stub — nothing to shut down.
}

AnalysisResult NinaLLMEngine::analyze_block_anomaly(
    uint64_t /*height*/, uint64_t /*solve_time*/, int /*hash_entropy*/,
    double /*diff_change_pct*/, uint64_t /*block_size*/, uint32_t /*tx_count*/,
    double /*ml_risk*/, double /*ml_confidence*/)
{
    AnalysisResult r;
    r.success = false;
    r.error = "LLM engine not available (stub build)";
    return r;
}

AnalysisResult NinaLLMEngine::explain_learning_anomaly(
    const std::string& /*metric_name*/, double /*current_value*/, double /*avg_value*/,
    double /*std_dev*/, double /*min_val*/, double /*max_val*/, uint64_t /*sample_count*/,
    double /*deviation_sigma*/)
{
    AnalysisResult r;
    r.success = false;
    r.error = "LLM engine not available (stub build)";
    return r;
}

AnalysisResult NinaLLMEngine::generate_suggestion(
    const std::string& /*situation_type*/, const std::string& /*metrics*/,
    const std::string& /*events*/)
{
    AnalysisResult r;
    r.success = false;
    r.error = "LLM engine not available (stub build)";
    return r;
}

AnalysisResult NinaLLMEngine::analyze_memory_patterns(
    const std::string& /*patterns_data*/, const std::string& /*peers_data*/)
{
    AnalysisResult r;
    r.success = false;
    r.error = "LLM engine not available (stub build)";
    return r;
}

} // namespace ninacatcoin_ai
