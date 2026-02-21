// Copyright (c) 2026, The ninacatcoin Project
//
// Auto-updater implementation: downloads from GitHub, compiles, installs.

#include "ai_auto_updater.hpp"
#include "ai_integrity_verifier.hpp"
#include "common/util.h"
#include "crypto/hash.h"
#include "string_tools.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <chrono>
#include <thread>
#include <array>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#endif

namespace ninacatcoin_ai {

static AutoUpdater* g_updater = nullptr;

AutoUpdater& AutoUpdater::getInstance() {
    if (!g_updater) {
        g_updater = new AutoUpdater();
    }
    return *g_updater;
}

AutoUpdater::AutoUpdater() {
    setStatus("Idle — no update in progress");
}

void AutoUpdater::setStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    m_status = status;
    std::cout << "[AI AutoUpdater] " << status << std::endl;
}

std::string AutoUpdater::getStatus() const {
    std::lock_guard<std::mutex> lock(m_status_mutex);
    return m_status;
}

bool AutoUpdater::shouldUpdate(const std::string& our_hash,
                               const std::string& consensus_hash,
                               double agreement_pct)
{
    // Don't update if already updating
    if (m_updating.load()) {
        return false;
    }

    // Our hash matches the consensus — no update needed
    if (our_hash == consensus_hash) {
        return false;
    }

    // Need sufficient consensus before auto-updating
    if (agreement_pct < MIN_CONSENSUS_PCT) {
        setStatus("Hash mismatch but consensus too low ("
                  + std::to_string((int)(agreement_pct * 100)) + "%), waiting...");
        return false;
    }

    setStatus("Update recommended: our hash differs from " 
              + std::to_string((int)(agreement_pct * 100)) + "% of network");
    return true;
}

std::string AutoUpdater::getUpdateDir() const {
    const char* home = std::getenv("HOME");
    if (!home) home = std::getenv("USERPROFILE");
    if (!home) home = "/tmp";
    return std::string(home) + "/.ninacatcoin/auto_update";
}

std::string AutoUpdater::getDaemonPath() const {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::string(path);
#else
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len != -1) {
        path[len] = '\0';
        return std::string(path);
    }
    return "/usr/local/bin/ninacatcoind";
#endif
}

int AutoUpdater::runCommand(const std::string& cmd, int timeout_secs) {
    std::string full_cmd = cmd;
    if (timeout_secs > 0) {
        // Use timeout command on Linux
        full_cmd = "timeout " + std::to_string(timeout_secs) + " " + cmd;
    }

    setStatus("Running: " + cmd.substr(0, 80) + (cmd.size() > 80 ? "..." : ""));
    int ret = std::system(full_cmd.c_str());

#ifndef _WIN32
    // On Unix, system() returns encoded status
    if (WIFEXITED(ret)) {
        return WEXITSTATUS(ret);
    }
    return -1;
#else
    return ret;
#endif
}

