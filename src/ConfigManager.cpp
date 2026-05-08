#include "ConfigManager.h"
#include "Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace dbsync {

bool ConfigManager::LoadConfig(const std::string& configFilePath) {
    std::ifstream file(configFilePath);
    if (!file.is_open()) {
        LOG_WARNING("Config file not found: " + configFilePath + ", using default config");
        SetDefaultConfig();
        return false;
    }
    
    std::string line;
    std::string currentSection;
    
    while (std::getline(file, line)) {
        // 去除注释
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }
        
        // 去除首尾空格
        line = Trim(line);
        if (line.empty()) continue;
        
        // 解析节
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        
        // 解析键值对
        size_t equalPos = line.find('=');
        if (equalPos == std::string::npos) continue;
        
        std::string key = Trim(line.substr(0, equalPos));
        std::string value = Trim(line.substr(equalPos + 1));
        
        // 根据节和键设置值
        if (currentSection == "local_database") {
            if (key == "host") localDbConfig_.host = value;
            else if (key == "port") localDbConfig_.port = std::stoi(value);
            else if (key == "database") localDbConfig_.database = value;
            else if (key == "username") localDbConfig_.username = value;
            else if (key == "password") localDbConfig_.password = value;
            else if (key == "charset") localDbConfig_.charset = value;
            else if (key == "embedded") localDbConfig_.embedded = (value == "true" || value == "1");
        }
        else if (currentSection == "remote_database") {
            if (key == "host") remoteDbConfig_.host = value;
            else if (key == "port") remoteDbConfig_.port = std::stoi(value);
            else if (key == "database") remoteDbConfig_.database = value;
            else if (key == "username") remoteDbConfig_.username = value;
            else if (key == "password") remoteDbConfig_.password = value;
            else if (key == "charset") remoteDbConfig_.charset = value;
            else if (key == "embedded") remoteDbConfig_.embedded = (value == "true" || value == "1");
        }
        else if (currentSection == "network") {
            if (key == "local_ip") networkConfig_.localIp = value;
            else if (key == "local_port") networkConfig_.localPort = std::stoi(value);
            else if (key == "remote_ip") networkConfig_.remoteIp = value;
            else if (key == "remote_port") networkConfig_.remotePort = std::stoi(value);
            else if (key == "connection_timeout_ms") networkConfig_.connectionTimeoutMs = std::stoi(value);
            else if (key == "retry_interval_ms") networkConfig_.retryIntervalMs = std::stoi(value);
        }
        else if (currentSection == "sync") {
            if (key == "auto_start") syncConfig_.autoStart = (value == "true" || value == "1");
            else if (key == "minimize_to_tray") syncConfig_.minimizeToTray = (value == "true" || value == "1");
            else if (key == "start_with_windows") syncConfig_.startWithWindows = (value == "true" || value == "1");
            else if (key == "sync_interval_ms") syncConfig_.syncIntervalMs = std::stoi(value);
            else if (key == "max_retries") syncConfig_.maxRetries = std::stoi(value);
            else if (key == "resolve_conflicts_automatically") syncConfig_.resolveConflictsAutomatically = (value == "true" || value == "1");
            else if (key == "conflict_resolution_strategy") syncConfig_.conflictResolutionStrategy = value;
            else if (key == "sync_tables") syncConfig_.syncTables = ParseList(value);
            else if (key == "sync_all_tables") syncConfig_.syncAllTables = (value == "true" || value == "1");
        }
        else if (currentSection == "node") {
            if (key == "id") nodeId_ = value;
        }
    }
    
    file.close();
    LOG_INFO("Configuration loaded successfully from: " + configFilePath);
    return true;
}

