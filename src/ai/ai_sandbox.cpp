// Copyright (c) 2026, The ninacatcoin Project
//
// Filesystem sandbox implementation

#include "ai_sandbox.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace ninacatcoin_ai {

static FileSystemSandbox* g_fs_sandbox = nullptr;

FileSystemSandbox& FileSystemSandbox::getInstance() {
    if (!g_fs_sandbox) {
        g_fs_sandbox = new FileSystemSandbox();
    }
    return *g_fs_sandbox;
}

FileSystemSandbox::FileSystemSandbox()
    : is_initialized(false) {
}

FileSystemSandbox::~FileSystemSandbox() {
}

bool FileSystemSandbox::initialize() {
    if (is_initialized) {
        return true;
    }

    initializeWhitelist();
    initializeBlacklist();

    is_initialized = true;
    std::cout << "[AI Sandbox] Filesystem sandbox initialized" << std::endl;
    return true;
}

bool FileSystemSandbox::isPathAllowed(const std::string& path) {
    if (!is_initialized) {
        return false;
    }

    std::string normalized = normalizePath(path);

    // Check blacklist first (explicit denial)
    for (const auto& blocked : blacklisted_paths) {
        if (normalized.find(blocked) == 0) {
            return false;
        }
    }

    // Check whitelist (explicit allowance)
    for (const auto& allowed : whitelisted_paths) {
        if (normalized.find(allowed) == 0) {
            return true;
        }
    }

    // Default deny
    return false;
}

FILE* FileSystemSandbox::fopen_sandboxed(
    const std::string& path,
    const char* mode
) {
    if (!isPathAllowed(path)) {
        logAccessAttempt(path, "fopen", false);
        std::cerr << "[AI Sandbox] DENIED fopen: " << path << std::endl;
        return nullptr;
    }

    logAccessAttempt(path, "fopen", true);
    return fopen(path.c_str(), mode);
}

int FileSystemSandbox::open_sandboxed(const std::string& path, int flags) {
    if (!isPathAllowed(path)) {
        logAccessAttempt(path, "open", false);
        std::cerr << "[AI Sandbox] DENIED open: " << path << std::endl;
        return -1;
    }

    logAccessAttempt(path, "open", true);
    return open(path.c_str(), flags);
}

bool FileSystemSandbox::canRead(const std::string& path) {
    return isPathAllowed(path);
}

bool FileSystemSandbox::canWrite(const std::string& path) {
    return isPathAllowed(path);
}

std::vector<std::string> FileSystemSandbox::getAccessLog() {
    return access_log;
}

bool FileSystemSandbox::verifySandboxActive() {
    return is_initialized;
}

std::string FileSystemSandbox::normalizePath(const std::string& path) {
    // Remove double slashes and normalize
    std::string normalized = path;
    size_t pos = 0;
    
    while ((pos = normalized.find("//", pos)) != std::string::npos) {
        normalized.erase(pos, 1);
    }
    
    // Convert backslashes to forward slashes (Windows compatibility)
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    
    return normalized;
}

void FileSystemSandbox::logAccessAttempt(
    const std::string& path,
    const std::string& operation,
    bool allowed
) {
    std::string log_entry = 
        "[" + std::string(allowed ? "ALLOW" : "DENY") + "] " +
        operation + ": " + path;
    
    access_log.push_back(log_entry);
    
    if (!allowed && access_log.size() % 10 == 0) {
        // Log to file periodically
        // TODO: Write to audit log file
    }
}

void FileSystemSandbox::initializeWhitelist() {
    // Add all allowed paths
    for (const auto& path : AISecurityConfig::ALLOWED_PATHS) {
        whitelisted_paths.insert(normalizePath(path));
    }
    
    std::cout << "[AI Sandbox] Whitelist initialized with " 
              << whitelisted_paths.size() << " paths" << std::endl;
}

void FileSystemSandbox::initializeBlacklist() {
    // Explicitly blacklist dangerous paths
    blacklisted_paths.insert("/etc/");
    blacklisted_paths.insert("/sys/");
    blacklisted_paths.insert("/proc/");
    blacklisted_paths.insert("/dev/");
    blacklisted_paths.insert("/var/");
    blacklisted_paths.insert("/tmp/");
    blacklisted_paths.insert("/home/");
    blacklisted_paths.insert("/root/");
    blacklisted_paths.insert("C:\\Users\\");
    blacklisted_paths.insert("C:\\Program Files\\");
    blacklisted_paths.insert("C:\\Windows\\");
    
    std::cout << "[AI Sandbox] Blacklist initialized with " 
              << blacklisted_paths.size() << " paths" << std::endl;
}

} // namespace ninacatcoin_ai