bool AutoUpdater::performUpdate(const std::string& expected_hash) {
    bool expected = false;
    if (!m_updating.compare_exchange_strong(expected, true)) {
        setStatus("Update already in progress, skipping");
        return false;
    }

    // Ensure cleanup on exit
    struct UpdateGuard {
        std::atomic<bool>& flag;
        ~UpdateGuard() { flag.store(false); }
    } guard{m_updating};

    std::string update_dir = getUpdateDir();
    std::string source_dir = update_dir + "/ninacatcoin";
    std::string build_dir  = update_dir + "/build";

    setStatus("=== Starting auto-update from GitHub ===");
    setStatus("Expected consensus hash: " + expected_hash.substr(0, 16) + "...");

    // Step 1: Download source
    if (!downloadSource(source_dir)) {
        setStatus("❌ Failed to download source from GitHub");
        return false;
    }

    // Step 2: Verify source hash BEFORE compiling (fast check)
    // If expected_hash is empty, this is a full-source remediation from GitHub
    // (local tampering detected) — skip hash comparison, trust GitHub directly
    if (!expected_hash.empty()) {
        std::string source_hash = calculateSourceHash(source_dir);
        if (source_hash.empty()) {
            setStatus("❌ Failed to calculate hash of downloaded source");
            return false;
        }

        setStatus("Downloaded source hash: " + source_hash.substr(0, 16) + "...");

        if (source_hash != expected_hash) {
            setStatus("❌ Source hash does NOT match consensus! Possible supply-chain attack.");
            setStatus("   Expected: " + expected_hash);
            setStatus("   Got:      " + source_hash);
            setStatus("   Update ABORTED for security");
            return false;
        }

        setStatus("✅ Source hash matches network consensus");
    } else {
        setStatus("Full-source remediation mode — trusting GitHub master branch");
    }

    // Step 3: Build
    if (!buildProject(source_dir, build_dir)) {
        setStatus("❌ Compilation failed");
        return false;
    }

    // Step 4: Verify the built binary exists
    std::string new_binary = build_dir + "/bin/ninacatcoind";
    if (!std::filesystem::exists(new_binary)) {
        setStatus("❌ Built binary not found at " + new_binary);
        return false;
    }

    // Step 5: Backup current binary
    std::string current_binary = getDaemonPath();
    std::string backup_path = current_binary + ".backup." 
                              + std::to_string(std::time(nullptr));

    if (!backupCurrentBinary(backup_path)) {
        setStatus("❌ Failed to backup current binary");
        return false;
    }

    // Step 6: Install new binary
    if (!installNewBinary(new_binary, current_binary)) {
        setStatus("❌ Failed to install new binary — restoring backup");
        // Restore backup
        std::filesystem::copy_file(backup_path, current_binary,
            std::filesystem::copy_options::overwrite_existing);
        return false;
    }

    setStatus("✅ New binary installed successfully");
    setStatus("✅ Backup at: " + backup_path);

    // Step 7: Signal restart
    signalRestart();

    return true;
}

bool AutoUpdater::downloadSource(const std::string& target_dir) {
    // Create parent directory
    std::filesystem::create_directories(getUpdateDir());

    if (std::filesystem::exists(target_dir + "/.git")) {
        // Already cloned — just pull latest
        setStatus("Source exists, pulling latest from " + std::string(GITHUB_BRANCH));
        std::string cmd = "cd \"" + target_dir + "\" && git fetch origin "
                         + GITHUB_BRANCH + " && git reset --hard origin/" + GITHUB_BRANCH;
        int ret = runCommand(cmd, 120);
        if (ret != 0) {
            // Pull failed — remove and re-clone
            setStatus("Pull failed, removing and re-cloning...");
            std::filesystem::remove_all(target_dir);
        } else {
            return true;
        }
    }

    // Fresh clone (shallow for speed)
    setStatus("Cloning " + std::string(GITHUB_REPO) + " (branch: " + GITHUB_BRANCH + ")...");
    std::string cmd = "git clone --depth 1 --branch " + std::string(GITHUB_BRANCH)
                    + " " + std::string(GITHUB_REPO) + " \"" + target_dir + "\"";
    int ret = runCommand(cmd, 300); // 5 min timeout
    if (ret != 0) {
        setStatus("git clone failed with exit code " + std::to_string(ret));
        return false;
    }

    setStatus("✅ Source downloaded successfully");
    return true;
}

bool AutoUpdater::buildProject(const std::string& source_dir, const std::string& build_dir) {
    // Create build directory
    std::filesystem::create_directories(build_dir);

    // Step 1: cmake
    setStatus("Running cmake...");
    std::string cmake_cmd = "cd \"" + build_dir + "\" && cmake \"" + source_dir
                          + "\" -DBUILD_TESTS=OFF -DBUILD_DEBUG_UTILITIES=OFF 2>&1";
    int ret = runCommand(cmake_cmd, 120);
    if (ret != 0) {
        setStatus("cmake failed with exit code " + std::to_string(ret));
        return false;
    }

    // Step 2: make (use -j2 to avoid race conditions)
    setStatus("Compiling daemon (this may take several minutes)...");
    std::string make_cmd = "cd \"" + build_dir + "\" && make -j2 daemon 2>&1";
    ret = runCommand(make_cmd, BUILD_TIMEOUT_SECS);
    if (ret != 0) {
        setStatus("make failed with exit code " + std::to_string(ret));
        return false;
    }

    setStatus("✅ Compilation successful");
    return true;
}