bool ConfigManager::SaveConfig(const std::string& configFilePath) {
    std::ofstream file(configFilePath);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file for writing: " + configFilePath);
        return false;
    }
    
    file << "# DbSync Configuration File\n";
    file << "# Generated: " << Utils::GetCurrentTimestamp() << "\n\n";
    
    // Node section
    file << "[node]\n";
    file << "id = " << nodeId_ << "\n\n";
    
    // Local database section
    file << "[local_database]\n";
    file << "host = " << localDbConfig_.host << "\n";
    file << "port = " << localDbConfig_.port << "\n";
    file << "database = " << localDbConfig_.database << "\n";
    file << "username = " << localDbConfig_.username << "\n";
    file << "password = " << localDbConfig_.password << "\n";
    file << "charset = " << localDbConfig_.charset << "\n";
    file << "embedded = " << (localDbConfig_.embedded ? "true" : "false") << "\n\n";
    
    // Remote database section
    file << "[remote_database]\n";
    file << "host = " << remoteDbConfig_.host << "\n";
    file << "port = " << remoteDbConfig_.port << "\n";
    file << "database = " << remoteDbConfig_.database << "\n";
    file << "username = " << remoteDbConfig_.username << "\n";
    file << "password = " << remoteDbConfig_.password << "\n";
    file << "charset = " << remoteDbConfig_.charset << "\n";
    file << "embedded = " << (remoteDbConfig_.embedded ? "true" : "false") << "\n\n";
    
    // Network section
    file << "[network]\n";
    file << "local_ip = " << networkConfig_.localIp << "\n";
    file << "local_port = " << networkConfig_.localPort << "\n";
    file << "remote_ip = " << networkConfig_.remoteIp << "\n";
    file << "remote_port = " << networkConfig_.remotePort << "\n";
    file << "connection_timeout_ms = " << networkConfig_.connectionTimeoutMs << "\n";
    file << "retry_interval_ms = " << networkConfig_.retryIntervalMs << "\n\n";
    
    // Sync section
    file << "[sync]\n";
    file << "auto_start = " << (syncConfig_.autoStart ? "true" : "false") << "\n";
    file << "minimize_to_tray = " << (syncConfig_.minimizeToTray ? "true" : "false") << "\n";
    file << "start_with_windows = " << (syncConfig_.startWithWindows ? "true" : "false") << "\n";
    file << "sync_interval_ms = " << syncConfig_.syncIntervalMs << "\n";
    file << "max_retries = " << syncConfig_.maxRetries << "\n";
    file << "resolve_conflicts_automatically = " << (syncConfig_.resolveConflictsAutomatically ? "true" : "false") << "\n";
    file << "conflict_resolution_strategy = " << syncConfig_.conflictResolutionStrategy << "\n";
    file << "sync_all_tables = " << (syncConfig_.syncAllTables ? "true" : "false") << "\n";
    file << "sync_tables = ";
    for (size_t i = 0; i < syncConfig_.syncTables.size(); ++i) {
        if (i > 0) file << ", ";
        file << syncConfig_.syncTables[i];
    }
    file << "\n";
    
    file.close();
    LOG_INFO("Configuration saved successfully to: " + configFilePath);
    return true;
}

void ConfigManager::SetDefaultConfig() {
    // Local database defaults
    localDbConfig_.host = "localhost";
    localDbConfig_.port = 3050;
    localDbConfig_.database = "LOCAL_DB";
    localDbConfig_.username = "SYSDBA";
    localDbConfig_.password = "masterkey";
    localDbConfig_.charset = "UTF8";
    localDbConfig_.embedded = true;  // 默认使用嵌入式模式
    
    // Remote database defaults
    remoteDbConfig_.host = "192.168.1.100";
    remoteDbConfig_.port = 3050;
    remoteDbConfig_.database = "REMOTE_DB";
    remoteDbConfig_.username = "SYSDBA";
    remoteDbConfig_.password = "masterkey";
    remoteDbConfig_.charset = "UTF8";
    remoteDbConfig_.embedded = true;  // 默认使用嵌入式模式
    
    // Network defaults
    networkConfig_.localIp = "0.0.0.0";
    networkConfig_.localPort = DEFAULT_SYNC_PORT;
    networkConfig_.remoteIp = "192.168.1.100";
    networkConfig_.remotePort = DEFAULT_SYNC_PORT;
    networkConfig_.connectionTimeoutMs = 5000;
    networkConfig_.retryIntervalMs = 3000;
    
    // Sync defaults
    syncConfig_.autoStart = true;
    syncConfig_.minimizeToTray = true;
    syncConfig_.startWithWindows = false;
    syncConfig_.syncIntervalMs = DEFAULT_MONITOR_INTERVAL_MS;
    syncConfig_.maxRetries = 3;
    syncConfig_.resolveConflictsAutomatically = true;
    syncConfig_.conflictResolutionStrategy = "timestamp";
    syncConfig_.syncAllTables = true;
    
    // Generate node ID
    nodeId_ = Utils::GenerateUUID();
}

std::string ConfigManager::Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> ConfigManager::ParseList(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = Trim(item);
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

} // namespace dbsync
