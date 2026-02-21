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
    // Fallback only — prefer getSourceDir()/getBuildDir() for actual builds
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

// Detect the build directory from the running binary path
// Examples:
//   /mnt/i/ninacatcoin/build-linux/bin/ninacatcoind → /mnt/i/ninacatcoin/build-linux
//   /mnt/i/ninacatcoin/build/bin/ninacatcoind → /mnt/i/ninacatcoin/build
//   C:\ninacatcoin\build\bin\Release\ninacatcoind.exe → C:\ninacatcoin\build
//   /home/user/ninacatcoin/build/bin/ninacatcoind → /home/user/ninacatcoin/build
//   /root/ninacatcoin/build/bin/ninacatcoind → /root/ninacatcoin/build
std::string AutoUpdater::getBuildDir() {
    namespace fs = std::filesystem;
    fs::path daemon = fs::path(getDaemonPath());

    // Walk up from binary looking for a parent named build* that contains CMakeCache.txt
    fs::path current = daemon.parent_path(); // skip the binary itself
    while (current.has_parent_path() && current != current.root_path()) {
        std::string dirname = current.filename().string();
        // Check if this dir has CMakeCache.txt (definitive cmake build dir)
        if (fs::exists(current / "CMakeCache.txt")) {
            setStatus("Detected build directory: " + current.string());
            return current.string();
        }
        current = current.parent_path();
    }

    // Fallback: assume binary is at <build_dir>/bin/ninacatcoind
    fs::path fallback = daemon.parent_path().parent_path();
    setStatus("Build dir fallback (no CMakeCache.txt found): " + fallback.string());
    return fallback.string();
}

// Detect the source root from the build directory
// Reads CMakeCache.txt to find CMAKE_HOME_DIRECTORY or parses the path
std::string AutoUpdater::getSourceDir() {
    namespace fs = std::filesystem;
    std::string build_dir = getBuildDir();

    // Method 1: Read CMakeCache.txt for the actual source path
    std::string cache_file = build_dir + "/CMakeCache.txt";
    if (fs::exists(cache_file)) {
        std::ifstream cache(cache_file);
        std::string line;
        while (std::getline(cache, line)) {
            // Look for: CMAKE_HOME_DIRECTORY:INTERNAL=/path/to/source
            if (line.find("CMAKE_HOME_DIRECTORY:INTERNAL=") == 0) {
                std::string src = line.substr(line.find('=') + 1);
                if (!src.empty() && fs::exists(src + "/CMakeLists.txt")) {
                    setStatus("Source dir from CMakeCache: " + src);
                    return src;
                }
            }
            // Alternative: ninacatcoin_SOURCE_DIR
            if (line.find("ninacatcoin_SOURCE_DIR:STATIC=") == 0) {
                std::string src = line.substr(line.find('=') + 1);
                if (!src.empty() && fs::exists(src + "/CMakeLists.txt")) {
                    setStatus("Source dir from CMakeCache (SOURCE_DIR): " + src);
                    return src;
                }
            }
        }
    }

    // Method 2: If build dir is inside source (e.g. /src/ninacatcoin/build-linux)
    // then parent of build dir is the source
    fs::path build_path(build_dir);
    fs::path parent = build_path.parent_path();
    if (fs::exists(parent / "CMakeLists.txt") && fs::exists(parent / "src")) {
        setStatus("Source dir from build parent: " + parent.string());
        return parent.string();
    }

    // Method 3: Fallback to update dir
    std::string fallback = getUpdateDir() + "/ninacatcoin";
    setStatus("Source dir fallback: " + fallback);
    return fallback;
}

// Detect the build type name from the build directory
// Returns "build-linux", "build", "build-win64", etc.
std::string AutoUpdater::detectBuildType() {
    namespace fs = std::filesystem;
    fs::path build_path(getBuildDir());
    return build_path.filename().string();
}