std::string AutoUpdater::calculateSourceHash(const std::string& source_dir) {
    // Use the same file list as cmake/AIIntegrityHash.cmake
    // sorted alphabetically for determinism
    static const std::vector<std::string> filenames = {
        "ai_checkpoint_monitor.cpp",
        "ai_checkpoint_monitor.hpp",
        "ai_checkpoint_validator.cpp",
        "ai_checkpoint_validator.hpp",
        "ai_config.hpp",
        "ai_forced_remediation.cpp",
        "ai_forced_remediation.hpp",
        "ai_integrity_verifier.cpp",
        "ai_integrity_verifier.hpp",
        "ai_lwma_learning.cpp",
        "ai_lwma_learning.hpp",
        "ai_module.cpp",
        "ai_module.hpp",
        "ai_network_sandbox.cpp",
        "ai_network_sandbox.hpp",
        "ai_quarantine_system.cpp",
        "ai_quarantine_system.hpp",
        "ai_sandbox.cpp",
        "ai_sandbox.hpp"
    };

    std::string ai_dir = source_dir + "/src/ai";
    std::string combined;

    for (const auto& fn : filenames) {
        std::string filepath = ai_dir + "/" + fn;
        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[AI AutoUpdater] Missing file: " << filepath << std::endl;
            return "";
        }
        std::stringstream ss;
        ss << file.rdbuf();
        combined += ss.str();
    }

    if (combined.empty()) {
        return "";
    }

    // Calculate SHA-256 using the project's tools::sha256sum
    // This matches cmake's string(SHA256 ...) for the same content
    crypto::hash hash;
    if (tools::sha256sum(
            reinterpret_cast<const uint8_t*>(combined.data()),
            combined.size(),
            hash))
    {
        return epee::string_tools::pod_to_hex(hash);
    }

    return "";
}

bool AutoUpdater::backupCurrentBinary(const std::string& backup_path) {
    std::string current = getDaemonPath();
    try {
        std::filesystem::copy_file(current, backup_path);
        setStatus("Backup: " + current + " → " + backup_path);
        return true;
    } catch (const std::exception& e) {
        setStatus("Backup failed: " + std::string(e.what()));
        return false;
    }
}

bool AutoUpdater::installNewBinary(const std::string& new_binary, const std::string& target_path) {
    try {
        // Copy new binary to target
        std::filesystem::copy_file(new_binary, target_path,
            std::filesystem::copy_options::overwrite_existing);

        // Make executable on Unix
#ifndef _WIN32
        std::filesystem::permissions(target_path,
            std::filesystem::perms::owner_exec |
            std::filesystem::perms::group_exec |
            std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add);
#endif

        return true;
    } catch (const std::exception& e) {
        setStatus("Install failed: " + std::string(e.what()));
        return false;
    }
}

void AutoUpdater::signalRestart() {
    setStatus("🔄 Update installed — daemon will restart now");
    setStatus("Sending SIGHUP for graceful restart...");

#ifndef _WIN32
    // Send SIGHUP to ourselves to trigger a graceful restart
    // The daemon's signal handler should catch this and restart
    pid_t pid = getpid();
    
    // Give time for logs to flush
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // First try SIGHUP (graceful restart)
    kill(pid, SIGHUP);
    
    // If still running after 10 seconds, exit with restart code
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    setStatus("SIGHUP didn't restart — exiting with code 42 (restart requested)");
    // Exit code 42 = "please restart me"
    // A supervisor script (systemd, etc.) can detect this and restart
    _exit(42);
#else
    setStatus("Windows: manual restart required");
#endif
}

} // namespace ninacatcoin_ai
