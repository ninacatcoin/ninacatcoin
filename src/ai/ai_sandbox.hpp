// Copyright (c) 2026, The ninacatcoin Project
//
// Filesystem sandbox - restricts IA module file access

#pragma once

#include "ai_config.hpp"
#include <string>
#include <vector>
#include <set>
#include <cstdio>

namespace ninacatcoin_ai {

/**
 * @class FileSystemSandbox
 * @brief Enforces strict filesystem access control for AI module
 * 
 * CRITICAL SECURITY:
 * - Only allows access to whitelisted paths
 * - Blocks access to user files, system files, etc
 * - Intercepts ALL file operations
 * - Logs all access attempts
 */
class FileSystemSandbox {
public:
    static FileSystemSandbox& getInstance();

    // Initialize sandbox (must be called before file operations)
    bool initialize();

    // Check if path is allowed
    bool isPathAllowed(const std::string& path);

    // Sandboxed file operations
    FILE* fopen_sandboxed(const std::string& path, const char* mode);
    
    int open_sandboxed(const std::string& path, int flags);

    // Check read permission
    bool canRead(const std::string& path);

    // Check write permission
    bool canWrite(const std::string& path);

    // Get audit log
    std::vector<std::string> getAccessLog();

    // Verify sandbox is active
    bool verifySandboxActive();

private:
    FileSystemSandbox();
    ~FileSystemSandbox();

    // Delete copy/move
    FileSystemSandbox(const FileSystemSandbox&) = delete;
    FileSystemSandbox& operator=(const FileSystemSandbox&) = delete;

    std::set<std::string> whitelisted_paths;
    std::set<std::string> blacklisted_paths;
    std::vector<std::string> access_log;
    bool is_initialized;

    // Normalize path for comparison
    std::string normalizePath(const std::string& path);

    // Log access attempt
    void logAccessAttempt(
        const std::string& path,
        const std::string& operation,
        bool allowed
    );

    // Initialize whitelist
    void initializeWhitelist();

    // Initialize blacklist  
    void initializeBlacklist();
};

} // namespace ninacatcoin_ai