int AutoUpdater::runCommand(const std::string& cmd, int timeout_secs) {
#ifdef _WIN32
    // Windows: use cmd.exe /C
    std::string full_cmd = "cmd.exe /C \"" + cmd + "\"";
    if (timeout_secs > 0) {
        // No native timeout on Windows — just run it
        // (cmake/MSBuild will respect their own timeouts)
        full_cmd = "cmd.exe /C \"" + cmd + "\"";
    }
#else
    std::string full_cmd;
    // Wrap in bash -c to support shell builtins (cd, &&, etc.)
    std::string escaped_cmd = cmd;
    size_t pos = 0;
    while ((pos = escaped_cmd.find('\'', pos)) != std::string::npos) {
        escaped_cmd.replace(pos, 1, "'\\''");
        pos += 4;
    }
    if (timeout_secs > 0) {
        full_cmd = "timeout " + std::to_string(timeout_secs) + " bash -c '" + escaped_cmd + "'";
    } else {
        full_cmd = "bash -c '" + escaped_cmd + "'";
    }
#endif

    setStatus("Running: " + cmd.substr(0, 100) + (cmd.size() > 100 ? "..." : ""));
    int ret = std::system(full_cmd.c_str());

#ifndef _WIN32
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

    // === Smart path detection from running binary ===
    std::string daemon_path = getDaemonPath();
    std::string source_dir  = getSourceDir();
    std::string build_dir   = getBuildDir();
    std::string build_type  = detectBuildType();

    setStatus("=== Starting auto-update from GitHub ===");
    setStatus("Daemon binary: " + daemon_path);
    setStatus("Source directory: " + source_dir);
    setStatus("Build directory: " + build_dir);
    setStatus("Build type: " + build_type);
    if (!expected_hash.empty())
        setStatus("Expected consensus hash: " + expected_hash.substr(0, 16) + "...");

    // Step 1: Download/update source in the ORIGINAL source directory
    if (!downloadSource(source_dir)) {
        setStatus("[FAIL] Failed to download source from GitHub");
        return false;
    }

    // Step 2: Verify source hash BEFORE compiling (fast check)
    if (!expected_hash.empty()) {
        std::string source_hash = calculateSourceHash(source_dir);
        if (source_hash.empty()) {
            setStatus("[FAIL] Failed to calculate hash of downloaded source");
            return false;
        }

        setStatus("Downloaded source hash: " + source_hash.substr(0, 16) + "...");

        if (source_hash != expected_hash) {
            setStatus("[FAIL] Source hash does NOT match consensus! Possible supply-chain attack.");
            setStatus("   Expected: " + expected_hash);
            setStatus("   Got:      " + source_hash);
            setStatus("   Update ABORTED for security");
            return false;
        }

        setStatus("[OK] Source hash matches network consensus");
    } else {
        setStatus("Full-source remediation mode - trusting GitHub master branch");
    }

    // Step 3: Build in the ORIGINAL build directory
    if (!buildProject(source_dir, build_dir)) {
        setStatus("[FAIL] Compilation failed");
        return false;
    }

    // Step 4: Verify the built binary exists
    std::string new_binary;
#ifdef _WIN32
    if (std::filesystem::exists(build_dir + "/bin/Release/ninacatcoind.exe"))
        new_binary = build_dir + "/bin/Release/ninacatcoind.exe";
    else if (std::filesystem::exists(build_dir + "/bin/ninacatcoind.exe"))
        new_binary = build_dir + "/bin/ninacatcoind.exe";
    else
        new_binary = build_dir + "/bin/ninacatcoind.exe";
#else
    new_binary = build_dir + "/bin/ninacatcoind";
#endif

    if (!std::filesystem::exists(new_binary)) {
        setStatus("[FAIL] Built binary not found at " + new_binary);
        return false;
    }

    // Step 5: Check if binary was rebuilt in-place (same path as running daemon)
    bool in_place = false;
    try {
        in_place = std::filesystem::equivalent(new_binary, daemon_path);
    } catch (...) {}

    if (in_place) {
        setStatus("[OK] Binary rebuilt in-place - no install needed, just restart");
    } else {
        // Backup current binary
        std::string backup_path = daemon_path + ".backup."
                                + std::to_string(std::time(nullptr));
        if (!backupCurrentBinary(backup_path)) {
            setStatus("[FAIL] Failed to backup current binary");
            return false;
        }

        // Install new binary (with Text file busy handling)
        if (!installNewBinary(new_binary, daemon_path)) {
            setStatus("[FAIL] Failed to install new binary - restoring backup");
            try {
                std::filesystem::copy_file(backup_path, daemon_path,
                    std::filesystem::copy_options::overwrite_existing);
            } catch (...) {
                setStatus("[CRITICAL] Could not restore backup either!");
            }
            return false;
        }

        setStatus("[OK] New binary installed successfully");
        setStatus("[OK] Backup at: " + backup_path);
    }

    // Step 6: Signal restart
    signalRestart();

    return true;
}

