// Copyright (c) 2026, The ninacatcoin Project
//
// NINA IA Auto-Update Orchestrator — Implementation
// NINA IA autonomously monitors GitHub, analyzes changes, detects forks,
// and updates the node. She is the intelligent decision-maker.

#include "nina_ia_auto_update.hpp"
#include "ai_version_checker.hpp"
#include "ai_auto_updater.hpp"
#include "misc_log_ex.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <array>
#include <filesystem>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <limits.h>
#endif

#undef ninacatcoin_DEFAULT_LOG_CATEGORY
#define ninacatcoin_DEFAULT_LOG_CATEGORY "nina.ia.update"

namespace ninacatcoin_ai {

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────
static NinaIAAutoUpdate* g_nina_updater = nullptr;

NinaIAAutoUpdate& NinaIAAutoUpdate::getInstance() {
    if (!g_nina_updater) {
        g_nina_updater = new NinaIAAutoUpdate();
    }
    return *g_nina_updater;
}

NinaIAAutoUpdate::NinaIAAutoUpdate() {
    ninaLog("NINA IA Auto-Update: instancia creada, esperando inicializacion");
}

NinaIAAutoUpdate::~NinaIAAutoUpdate() {
    stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Logging — all messages branded as NINA IA
// ─────────────────────────────────────────────────────────────────────────────
void NinaIAAutoUpdate::ninaLog(const std::string& message, int severity) {
    std::string prefix;
    switch (severity) {
        case 1: prefix = "[NINA IA 🐱] "; break;
        case 2: prefix = "[NINA IA 🐱 INFO] "; break;
        case 3: prefix = "[NINA IA 🐱 ⚠️] "; break;
        case 4: prefix = "[NINA IA 🐱 🔴] "; break;
        case 5: prefix = "[NINA IA 🐱 🚨 CRITICAL] "; break;
        default: prefix = "[NINA IA 🐱] "; break;
    }

    std::string full_msg = prefix + message;

    if (severity >= 4) {
        MERROR(full_msg);
    } else if (severity >= 3) {
        MWARNING(full_msg);
    } else {
        MINFO(full_msg);
    }

    // Update status
    {
        std::lock_guard<std::mutex> lock(m_status_mutex);
        m_status = full_msg;
    }

    // Notify callback (Discord, etc.)
    if (m_notify_callback && severity >= 2) {
        try {
            m_notify_callback("NINA IA Auto-Update", message, severity);
        } catch (...) {}
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialization
// ─────────────────────────────────────────────────────────────────────────────
void NinaIAAutoUpdate::initialize(const std::string& local_version,
                                   uint64_t current_height,
                                   bool is_syncing) {
    m_local_version = local_version;
    m_current_height.store(current_height);
    m_is_syncing.store(is_syncing);
    m_initialized.store(true);

    ninaLog("Inicializada — version local: v" + local_version
            + " | altura: " + std::to_string(current_height)
            + " | sincronizando: " + (is_syncing ? "SI" : "NO"), 2);
}

void NinaIAAutoUpdate::updateBlockchainState(uint64_t current_height, bool is_syncing) {
    m_current_height.store(current_height);
    m_is_syncing.store(is_syncing);

    // Check if we're in urgent mode and approaching a fork
    uint64_t fork_h = m_next_fork_height.load();
    if (fork_h > 0 && current_height > 0) {
        int64_t blocks_left = static_cast<int64_t>(fork_h) - static_cast<int64_t>(current_height);
        if (blocks_left > 0 && blocks_left <= PRE_FORK_BLOCKS_MARGIN * 2) {
            if (!m_urgent_mode.load()) {
                m_urgent_mode.store(true);
                ninaLog("🚨 MODO URGENTE: fork detectado en altura " + std::to_string(fork_h)
                        + " — faltan " + std::to_string(blocks_left) + " bloques!", 4);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Start / Stop
// ─────────────────────────────────────────────────────────────────────────────
void NinaIAAutoUpdate::start() {
    if (!m_initialized.load()) {
        ninaLog("No puedo iniciar — falta inicializacion", 3);
        return;
    }

    if (m_running.load()) {
        ninaLog("Ya estoy monitorizando GitHub", 2);
        return;
    }

    m_running.store(true);
    m_monitor_thread = std::thread(&NinaIAAutoUpdate::monitorLoop, this);
    m_monitor_thread.detach();

    ninaLog("╔══════════════════════════════════════════════════════════╗", 2);
    ninaLog("║  🐱 NINA IA AUTO-UPDATE SYSTEM ACTIVATED               ║", 2);
    ninaLog("║                                                          ║", 2);
    ninaLog("║  Yo soy NINA. Monitorizo GitHub continuamente.          ║", 2);
    ninaLog("║  Si hay una nueva version o un fork proximo,            ║", 2);
    ninaLog("║  descargo, compilo e instalo automaticamente.           ║", 2);
    ninaLog("║                                                          ║", 2);
    ninaLog("║  Intervalo normal:  cada 6 horas                        ║", 2);
    ninaLog("║  Modo urgente:      cada 30 minutos (pre-fork)          ║", 2);
    ninaLog("║  Fuente:            github.com/ninacatcoin/ninacatcoin  ║", 2);
    ninaLog("╚══════════════════════════════════════════════════════════╝", 2);
}

void NinaIAAutoUpdate::stop() {
    if (m_running.load()) {
        m_running.store(false);
        ninaLog("Deteniendo monitorizacion de GitHub...", 2);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// HTTP GET (via curl)
// ─────────────────────────────────────────────────────────────────────────────
std::string NinaIAAutoUpdate::httpGet(const std::string& url, int timeout_secs) {
    std::string cmd = "curl -sS -L --max-time " + std::to_string(timeout_secs)
                    + " -H \"User-Agent: ninacatcoin-nina-ia/" + m_local_version + "\""
                    + " -H \"Accept: application/vnd.github.v3+json\""
                    + " \"" + url + "\" 2>/dev/null";

    std::string result;
    std::array<char, 8192> buffer;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int ret = pclose(pipe);
    return (ret == 0) ? result : "";
}

// ─────────────────────────────────────────────────────────────────────────────
// Local file reading
// ─────────────────────────────────────────────────────────────────────────────
std::string NinaIAAutoUpdate::readLocalFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::string NinaIAAutoUpdate::getLocalConfigPath() const {
    // Determine path relative to the running binary
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string bin_dir = std::filesystem::path(path).parent_path().parent_path().string();
#else
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    std::string bin_dir;
    if (len != -1) {
        path[len] = '\0';
        bin_dir = std::filesystem::path(path).parent_path().parent_path().string();
    } else {
        bin_dir = ".";
    }
#endif
    // Try common locations
    std::vector<std::string> candidates = {
        bin_dir + "/../src/cryptonote_config.h",
        bin_dir + "/../../src/cryptonote_config.h",
        "/usr/local/src/ninacatcoin/src/cryptonote_config.h",
        "src/cryptonote_config.h"
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    return "";
}

std::string NinaIAAutoUpdate::getLocalHardforksPath() const {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string bin_dir = std::filesystem::path(path).parent_path().parent_path().string();
#else
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    std::string bin_dir;
    if (len != -1) {
        path[len] = '\0';
        bin_dir = std::filesystem::path(path).parent_path().parent_path().string();
    } else {
        bin_dir = ".";
    }
#endif
    std::vector<std::string> candidates = {
        bin_dir + "/../src/hardforks/hardforks.cpp",
        bin_dir + "/../../src/hardforks/hardforks.cpp",
        "src/hardforks/hardforks.cpp"
    };
    for (const auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    return "";
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse #define values from config header
// ─────────────────────────────────────────────────────────────────────────────
std::map<std::string, std::string> NinaIAAutoUpdate::parseDefines(const std::string& content) {
    std::map<std::string, std::string> defines;
    std::istringstream stream(content);
    std::string line;

    // Match: #define NAME VALUE  (with optional comments)
    std::regex define_re(R"(^\s*#define\s+(\w+)\s+(.+?)(?:\s*//.*)?$)");

    while (std::getline(stream, line)) {
        // FIX: strip \r from Windows/NTFS line endings (critical for WSL mounts)
        // Without this, regex_match fails on ALL lines when reading from /mnt/ paths,
        // causing parseDefines to return an empty map and false-positive "NUEVO" diffs.
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        std::smatch match;
        if (std::regex_match(line, match, define_re)) {
            std::string name = match[1].str();
            std::string value = match[2].str();
            // Trim trailing whitespace (including \r for safety)
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
                value.pop_back();
            defines[name] = value;
        }
    }
    return defines;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse hard fork table: { version, height, ... }
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::pair<int, uint64_t>> NinaIAAutoUpdate::parseHardForkTable(
    const std::string& content) {
    std::vector<std::pair<int, uint64_t>> forks;

    // Match lines like: { 7, 1, 0, 1341378000 },
    // or { version, height } patterns
    std::regex hf_re(R"(\{\s*(\d+)\s*,\s*(\d+)\s*,)");
    std::sregex_iterator it(content.begin(), content.end(), hf_re);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        try {
            int version = std::stoi((*it)[1].str());
            uint64_t height = std::stoull((*it)[2].str());
            forks.push_back({version, height});
        } catch (...) {}
    }
    return forks;
}

// ─────────────────────────────────────────────────────────────────────────────
// Classify a single config change into ChangeType
// ─────────────────────────────────────────────────────────────────────────────
DetectedChange NinaIAAutoUpdate::classifyChange(const std::string& define_name,
                                                  const std::string& old_value,
                                                  const std::string& new_value) {
    DetectedChange change;
    change.file = "cryptonote_config.h";
    change.fork_height = 0;
    change.severity = 2;

    // Hard fork related defines
    if (define_name.find("FORK") != std::string::npos ||
        define_name.find("HF_VERSION") != std::string::npos ||
        define_name.find("HARD_FORK") != std::string::npos) {
        change.type = ChangeType::HARD_FORK;
        change.severity = 5;
        change.description = "HARD FORK: " + define_name + " cambiado de "
                            + old_value + " a " + new_value;
        // Try to extract height
        try {
            uint64_t h = std::stoull(new_value);
            if (h > 1000) change.fork_height = h; // Likely a height
        } catch (...) {}
        return change;
    }

    // Ring size changes (soft fork / consensus)
    if (define_name.find("RING") != std::string::npos ||
        define_name.find("MIXIN") != std::string::npos ||
        define_name.find("MIN_MIXIN") != std::string::npos) {
        change.type = ChangeType::SOFT_FORK;
        change.severity = 4;
        change.description = "CONSENSO: " + define_name + " cambiado de "
                            + old_value + " a " + new_value;
        return change;
    }

    // Difficulty changes
    if (define_name.find("DIFFICULTY") != std::string::npos ||
        define_name.find("LWMA") != std::string::npos ||
        define_name.find("NINA_LOCAL") != std::string::npos) {
        change.type = ChangeType::SOFT_FORK;
        change.severity = 4;
        change.description = "DIFICULTAD: " + define_name + " cambiado de "
                            + old_value + " a " + new_value;
        return change;
    }

    // Fee changes
    if (define_name.find("FEE") != std::string::npos) {
        change.type = ChangeType::SOFT_FORK;
        change.severity = 3;
        change.description = "COMISIONES: " + define_name + " cambiado de "
                            + old_value + " a " + new_value;
        return change;
    }

    // Emission / reward changes
    if (define_name.find("REWARD") != std::string::npos ||
        define_name.find("EMISSION") != std::string::npos ||
        define_name.find("MONEY_SUPPLY") != std::string::npos ||
        define_name.find("HALVING") != std::string::npos) {
        change.type = ChangeType::HARD_FORK;
        change.severity = 5;
        change.description = "EMISION: " + define_name + " cambiado de "
                            + old_value + " a " + new_value;
        return change;
    }

    // Network / P2P changes
    if (define_name.find("P2P") != std::string::npos ||
        define_name.find("NETWORK") != std::string::npos ||
        define_name.find("SEED") != std::string::npos) {
        change.type = ChangeType::CONFIG_CHANGE;
        change.severity = 2;
        change.description = "RED: " + define_name + " cambiado de "
                            + old_value + " a " + new_value;
        return change;
    }

    // Default: generic config change
    change.type = ChangeType::CONFIG_CHANGE;
    change.severity = 1;
    change.description = "CONFIG: " + define_name + " cambiado de "
                        + old_value + " a " + new_value;
    return change;
}

// ─────────────────────────────────────────────────────────────────────────────
// Diff config files: find all changed #defines
// ─────────────────────────────────────────────────────────────────────────────
std::vector<DetectedChange> NinaIAAutoUpdate::diffConfigFiles(
    const std::string& local_content,
    const std::string& remote_content) {

    std::vector<DetectedChange> changes;

    auto local_defines = parseDefines(local_content);
    auto remote_defines = parseDefines(remote_content);

    // Sanity check: if local parsed 0 defines but content is non-empty, likely a parsing bug
    if (local_defines.empty() && local_content.size() > 100) {
        ninaLog("WARNING: parseDefines devolvio 0 defines del archivo local ("
                + std::to_string(local_content.size()) + " bytes) — posible bug de parseo", 3);
        return changes; // Don't report false positives
    }
    if (remote_defines.empty() && remote_content.size() > 100) {
        ninaLog("WARNING: parseDefines devolvio 0 defines del archivo remoto — posible error de descarga", 3);
        return changes;
    }

    // Find changed or new defines
    for (const auto& [name, remote_val] : remote_defines) {
        auto it = local_defines.find(name);
        if (it == local_defines.end()) {
            // New define — added in remote
            DetectedChange c;
            c.type = ChangeType::FEATURE;
            c.file = "cryptonote_config.h";
            c.severity = 3;
            c.fork_height = 0;
            c.description = "NUEVO: #define " + name + " " + remote_val;

            // Check if it's a fork-related new define
            if (name.find("FORK") != std::string::npos || name.find("HF_") != std::string::npos) {
                c.type = ChangeType::HARD_FORK;
                c.severity = 5;
                try {
                    uint64_t h = std::stoull(remote_val);
                    // Only treat as fork height if value looks like a block height (> 1000)
                    // AND the define name contains HEIGHT (not just VERSION numbers like HF_VERSION_* = 15)
                    if (h > 1000 && name.find("HEIGHT") != std::string::npos) {
                        c.fork_height = h;
                    }
                } catch (...) {}
            }
            changes.push_back(c);
        } else if (it->second != remote_val) {
            // Changed define
            changes.push_back(classifyChange(name, it->second, remote_val));
        }
    }

    // Find removed defines
    for (const auto& [name, local_val] : local_defines) {
        if (remote_defines.find(name) == remote_defines.end()) {
            DetectedChange c;
            c.type = ChangeType::CONFIG_CHANGE;
            c.file = "cryptonote_config.h";
            c.severity = 2;
            c.fork_height = 0;
            c.description = "ELIMINADO: #define " + name + " (era: " + local_val + ")";
            changes.push_back(c);
        }
    }

    return changes;
}

// ─────────────────────────────────────────────────────────────────────────────
// Analyze remote cryptonote_config.h
// ─────────────────────────────────────────────────────────────────────────────
std::vector<DetectedChange> NinaIAAutoUpdate::analyzeRemoteConfig() {
    ninaLog("Analizando cryptonote_config.h remoto...", 1);

    // Fetch remote config
    std::string remote_config = httpGet(GITHUB_RAW_CONFIG, 30);
    if (remote_config.empty()) {
        ninaLog("No pude descargar config remoto — reintentare mas tarde", 3);
        return {};
    }

    // Read local config
    std::string local_path = getLocalConfigPath();
    std::string local_config;
    if (!local_path.empty()) {
        local_config = readLocalFile(local_path);
    }

    if (local_config.empty()) {
        ninaLog("No encontre config local — comparacion basada solo en defines conocidos", 3);
        // Use what we know from compile-time constants
        // This is a fallback; the config comparison will still work
        // but won't detect removed defines
        return {};
    }

    auto changes = diffConfigFiles(local_config, remote_config);

    if (changes.empty()) {
        ninaLog("cryptonote_config.h: sin cambios respecto a GitHub", 1);
    } else {
        ninaLog("Detectados " + std::to_string(changes.size())
                + " cambios en cryptonote_config.h:", 2);
        for (const auto& c : changes) {
            ninaLog("  → " + c.description, c.severity);
        }
    }

    return changes;
}

// ─────────────────────────────────────────────────────────────────────────────
// Analyze remote hardforks.cpp
// ─────────────────────────────────────────────────────────────────────────────
std::vector<DetectedChange> NinaIAAutoUpdate::analyzeRemoteHardforks() {
    ninaLog("Analizando hardforks.cpp remoto...", 1);

    std::string remote_hf = httpGet(GITHUB_RAW_HF, 30);
    if (remote_hf.empty()) {
        ninaLog("No pude descargar hardforks.cpp remoto", 3);
        return {};
    }

    std::string local_path = getLocalHardforksPath();
    std::string local_hf;
    if (!local_path.empty()) {
        local_hf = readLocalFile(local_path);
    }

    if (local_hf.empty()) {
        ninaLog("No encontre hardforks.cpp local — analisis parcial", 3);
        return {};
    }

    auto local_forks = parseHardForkTable(local_hf);
    auto remote_forks = parseHardForkTable(remote_hf);

    std::vector<DetectedChange> changes;

    // Check for new hard forks in remote that don't exist locally
    for (const auto& [rv, rh] : remote_forks) {
        bool found = false;
        for (const auto& [lv, lh] : local_forks) {
            if (rv == lv && rh == lh) { found = true; break; }
        }
        if (!found) {
            DetectedChange c;
            c.type = ChangeType::HARD_FORK;
            c.file = "hardforks.cpp";
            c.severity = 5;
            c.fork_height = rh;
            c.description = "🚨 NUEVO HARD FORK: version " + std::to_string(rv)
                          + " en altura " + std::to_string(rh);
            changes.push_back(c);

            ninaLog("🚨 DETECTADO NUEVO HARD FORK: v" + std::to_string(rv)
                    + " @ altura " + std::to_string(rh), 5);
        }
    }

    // Check for changed heights (same version, different height)
    for (const auto& [rv, rh] : remote_forks) {
        for (const auto& [lv, lh] : local_forks) {
            if (rv == lv && rh != lh) {
                DetectedChange c;
                c.type = ChangeType::HARD_FORK;
                c.file = "hardforks.cpp";
                c.severity = 5;
                c.fork_height = rh;
                c.description = "HARD FORK MODIFICADO: v" + std::to_string(rv)
                              + " altura cambiada de " + std::to_string(lh)
                              + " a " + std::to_string(rh);
                changes.push_back(c);
            }
        }
    }

    if (changes.empty()) {
        ninaLog("hardforks.cpp: sin nuevos forks detectados", 1);
    }

    return changes;
}

// ─────────────────────────────────────────────────────────────────────────────
// Calculate blocks/time until a fork
// ─────────────────────────────────────────────────────────────────────────────
int64_t NinaIAAutoUpdate::blocksUntilFork(uint64_t fork_height) const {
    uint64_t current = m_current_height.load();
    if (current == 0 || fork_height == 0) return -1;
    return static_cast<int64_t>(fork_height) - static_cast<int64_t>(current);
}

int64_t NinaIAAutoUpdate::secondsUntilFork(uint64_t fork_height) const {
    int64_t blocks = blocksUntilFork(fork_height);
    if (blocks < 0) return -1;
    return blocks * 120; // DIFFICULTY_TARGET = 120 seconds
}

// ─────────────────────────────────────────────────────────────────────────────
// Make intelligent update decision
// ─────────────────────────────────────────────────────────────────────────────
UpdatePlan NinaIAAutoUpdate::makeDecision(const std::string& remote_version,
                                           const std::string& remote_tag,
                                           const std::vector<DetectedChange>& changes) {
    UpdatePlan plan;
    plan.remote_version = remote_version;
    plan.remote_tag = remote_tag;
    plan.changes = changes;
    plan.scheduled_time = 0;
    plan.fork_deadline_height = 0;
    plan.urgency = 1;

    if (changes.empty()) {
        plan.decision = UpdateDecision::UPDATE_OPTIONAL;
        plan.reason = "Nueva version disponible sin cambios de consenso — actualizacion opcional";
        plan.urgency = 2;
        return plan;
    }

    // Scan for the most critical change
    int max_severity = 0;
    bool has_hard_fork = false;
    bool has_soft_fork = false;
    bool has_security = false;
    uint64_t earliest_fork_height = UINT64_MAX;

    for (const auto& c : changes) {
        max_severity = std::max(max_severity, c.severity);
        if (c.type == ChangeType::HARD_FORK) {
            has_hard_fork = true;
            if (c.fork_height > 0 && c.fork_height < earliest_fork_height) {
                earliest_fork_height = c.fork_height;
            }
        }
        if (c.type == ChangeType::SOFT_FORK) has_soft_fork = true;
        if (c.type == ChangeType::SECURITY_FIX) has_security = true;
    }

    // ── Decision logic ──

    // Case 1: Hard fork detected with known height
    if (has_hard_fork && earliest_fork_height < UINT64_MAX) {
        plan.fork_deadline_height = earliest_fork_height;
        int64_t blocks_left = blocksUntilFork(earliest_fork_height);

        if (blocks_left < 0) {
            // Fork already passed — we MUST update NOW
            plan.decision = UpdateDecision::UPDATE_NOW;
            plan.reason = "🚨 HARD FORK ya paso en altura " + std::to_string(earliest_fork_height)
                        + " — ACTUALIZACION CRITICA INMEDIATA";
            plan.urgency = 10;
        } else if (blocks_left <= PRE_FORK_BLOCKS_MARGIN) {
            // Fork very close — update NOW even if syncing
            plan.decision = UpdateDecision::UPDATE_NOW;
            plan.reason = "🚨 HARD FORK en " + std::to_string(blocks_left)
                        + " bloques — ACTUALIZACION URGENTE";
            plan.urgency = 9;
        } else if (blocks_left <= PRE_FORK_BLOCKS_MARGIN * 4) {
            // Fork approaching — update soon
            plan.decision = UpdateDecision::UPDATE_BEFORE_FORK;
            int64_t hours = secondsUntilFork(earliest_fork_height) / 3600;
            plan.reason = "Hard fork en ~" + std::to_string(hours)
                        + " horas (" + std::to_string(blocks_left)
                        + " bloques) — actualizare pronto";
            plan.urgency = 7;

            // Activate urgent mode
            m_next_fork_height.store(earliest_fork_height);
            m_urgent_mode.store(true);
        } else {
            // Fork far away — schedule update
            plan.decision = UpdateDecision::UPDATE_SCHEDULED;
            int64_t days = secondsUntilFork(earliest_fork_height) / 86400;
            plan.reason = "Hard fork en ~" + std::to_string(days)
                        + " dias — programare actualizacion";
            plan.urgency = 5;

            m_next_fork_height.store(earliest_fork_height);
        }
        return plan;
    }

    // Case 2: Hard fork detected but no specific height
    if (has_hard_fork) {
        plan.decision = UpdateDecision::UPDATE_NOW;
        plan.reason = "Cambio de hard fork detectado — actualizacion inmediata recomendada";
        plan.urgency = 8;
        return plan;
    }

    // Case 3: Security fix
    if (has_security) {
        plan.decision = UpdateDecision::UPDATE_NOW;
        plan.reason = "Correccion de seguridad detectada — actualizacion inmediata";
        plan.urgency = 8;
        return plan;
    }

    // Case 4: Soft fork / consensus change
    if (has_soft_fork) {
        if (m_is_syncing.load()) {
            plan.decision = UpdateDecision::UPDATE_DEFERRED;
            plan.reason = "Cambio de consenso detectado — esperando fin de sincronizacion";
            plan.urgency = 5;
        } else {
            plan.decision = UpdateDecision::UPDATE_SCHEDULED;
            plan.reason = "Cambio de consenso — programare actualizacion";
            plan.urgency = 5;
        }
        return plan;
    }

    // Case 5: Normal update (features, config, performance)
    if (m_is_syncing.load()) {
        plan.decision = UpdateDecision::UPDATE_DEFERRED;
        plan.reason = "Actualizacion disponible — esperando fin de sincronizacion";
        plan.urgency = 2;
    } else {
        plan.decision = UpdateDecision::UPDATE_OPTIONAL;
        plan.reason = "Actualizacion disponible (mejoras menores)";
        plan.urgency = 2;
    }
    return plan;
}

// ─────────────────────────────────────────────────────────────────────────────
// Execute update
// ─────────────────────────────────────────────────────────────────────────────
bool NinaIAAutoUpdate::executeUpdate(const UpdatePlan& plan) {
    ninaLog("═══════════════════════════════════════════════════════════", 2);
    ninaLog("EJECUTANDO ACTUALIZACION a v" + plan.remote_version, 2);
    ninaLog("Razon: " + plan.reason, 2);
    ninaLog("Urgencia: " + std::to_string(plan.urgency) + "/10", 2);

    if (!plan.changes.empty()) {
        ninaLog("Cambios detectados (" + std::to_string(plan.changes.size()) + "):", 2);
        for (const auto& c : plan.changes) {
            std::string type_str;
            switch (c.type) {
                case ChangeType::HARD_FORK:    type_str = "HARD_FORK"; break;
                case ChangeType::SOFT_FORK:    type_str = "SOFT_FORK"; break;
                case ChangeType::SECURITY_FIX: type_str = "SECURITY"; break;
                case ChangeType::PERFORMANCE:  type_str = "PERFORMANCE"; break;
                case ChangeType::FEATURE:      type_str = "FEATURE"; break;
                case ChangeType::CONFIG_CHANGE:type_str = "CONFIG"; break;
                default:                       type_str = "UNKNOWN"; break;
            }
            ninaLog("  [" + type_str + "] " + c.description, c.severity);
        }
    }
    ninaLog("═══════════════════════════════════════════════════════════", 2);

    // Delegate to AutoUpdater for actual clone/compile/install
    auto& updater = AutoUpdater::getInstance();
    if (updater.isUpdating()) {
        ninaLog("AutoUpdater ya esta ejecutando — espero...", 3);
        return false;
    }

    ninaLog("Delegando a AutoUpdater: git clone → cmake → make → install → restart", 2);

    // Pass empty hash for version-based updates (trust GitHub)
    bool success = updater.performUpdate("");

    // Record in history
    UpdateRecord record;
    record.timestamp = static_cast<uint64_t>(std::time(nullptr));
    record.from_version = m_local_version;
    record.to_version = plan.remote_version;
    record.success = success;
    record.changes = plan.changes;
    if (!success) {
        record.error_msg = updater.getStatus();
    }
    recordUpdate(record);

    if (success) {
        ninaLog("✅ ACTUALIZACION EXITOSA a v" + plan.remote_version
                + " — daemon reiniciando...", 2);
    } else {
        ninaLog("❌ ACTUALIZACION FALLIDA — " + updater.getStatus(), 4);
        ninaLog("Accion manual: git pull && cd build-linux && cmake .. && make -j2 daemon", 3);
    }

    return success;
}

// ─────────────────────────────────────────────────────────────────────────────
// Force check (for RPC or manual trigger)
// ─────────────────────────────────────────────────────────────────────────────
UpdatePlan NinaIAAutoUpdate::forceCheck() {
    ninaLog("Verificacion forzada solicitada", 2);

    // Step 1: Analyze config changes independently
    auto config_changes = analyzeRemoteConfig();
    auto hf_changes = analyzeRemoteHardforks();

    // Merge all changes
    std::vector<DetectedChange> all_changes;
    all_changes.insert(all_changes.end(), config_changes.begin(), config_changes.end());
    all_changes.insert(all_changes.end(), hf_changes.begin(), hf_changes.end());

    // Get remote version info
    std::string remote_ver = "unknown";
    std::string remote_tag = "";
    // We could query VersionChecker but it does its own full check
    // For now, use the tag from GitHub API
    std::string release_json = httpGet(
        "https://api.github.com/repos/ninacatcoin/ninacatcoin/releases/latest", 15);
    if (!release_json.empty()) {
        // Quick parse tag_name
        size_t pos = release_json.find("\"tag_name\"");
        if (pos != std::string::npos) {
            pos = release_json.find('"', pos + 11);
            if (pos != std::string::npos) {
                size_t end = release_json.find('"', pos + 1);
                if (end != std::string::npos) {
                    remote_tag = release_json.substr(pos + 1, end - pos - 1);
                    remote_ver = remote_tag;
                }
            }
        }
    }

    // Cache the analysis
    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        m_last_analysis_time = static_cast<uint64_t>(std::time(nullptr));
        m_cached_changes = all_changes;
        m_cached_remote_version = remote_ver;
    }

    // If no changes and no new version → nothing to do
    if (all_changes.empty() && remote_ver == m_local_version) {
        UpdatePlan noop;
        noop.decision = UpdateDecision::UPDATE_SKIP;
        noop.reason = "No hay cambios — todo actualizado";
        noop.urgency = 0;
        ninaLog("✅ Todo al dia — sin actualizaciones pendientes", 1);
        return noop;
    }

    // Make decision
    auto plan = makeDecision(remote_ver, remote_tag, all_changes);

    ninaLog("Decision: " + plan.reason + " (urgencia: "
            + std::to_string(plan.urgency) + "/10)", 2);

    // Auto-execute for critical updates
    if (plan.decision == UpdateDecision::UPDATE_NOW) {
        ninaLog("Ejecutando actualizacion inmediata...", 2);
        executeUpdate(plan);
    } else if (plan.decision == UpdateDecision::UPDATE_BEFORE_FORK) {
        ninaLog("Programando actualizacion pre-fork...", 2);
        executeUpdate(plan);
    } else if (plan.decision == UpdateDecision::UPDATE_SCHEDULED && !m_is_syncing.load()) {
        ninaLog("Ejecutando actualizacion programada...", 2);
        executeUpdate(plan);
    }

    return plan;
}

// ─────────────────────────────────────────────────────────────────────────────
// Status report
// ─────────────────────────────────────────────────────────────────────────────
std::string NinaIAAutoUpdate::getStatusReport() const {
    std::ostringstream ss;
    ss << "╔══════════════════════════════════════════════════════════╗\n";
    ss << "║       🐱 NINA IA — Auto-Update Status Report           ║\n";
    ss << "╠══════════════════════════════════════════════════════════╣\n";
    ss << "║ Version local:    v" << m_local_version << "\n";
    ss << "║ Altura actual:    " << m_current_height.load() << "\n";
    ss << "║ Sincronizando:    " << (m_is_syncing.load() ? "SI" : "NO") << "\n";
    ss << "║ Modo urgente:     " << (m_urgent_mode.load() ? "SI" : "NO") << "\n";

    uint64_t fork_h = m_next_fork_height.load();
    if (fork_h > 0) {
        int64_t blocks = blocksUntilFork(fork_h);
        int64_t hours = secondsUntilFork(fork_h) / 3600;
        ss << "║ Proximo fork:     altura " << fork_h
           << " (en ~" << blocks << " bloques, ~" << hours << "h)\n";
    } else {
        ss << "║ Proximo fork:     ninguno detectado\n";
    }

    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        if (m_last_analysis_time > 0) {
            uint64_t ago = static_cast<uint64_t>(std::time(nullptr)) - m_last_analysis_time;
            ss << "║ Ultima revision:  hace " << (ago / 60) << " minutos\n";
            ss << "║ Cambios pend.:    " << m_cached_changes.size() << "\n";
            if (!m_cached_remote_version.empty()) {
                ss << "║ Version remota:   " << m_cached_remote_version << "\n";
            }
        } else {
            ss << "║ Ultima revision:  pendiente\n";
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_history_mutex);
        ss << "║ Actualizaciones:  " << m_update_history.size() << " en historial\n";
        if (!m_update_history.empty()) {
            const auto& last = m_update_history.back();
            ss << "║ Ultima:           v" << last.from_version << " → v" << last.to_version
               << " (" << (last.success ? "OK" : "FALLO") << ")\n";
        }
    }

    ss << "║ Monitorizando:    " << (m_running.load() ? "ACTIVO" : "DETENIDO") << "\n";
    ss << "╚══════════════════════════════════════════════════════════╝";
    return ss.str();
}

// ─────────────────────────────────────────────────────────────────────────────
// Update history
// ─────────────────────────────────────────────────────────────────────────────
std::vector<UpdateRecord> NinaIAAutoUpdate::getUpdateHistory() const {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    return m_update_history;
}

void NinaIAAutoUpdate::recordUpdate(const UpdateRecord& record) {
    std::lock_guard<std::mutex> lock(m_history_mutex);
    m_update_history.push_back(record);

    // Keep last 50 records
    if (m_update_history.size() > 50) {
        m_update_history.erase(m_update_history.begin());
    }
}

void NinaIAAutoUpdate::setNotifyCallback(NotifyCallback cb) {
    m_notify_callback = std::move(cb);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main monitoring loop — NINA IA's autonomous brain
// ─────────────────────────────────────────────────────────────────────────────
void NinaIAAutoUpdate::monitorLoop() {
    ninaLog("Esperando " + std::to_string(STARTUP_DELAY_SECS)
            + " segundos para inicializacion completa del daemon...", 1);

    // Wait for daemon to fully start
    for (int i = 0; i < STARTUP_DELAY_SECS && m_running.load(); i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    ninaLog("Comenzando monitorizacion autonoma de GitHub...", 2);

    while (m_running.load()) {
        try {
            ninaLog("═══ Ciclo de verificacion iniciado ═══", 1);

            // If syncing and not urgent, wait
            if (m_is_syncing.load() && !m_urgent_mode.load()) {
                ninaLog("Nodo sincronizando — esperare (excepto si hay fork urgente)", 1);

                // Wait up to MAX_SYNC_WAIT_SECS, then check anyway
                for (int i = 0; i < MAX_SYNC_WAIT_SECS && m_running.load() && m_is_syncing.load(); i++) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                if (m_is_syncing.load()) {
                    ninaLog("Timeout de espera de sync — verificando de todos modos", 2);
                }
            }

            // Step 1: Analyze remote config for changes
            auto config_changes = analyzeRemoteConfig();

            // Step 2: Analyze remote hardforks for new forks
            auto hf_changes = analyzeRemoteHardforks();

            // Merge changes
            std::vector<DetectedChange> all_changes;
            all_changes.insert(all_changes.end(), config_changes.begin(), config_changes.end());
            all_changes.insert(all_changes.end(), hf_changes.begin(), hf_changes.end());

            // Cache
            {
                std::lock_guard<std::mutex> lock(m_cache_mutex);
                m_last_analysis_time = static_cast<uint64_t>(std::time(nullptr));
                m_cached_changes = all_changes;
            }

            // Step 3: Check version via GitHub API
            std::string release_json = httpGet(
                "https://api.github.com/repos/ninacatcoin/ninacatcoin/releases/latest", 15);
            std::string remote_ver = m_local_version;
            std::string remote_tag;

            if (!release_json.empty()) {
                size_t pos = release_json.find("\"tag_name\"");
                if (pos != std::string::npos) {
                    pos = release_json.find('"', pos + 11);
                    if (pos != std::string::npos) {
                        size_t end = release_json.find('"', pos + 1);
                        if (end != std::string::npos) {
                            remote_tag = release_json.substr(pos + 1, end - pos - 1);
                            remote_ver = remote_tag;
                        }
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_cache_mutex);
                m_cached_remote_version = remote_ver;
            }

            // Step 4: Make decision
            if (!all_changes.empty() || remote_ver != m_local_version) {
                auto plan = makeDecision(remote_ver, remote_tag, all_changes);

                ninaLog("Decision de NINA IA: " + plan.reason, 2);
                ninaLog("Urgencia: " + std::to_string(plan.urgency) + "/10", 2);

                // Auto-execute based on decision
                switch (plan.decision) {
                    case UpdateDecision::UPDATE_NOW:
                        ninaLog("⚡ ACTUALIZACION INMEDIATA", 4);
                        executeUpdate(plan);
                        break;

                    case UpdateDecision::UPDATE_BEFORE_FORK:
                        ninaLog("📅 ACTUALIZACION PRE-FORK", 3);
                        executeUpdate(plan);
                        break;

                    case UpdateDecision::UPDATE_SCHEDULED:
                        if (!m_is_syncing.load()) {
                            ninaLog("📦 ACTUALIZACION PROGRAMADA — ejecutando ahora", 2);
                            executeUpdate(plan);
                        } else {
                            ninaLog("📦 ACTUALIZACION PROGRAMADA — esperare fin de sync", 2);
                        }
                        break;

                    case UpdateDecision::UPDATE_DEFERRED:
                        ninaLog("⏸️ ACTUALIZACION DIFERIDA — nodo ocupado", 2);
                        break;

                    case UpdateDecision::UPDATE_OPTIONAL:
                        ninaLog("ℹ️ Actualizacion opcional disponible: v" + remote_ver, 1);
                        // For optional updates, still auto-update if not syncing
                        if (!m_is_syncing.load() && !all_changes.empty()) {
                            ninaLog("Instalando actualizacion opcional automaticamente...", 2);
                            executeUpdate(plan);
                        }
                        break;

                    case UpdateDecision::UPDATE_SKIP:
                        ninaLog("✅ Todo al dia — sin cambios necesarios", 1);
                        break;
                }
            } else {
                ninaLog("✅ Sin cambios detectados — version actual al dia", 1);
            }

            ninaLog("═══ Ciclo de verificacion completado ═══", 1);

        } catch (const std::exception& e) {
            ninaLog("Excepcion en ciclo de monitorizacion: " + std::string(e.what()), 4);
        } catch (...) {
            ninaLog("Excepcion desconocida en ciclo de monitorizacion", 4);
        }

        // Sleep until next check — shorter interval in urgent mode
        int sleep_secs = m_urgent_mode.load() ? URGENT_CHECK_INTERVAL : CHECK_INTERVAL_SECS;
        ninaLog("Proxima verificacion en " + std::to_string(sleep_secs / 60) + " minutos"
                + (m_urgent_mode.load() ? " (MODO URGENTE)" : ""), 1);

        for (int i = 0; i < sleep_secs && m_running.load(); i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    ninaLog("Monitorizacion detenida", 2);
}

} // namespace ninacatcoin_ai
