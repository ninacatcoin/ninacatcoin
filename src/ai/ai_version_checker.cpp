// Copyright (c) 2026, The ninacatcoin Project
//
// Version Checker implementation: queries GitHub API for new releases,
// compares with local version, triggers auto-update when newer version found.

#include "ai_version_checker.hpp"
#include "ai_auto_updater.hpp"
#include "version.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <regex>
#include <chrono>
#include <array>
#include <algorithm>
#include <cctype>

namespace ninacatcoin_ai {

// ─────────────────────────────────────────────────────────────────────────────
// Singleton
// ─────────────────────────────────────────────────────────────────────────────
static VersionChecker* g_version_checker = nullptr;

VersionChecker& VersionChecker::getInstance() {
    if (!g_version_checker) {
        g_version_checker = new VersionChecker();
    }
    return *g_version_checker;
}

VersionChecker::VersionChecker() {
    setStatus("Not initialized");
}

VersionChecker::~VersionChecker() {
    stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Status helpers
// ─────────────────────────────────────────────────────────────────────────────
void VersionChecker::setStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    m_status = status;
    std::cout << "[VERSION-CHECK] " << status << std::endl;
}

std::string VersionChecker::getStatus() const {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    return m_status;
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialization
// ─────────────────────────────────────────────────────────────────────────────
void VersionChecker::initialize(const std::string& local_version) {
    m_local_version = parseVersion(local_version);
    m_initialized.store(true);
    setStatus("Initialized — local version: " + m_local_version.toString()
              + " (check interval: 12 hours)");
}

// ─────────────────────────────────────────────────────────────────────────────
// Version parsing: "v0.1.0.0" or "0.1.0.0" → VersionInfo
// ─────────────────────────────────────────────────────────────────────────────
VersionInfo VersionChecker::parseVersion(const std::string& version_str) {
    VersionInfo v;
    v.tag_name = version_str;

    std::string s = version_str;
    // Strip leading 'v' or 'V'
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
        s = s.substr(1);
    }

    // Parse up to 4 dot-separated numbers
    std::istringstream iss(s);
    std::string token;
    int parts[4] = {0, 0, 0, 0};
    int i = 0;
    while (std::getline(iss, token, '.') && i < 4) {
        try {
            // Stop at first non-digit (e.g., "0-rc1")
            size_t pos = 0;
            parts[i] = std::stoi(token, &pos);
            // Check if there's a pre-release suffix
            if (pos < token.size() && (token[pos] == '-' || token[pos] == 'r' || token[pos] == 'a')) {
                v.is_prerelease = true;
            }
        } catch (...) {
            // Non-numeric component, stop parsing
            break;
        }
        i++;
    }

    v.major = parts[0];
    v.minor = parts[1];
    v.patch = parts[2];
    v.build = parts[3];

    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// HTTP GET using curl (available on all Linux systems)
// ─────────────────────────────────────────────────────────────────────────────
std::string VersionChecker::httpGet(const std::string& url, int timeout_secs) {
    // Build curl command
    // -s = silent, -S = show error, -L = follow redirects
    // -H = set User-Agent (required by GitHub API)
    // --max-time = timeout
    std::string cmd = "curl -sS -L --max-time " + std::to_string(timeout_secs)
                    + " -H \"User-Agent: ninacatcoin-daemon/" + m_local_version.toString() + "\""
                    + " -H \"Accept: application/vnd.github.v3+json\""
                    + " \"" + url + "\" 2>/dev/null";

    // Execute and capture output
    std::string result;
    std::array<char, 4096> buffer;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        setStatus("Failed to execute curl command");
        return "";
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int ret = pclose(pipe);
    if (ret != 0) {
        setStatus("HTTP request failed (curl exit code " + std::to_string(ret) + ")");
        return "";
    }

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse GitHub releases API JSON (minimal parser, no external JSON library)
// ─────────────────────────────────────────────────────────────────────────────
VersionInfo VersionChecker::parseReleaseJson(const std::string& json_body) {
    VersionInfo v;

    if (json_body.empty()) return v;

    // Look for "tag_name": "vX.Y.Z.W"
    auto extractJsonString = [&](const std::string& key) -> std::string {
        std::string search_key = "\"" + key + "\"";
        size_t pos = json_body.find(search_key);
        if (pos == std::string::npos) return "";

        // Find the colon after the key
        pos = json_body.find(':', pos + search_key.size());
        if (pos == std::string::npos) return "";

        // Skip whitespace
        pos++;
        while (pos < json_body.size() && (json_body[pos] == ' ' || json_body[pos] == '\t')) pos++;

        if (pos >= json_body.size()) return "";

        // Check for null
        if (json_body.substr(pos, 4) == "null") return "";

        // Expect opening quote
        if (json_body[pos] != '"') return "";
        pos++;

        // Find closing quote
        size_t end = json_body.find('"', pos);
        if (end == std::string::npos) return "";

        return json_body.substr(pos, end - pos);
    };

    auto extractJsonBool = [&](const std::string& key) -> bool {
        std::string search_key = "\"" + key + "\"";
        size_t pos = json_body.find(search_key);
        if (pos == std::string::npos) return false;

        pos = json_body.find(':', pos + search_key.size());
        if (pos == std::string::npos) return false;

        // Look ahead for true/false
        std::string rest = json_body.substr(pos + 1, 10);
        return rest.find("true") != std::string::npos;
    };

    std::string tag_name = extractJsonString("tag_name");
    if (tag_name.empty()) {
        // Might be an error response
        std::string message = extractJsonString("message");
        if (!message.empty()) {
            setStatus("GitHub API error: " + message);
        }
        return v;
    }

    v = parseVersion(tag_name);
    v.tag_name = tag_name;
    v.is_prerelease = extractJsonBool("prerelease");
    v.release_notes = extractJsonString("body");
    v.download_url = extractJsonString("tarball_url");

    // Get target_commitish (the SHA or branch)
    v.commit_sha = extractJsonString("target_commitish");

    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Parse GitHub tags API JSON (fallback when no releases exist)
// ─────────────────────────────────────────────────────────────────────────────
VersionInfo VersionChecker::parseTagJson(const std::string& json_body) {
    VersionInfo best;

    if (json_body.empty() || json_body[0] != '[') return best;

    // Simple approach: find all "name": "vX.Y.Z" entries
    std::regex tag_regex("\"name\"\\s*:\\s*\"(v?[0-9]+\\.[0-9]+[^\"]*?)\"");
    std::sregex_iterator it(json_body.begin(), json_body.end(), tag_regex);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        std::string tag_str = (*it)[1].str();
        VersionInfo candidate = parseVersion(tag_str);
        candidate.tag_name = tag_str;

        // Skip pre-releases
        if (candidate.is_prerelease) continue;

        // Keep the highest version
        if (!best.isValid() || best.isOlderThan(candidate)) {
            best = candidate;
        }
    }

    // Try to extract the SHA for the best tag
    if (best.isValid()) {
        // Look for the sha near this tag's name
        size_t pos = json_body.find("\"" + best.tag_name + "\"");
        if (pos != std::string::npos) {
            // Find "sha": "..." nearby (within next 500 chars)
            std::string nearby = json_body.substr(pos, 500);
            std::regex sha_regex("\"sha\"\\s*:\\s*\"([a-f0-9]{40})\"");
            std::smatch match;
            if (std::regex_search(nearby, match, sha_regex)) {
                best.commit_sha = match[1].str();
            }
        }
    }

    return best;
}

// ─────────────────────────────────────────────────────────────────────────────
// Fetch latest release from GitHub
// ─────────────────────────────────────────────────────────────────────────────
VersionInfo VersionChecker::fetchLatestRelease() {
    setStatus("Querying GitHub releases API...");

    std::string json = httpGet(GITHUB_API_RELEASES, API_TIMEOUT_SECS);
    if (json.empty()) {
        setStatus("No response from GitHub releases API");
        return VersionInfo{};
    }

    VersionInfo v = parseReleaseJson(json);
    if (v.isValid()) {
        setStatus("Latest release: " + v.tag_name + " (" + v.toString() + ")");
    }
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Fetch latest tag from GitHub (fallback)
// ─────────────────────────────────────────────────────────────────────────────
VersionInfo VersionChecker::fetchLatestTag() {
    setStatus("Querying GitHub tags API (fallback)...");

    std::string json = httpGet(GITHUB_API_TAGS, API_TIMEOUT_SECS);
    if (json.empty()) {
        setStatus("No response from GitHub tags API");
        return VersionInfo{};
    }

    VersionInfo v = parseTagJson(json);
    if (v.isValid()) {
        setStatus("Latest tag: " + v.tag_name + " (" + v.toString() + ")");
    }
    return v;
}

// ─────────────────────────────────────────────────────────────────────────────
// Single version check
// ─────────────────────────────────────────────────────────────────────────────
bool VersionChecker::checkForUpdate() {
    if (!m_initialized.load()) {
        setStatus("Cannot check — not initialized");
        return false;
    }

    bool expected = false;
    if (!m_checking.compare_exchange_strong(expected, true)) {
        setStatus("Check already in progress, skipping");
        return false;
    }

    // Ensure m_checking is cleared on exit
    struct CheckGuard {
        std::atomic<bool>& flag;
        ~CheckGuard() { flag.store(false); }
    } guard{m_checking};

    m_last_check_time.store(static_cast<uint64_t>(std::time(nullptr)));

    setStatus("=== Checking for new version ===");
    setStatus("Local version: " + m_local_version.toString());

    // Try releases API first
    VersionInfo remote = fetchLatestRelease();

    // If no release found, try tags
    if (!remote.isValid()) {
        remote = fetchLatestTag();
    }

    if (!remote.isValid()) {
        setStatus("Could not determine latest version from GitHub");
        setStatus("Will retry in " + std::to_string(RETRY_DELAY_SECS / 60) + " minutes");
        return false;
    }

    // Skip pre-releases
    if (remote.is_prerelease) {
        setStatus("Latest release " + remote.tag_name + " is a pre-release — skipping");
        return false;
    }

    // Compare versions
    if (!m_local_version.isOlderThan(remote)) {
        setStatus("✅ Running latest version (" + m_local_version.toString()
                  + ") — no update needed");
        return false;
    }

    // ── New version detected! ──
    setStatus("🆕 NEW VERSION AVAILABLE: " + remote.tag_name
              + " (current: " + m_local_version.toString() + ")");

    if (!remote.release_notes.empty()) {
        // Truncate long release notes
        std::string notes = remote.release_notes;
        if (notes.size() > 200) notes = notes.substr(0, 200) + "...";
        setStatus("Release notes: " + notes);
    }

    // Notify via callback (Discord, etc.)
    if (m_update_callback) {
        try {
            m_update_callback(m_local_version.toString(), remote.toString(), remote.release_notes);
        } catch (const std::exception& e) {
            std::cerr << "[VERSION-CHECK] Callback error: " << e.what() << std::endl;
        }
    }

    // Trigger auto-update via existing AutoUpdater
    setStatus("Initiating auto-update to " + remote.tag_name + "...");

    auto& updater = AutoUpdater::getInstance();
    if (updater.isUpdating()) {
        setStatus("AutoUpdater already running — skipping");
        return false;
    }

    // For version updates, we pass empty expected_hash.
    // The AutoUpdater will clone GitHub master (which now has the new tag),
    // compile, verify the binary, and install it.
    // Since we're pulling from the OFFICIAL repo, this is safe.
    bool success = updater.performUpdate("");

    if (success) {
        setStatus("✅ Update to " + remote.tag_name + " successful — daemon restarting");
    } else {
        setStatus("❌ Auto-update failed — check logs for details");
        setStatus("Manual update: git pull && cd build-linux && cmake .. -DMANUAL_SUBMODULES=1 -DBUILD_TESTS=OFF && make -j2 daemon");
    }

    return success;
}

// ─────────────────────────────────────────────────────────────────────────────
// Callback setter
// ─────────────────────────────────────────────────────────────────────────────
void VersionChecker::setUpdateCallback(UpdateCallback cb) {
    m_update_callback = std::move(cb);
}

// ─────────────────────────────────────────────────────────────────────────────
// Periodic check loop (background thread)
// ─────────────────────────────────────────────────────────────────────────────
void VersionChecker::startPeriodicCheck() {
    if (m_running.load()) {
        setStatus("Periodic checker already running");
        return;
    }

    m_running.store(true);
    m_check_thread = std::thread(&VersionChecker::periodicCheckLoop, this);
    m_check_thread.detach();

    setStatus("Periodic version checker started (every 12 hours)");
}

void VersionChecker::stop() {
    m_running.store(false);
    setStatus("Periodic version checker stopped");
}

void VersionChecker::periodicCheckLoop() {
    // Wait 2 minutes after startup before first check
    // (let the daemon fully initialize and sync first)
    for (int i = 0; i < 120 && m_running.load(); i++) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    while (m_running.load()) {
        try {
            // Perform the check
            checkForUpdate();
        } catch (const std::exception& e) {
            setStatus("Exception during version check: " + std::string(e.what()));
        } catch (...) {
            setStatus("Unknown exception during version check");
        }

        // Sleep for CHECK_INTERVAL_SECS (12 hours), checking every second for stop signal
        for (int i = 0; i < CHECK_INTERVAL_SECS && m_running.load(); i++) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

} // namespace ninacatcoin_ai