bool AutoUpdater::downloadSource(const std::string& target_dir) {
    namespace fs = std::filesystem;

    // Ensure target directory or its parent exists
    fs::create_directories(fs::path(target_dir).parent_path());

    if (fs::exists(target_dir + "/.git")) {
        // Source dir is a git repo - stash local changes and pull latest
        setStatus("Git repo detected at " + target_dir + ", updating from " + std::string(GITHUB_BRANCH));

        // Stash any local changes
        std::string stash_cmd = "cd \"" + target_dir + "\" && git stash --include-untracked 2>&1";
        runCommand(stash_cmd, 30);

        // Fetch and reset to origin
        std::string cmd = "cd \"" + target_dir + "\" && git fetch origin "
                         + GITHUB_BRANCH + " && git reset --hard origin/" + GITHUB_BRANCH;
        int ret = runCommand(cmd, 120);
        if (ret == 0) {
            setStatus("[OK] Source updated via git pull");
            return true;
        }

        setStatus("Git pull failed, attempting fresh clone to temp dir...");
    } else if (fs::exists(target_dir + "/CMakeLists.txt")) {
        // Source exists but not a git repo - initialize git and pull
        setStatus("Source exists at " + target_dir + " but no .git - initializing...");
        std::string init_cmd = "cd \"" + target_dir + "\" && git init && git remote add origin "
                             + std::string(GITHUB_REPO)
                             + " && git fetch origin " + GITHUB_BRANCH
                             + " && git reset --hard origin/" + GITHUB_BRANCH;
        int ret = runCommand(init_cmd, 300);
        if (ret == 0) {
            setStatus("[OK] Source initialized and updated from GitHub");
            return true;
        }
        setStatus("Git init+fetch failed, attempting clone to temp...");
    }

    // Fresh clone strategy: clone to temp dir then rsync/copy to target
    std::string temp_clone = getUpdateDir() + "/temp_clone";
    if (fs::exists(temp_clone)) {
        try { fs::remove_all(temp_clone); } catch (...) {}
    }

    setStatus("Cloning " + std::string(GITHUB_REPO) + " (branch: " + GITHUB_BRANCH + ")...");
    std::string cmd = "git clone --depth 1 --branch " + std::string(GITHUB_BRANCH)
                    + " " + std::string(GITHUB_REPO) + " \"" + temp_clone + "\"";
    int ret = runCommand(cmd, 300);
    if (ret != 0) {
        setStatus("git clone failed with exit code " + std::to_string(ret));
        return false;
    }

    // Sync cloned files to the actual source directory
    setStatus("Syncing downloaded source to " + target_dir + "...");
#ifndef _WIN32
    // rsync: only overwrite changed files, preserve build dirs
    std::string sync_cmd = "rsync -a --exclude='.git' --exclude='build*' "
                           "\"" + temp_clone + "/\" \"" + target_dir + "/\"";
    ret = runCommand(sync_cmd, 120);
    if (ret != 0) {
        // Fallback: cp
        sync_cmd = "cp -r \"" + temp_clone + "/\"* \"" + target_dir + "/\" 2>/dev/null; "
                   "cp -r \"" + temp_clone + "/\".* \"" + target_dir + "/\" 2>/dev/null; true";
        runCommand(sync_cmd, 120);
    }
#else
    // Windows: robocopy (returns 0-7 for success)
    std::string sync_cmd = "robocopy \"" + temp_clone + "\" \"" + target_dir
                         + "\" /E /XD .git build build-linux build-win /NFL /NDL /NJH /NJS";
    ret = runCommand(sync_cmd, 120);
    if (ret > 7) {
        setStatus("robocopy failed with code " + std::to_string(ret));
        return false;
    }
#endif

    // Copy .git from temp clone so future updates can use pull
    if (!fs::exists(target_dir + "/.git") && fs::exists(temp_clone + "/.git")) {
        try {
            fs::copy(temp_clone + "/.git", target_dir + "/.git",
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        } catch (...) {
            setStatus("Warning: Could not copy .git dir (future updates will re-clone)");
        }
    }

    // Cleanup temp clone
    try { fs::remove_all(temp_clone); } catch (...) {}

    setStatus("[OK] Source downloaded and synced to " + target_dir);
    return true;
}

bool AutoUpdater::buildProject(const std::string& source_dir, const std::string& build_dir) {
    namespace fs = std::filesystem;
    // Create build directory if it doesn't exist
    fs::create_directories(build_dir);

    // Check if CMakeCache.txt exists - if so, incremental build (faster)
    bool has_cache = fs::exists(build_dir + "/CMakeCache.txt");

    if (!has_cache) {
        // Full cmake configure
        setStatus("Running cmake (first time, full configure)...");
        std::string cmake_cmd = "cd \"" + build_dir + "\" && cmake \"" + source_dir
                              + "\" -DCMAKE_BUILD_TYPE=Release -DMANUAL_SUBMODULES=1"
                              + " -DBUILD_TESTS=OFF -DBUILD_DEBUG_UTILITIES=OFF 2>&1";
        int ret = runCommand(cmake_cmd, 120);
        if (ret != 0) {
            setStatus("cmake failed with exit code " + std::to_string(ret));
            return false;
        }
    } else {
        // Re-run cmake to pick up any new/changed files (fast - cached)
        setStatus("Re-running cmake (cached, fast)...");
        std::string cmake_cmd = "cd \"" + build_dir + "\" && cmake . 2>&1";
        int ret = runCommand(cmake_cmd, 60);
        if (ret != 0) {
            // If incremental cmake fails, try full reconfigure
            setStatus("Incremental cmake failed, trying full reconfigure...");
            cmake_cmd = "cd \"" + build_dir + "\" && cmake \"" + source_dir
                      + "\" -DCMAKE_BUILD_TYPE=Release -DMANUAL_SUBMODULES=1"
                      + " -DBUILD_TESTS=OFF -DBUILD_DEBUG_UTILITIES=OFF 2>&1";
            ret = runCommand(cmake_cmd, 120);
            if (ret != 0) {
                setStatus("cmake failed with exit code " + std::to_string(ret));
                return false;
            }
        }
    }

    // Detect number of CPU cores for parallel build
    int jobs = 2; // safe default
#ifndef _WIN32
    FILE* nproc = popen("nproc 2>/dev/null", "r");
    if (nproc) {
        char buf[16];
        if (fgets(buf, sizeof(buf), nproc)) {
            int n = std::atoi(buf);
            if (n > 0) jobs = std::max(1, n - 1); // leave 1 core free
        }
        pclose(nproc);
    }
    setStatus("Compiling daemon with make -j" + std::to_string(jobs) + " ...");
    std::string make_cmd = "cd \"" + build_dir + "\" && make -j" + std::to_string(jobs) + " daemon 2>&1";
#else
    // Windows: use cmake --build (works with Visual Studio, Ninja, MinGW)
    setStatus("Compiling daemon with cmake --build...");
    std::string make_cmd = "cd \"" + build_dir
                         + "\" && cmake --build . --target daemon --config Release 2>&1";
#endif

    int ret = runCommand(make_cmd, BUILD_TIMEOUT_SECS);
    if (ret != 0) {
        setStatus("Build failed with exit code " + std::to_string(ret));
        return false;
    }

    setStatus("[OK] Compilation successful");
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
    namespace fs = std::filesystem;
    try {
#ifndef _WIN32
        // === Linux: Handle "Text file busy" with mv trick ===
        // When the binary is running, Linux locks the inode for writing
        // but allows rename (mv). So we:
        //   1. mv running_binary → running_binary.old (rename inode, process keeps running)
        //   2. cp new_binary → target_path (writes to new inode)
        //   3. rm running_binary.old (clean up, will complete after process exits)
        std::string old_path = target_path + ".old";

        // Remove any previous .old file
        if (fs::exists(old_path)) {
            try { fs::remove(old_path); } catch (...) {}
        }

        // Step 1: Rename the running binary (this works even while running!)
        setStatus("Moving running binary aside: " + target_path + " → .old");
        fs::rename(target_path, old_path);

        // Step 2: Copy new binary to original path
        setStatus("Installing new binary to " + target_path);
        fs::copy_file(new_binary, target_path, fs::copy_options::overwrite_existing);

        // Step 3: Make executable
        fs::permissions(target_path,
            fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write |
            fs::perms::group_exec | fs::perms::group_read |
            fs::perms::others_exec | fs::perms::others_read,
            fs::perm_options::replace);

        // Step 4: Clean up old binary (best-effort)
        try { fs::remove(old_path); } catch (...) {
            setStatus("Note: .old binary will be cleaned up on next restart");
        }

        setStatus("\xe2\x9c\x85 Binary installed via mv trick (no Text file busy)");
#else
        // === Windows: Binary can't be replaced while running ===
        // Strategy: copy to target.new, then use cmd to swap on next restart
        // Or if binary is not locked (different path), just copy
        std::error_code ec;
        fs::copy_file(new_binary, target_path, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            // Binary is locked — copy as .new and create a swap script
            std::string new_path = target_path + ".new";
            fs::copy_file(new_binary, new_path, fs::copy_options::overwrite_existing);

            // Create a batch script that will swap on next start
            std::string bat_path = fs::path(target_path).parent_path().string() + "/update_swap.bat";
            std::ofstream bat(bat_path);
            bat << "@echo off\n";
            bat << "timeout /t 3 /nobreak >nul\n";
            bat << "move /Y \"" << target_path << ".new\" \"" << target_path << "\"\n";
            bat << "del \"%~f0\"\n";
            bat.close();

            setStatus("\xe2\x9a\xa0\xef\xb8\x8f Binary locked — staged as .new, swap script created at " + bat_path);
            setStatus("Run update_swap.bat after stopping daemon, or restart to apply");
        }
#endif

        return true;
    } catch (const std::exception& e) {
        setStatus("Install failed: " + std::string(e.what()));
        return false;
    }
}

void AutoUpdater::signalRestart() {
    setStatus("[UPDATE] Update installed - daemon will restart now");

#ifndef _WIN32
    setStatus("Sending SIGHUP for graceful restart...");

    pid_t pid = getpid();
    
    // Give time for logs to flush
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // First try SIGHUP (graceful restart)
    kill(pid, SIGHUP);
    
    // If still running after 10 seconds, exit with restart code
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    setStatus("SIGHUP didn't restart - exiting with code 42 (restart requested)");
    // Exit code 42 = "please restart me"
    // A supervisor (systemd, script) can detect this and restart
    _exit(42);
#else
    // Windows: graceful exit with restart code 42
    setStatus("Windows: exiting with restart code 42");
    setStatus("Configure your service/script to restart on exit code 42");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    _exit(42);
#endif
}

} // namespace ninacatcoin_ai
