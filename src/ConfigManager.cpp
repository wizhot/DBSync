/**
 * @file ConfigManager.cpp
 * @brief 配置管理器实现
 * @details 实现 INI 格式配置文件的加载和保存，支持：
 *          - 自动根据文件扩展名判断数据库类型（.db=SQLite, .fdb=Firebird）
 *          - db_type 字段支持 "auto"/"sqlite"/"firebird" 三种值
 *          - encryption_key 字段支持 SQLite 数据库加密密钥
 *          - 合理的默认值回退机制
 */

#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cstring>

// 如果有 UUID 生成库，可替换此处
#include <random>

namespace dbsync {

// ==================== 单例访问 ====================

ConfigManager& ConfigManager::GetInstance()
{
    static ConfigManager instance;
    return instance;
}

// ==================== 构造函数 ====================

ConfigManager::ConfigManager()
{
    InitDefaults();
}

// ==================== 初始化默认配置 ====================

void ConfigManager::InitDefaults()
{
    // 节点 ID：默认为空，加载配置时设置
    nodeId_ = "";

    // 本地数据库配置：SQLite (grasp.db)
    localDbConfig_.host = "";
    localDbConfig_.port = 0;
    localDbConfig_.database = "data/grasp.db";
    localDbConfig_.username = "";
    localDbConfig_.password = "";
    localDbConfig_.charset = "UTF8";
    localDbConfig_.embedded = false;
    localDbConfig_.dbType = "sqlite";
    localDbConfig_.encryptionKey = "";

    // 远程数据库配置：Firebird (SALES.FDB)
    remoteDbConfig_.host = "192.168.1.100";
    remoteDbConfig_.port = 3050;
    remoteDbConfig_.database = "data/SALES.FDB";
    remoteDbConfig_.username = "SYSDBA";
    remoteDbConfig_.password = "masterkey";
    remoteDbConfig_.charset = "UTF8";
    remoteDbConfig_.embedded = true;
    remoteDbConfig_.dbType = "firebird";
    remoteDbConfig_.encryptionKey = "";

    // 网络配置
    networkConfig_.localIp = "0.0.0.0";
    networkConfig_.localPort = 15555;
    networkConfig_.remoteIp = "192.168.1.100";
    networkConfig_.remotePort = 15555;
    networkConfig_.connectionTimeoutMs = 5000;
    networkConfig_.retryIntervalMs = 3000;

    // 同步配置
    syncConfig_.autoStart = true;
    syncConfig_.minimizeToTray = true;
    syncConfig_.startWithWindows = false;
    syncConfig_.syncIntervalMs = 1000;
    syncConfig_.maxRetries = 3;
    syncConfig_.resolveConflictsAutomatically = true;
    syncConfig_.conflictResolutionStrategy = "timestamp";
    syncConfig_.mappingFile = "config/mapping.json";
}

// ==================== 数据库类型自动检测 ====================

std::string ConfigManager::DetectDbType(const std::string& dbPath)
{
    // 将路径转为小写，方便比较扩展名
    std::string lowerPath = dbPath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);

    // 根据文件扩展名判断数据库类型
    if (lowerPath.size() >= 4 && lowerPath.substr(lowerPath.size() - 4) == ".fdb") {
        return "firebird";
    }
    if (lowerPath.size() >= 3 && lowerPath.substr(lowerPath.size() - 3) == ".db") {
        return "sqlite";
    }

    // 默认返回 sqlite
    return "sqlite";
}

// ==================== INI 行解析 ====================

bool ConfigManager::ParseIniLine(const std::string& line, std::string& key, std::string& value)
{
    // 跳过空行和注释行
    if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') {
        return false;
    }

    // 查找等号
    size_t eq_pos = line.find('=');
    if (eq_pos == std::string::npos) {
        return false;
    }

    // 提取键和值
    key = line.substr(0, eq_pos);
    value = line.substr(eq_pos + 1);

    // 去除键和值两端的空白字符
    auto trim = [](std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end = s.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            s = s.substr(start, end - start + 1);
        } else {
            s.clear();
        }
    };

    trim(key);
    trim(value);

    return !key.empty();
}

// ==================== 加载配置文件 ====================

bool ConfigManager::LoadConfig(const std::string& configFilePath)
{
    std::lock_guard<std::mutex> lock(mutex_);

    configFilePath_ = configFilePath;

    // 先初始化默认值
    InitDefaults();

    // 打开配置文件
    std::ifstream ifs(configFilePath);
    if (!ifs.is_open()) {
        std::cerr << "[ConfigManager] 无法打开配置文件: " << configFilePath
                  << "，使用默认配置" << std::endl;
        return false;
    }

    std::cout << "[ConfigManager] 正在加载配置文件: " << configFilePath << std::endl;

    // 当前解析的节（section）
    std::string currentSection;
    std::string line;

    while (std::getline(ifs, line)) {
        // 去除行尾的 \r（Windows 换行符）
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // 检查是否是节标题 [section]
        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                currentSection = line.substr(1, end - 1);
            }
            continue;
        }

        // 解析键值对
        std::string key, value;
        if (!ParseIniLine(line, key, value)) {
            continue;
        }

        // 根据当前节分配配置值
        if (currentSection == "node") {
            if (key == "id") {
                nodeId_ = value;
            }
        }
        else if (currentSection == "local_database") {
            if (key == "database") {
                localDbConfig_.database = value;
            }
            else if (key == "host") {
                localDbConfig_.host = value;
            }
            else if (key == "port") {
                localDbConfig_.port = std::stoi(value);
            }
            else if (key == "username") {
                localDbConfig_.username = value;
            }
            else if (key == "password") {
                localDbConfig_.password = value;
            }
            else if (key == "charset") {
                localDbConfig_.charset = value;
            }
            else if (key == "embedded") {
                localDbConfig_.embedded = (value == "true" || value == "1");
            }
            else if (key == "db_type") {
                localDbConfig_.dbType = value;
            }
            else if (key == "encryption_key") {
                localDbConfig_.encryptionKey = value;
            }
        }
        else if (currentSection == "remote_database") {
            if (key == "database") {
                remoteDbConfig_.database = value;
            }
            else if (key == "host") {
                remoteDbConfig_.host = value;
            }
            else if (key == "port") {
                remoteDbConfig_.port = std::stoi(value);
            }
            else if (key == "username") {
                remoteDbConfig_.username = value;
            }
            else if (key == "password") {
                remoteDbConfig_.password = value;
            }
            else if (key == "charset") {
                remoteDbConfig_.charset = value;
            }
            else if (key == "embedded") {
                remoteDbConfig_.embedded = (value == "true" || value == "1");
            }
            else if (key == "db_type") {
                remoteDbConfig_.dbType = value;
            }
            else if (key == "encryption_key") {
                remoteDbConfig_.encryptionKey = value;
            }
        }
        else if (currentSection == "network") {
            if (key == "local_ip") {
                networkConfig_.localIp = value;
            }
            else if (key == "local_port") {
                networkConfig_.localPort = std::stoi(value);
            }
            else if (key == "remote_ip") {
                networkConfig_.remoteIp = value;
            }
            else if (key == "remote_port") {
                networkConfig_.remotePort = std::stoi(value);
            }
            else if (key == "connection_timeout_ms") {
                networkConfig_.connectionTimeoutMs = std::stoi(value);
            }
            else if (key == "retry_interval_ms") {
                networkConfig_.retryIntervalMs = std::stoi(value);
            }
        }
        else if (currentSection == "sync") {
            if (key == "auto_start") {
                syncConfig_.autoStart = (value == "true" || value == "1");
            }
            else if (key == "minimize_to_tray") {
                syncConfig_.minimizeToTray = (value == "true" || value == "1");
            }
            else if (key == "start_with_windows") {
                syncConfig_.startWithWindows = (value == "true" || value == "1");
            }
            else if (key == "sync_interval_ms") {
                syncConfig_.syncIntervalMs = std::stoi(value);
            }
            else if (key == "max_retries") {
                syncConfig_.maxRetries = std::stoi(value);
            }
            else if (key == "resolve_conflicts_automatically") {
                syncConfig_.resolveConflictsAutomatically = (value == "true" || value == "1");
            }
            else if (key == "conflict_resolution_strategy") {
                syncConfig_.conflictResolutionStrategy = value;
            }
            else if (key == "mapping_file") {
                syncConfig_.mappingFile = value;
            }
        }
    }

    // 自动检测数据库类型（当 db_type 为 "auto" 时）
    if (localDbConfig_.dbType == "auto") {
        localDbConfig_.dbType = DetectDbType(localDbConfig_.database);
    }
    if (remoteDbConfig_.dbType == "auto") {
        remoteDbConfig_.dbType = DetectDbType(remoteDbConfig_.database);
    }

    // 生成默认节点 ID（如果未配置）
    if (nodeId_.empty() || nodeId_ == "auto-generated-uuid") {
        // 简单的随机 UUID 生成
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);
        const char* hex = "0123456789abcdef";
        std::string uuid;
        for (int i = 0; i < 32; ++i) {
            if (i == 8 || i == 12 || i == 16 || i == 20) {
                uuid += '-';
            }
            uuid += hex[dis(gen)];
        }
        nodeId_ = uuid;
    }

    std::cout << "[ConfigManager] 配置加载完成" << std::endl;
    std::cout << "  本地数据库: " << localDbConfig_.database
              << " (类型: " << localDbConfig_.dbType << ")" << std::endl;
    std::cout << "  远程数据库: " << remoteDbConfig_.database
              << " (类型: " << remoteDbConfig_.dbType << ")" << std::endl;
    std::cout << "  同步间隔: " << syncConfig_.syncIntervalMs << "ms" << std::endl;
    std::cout << "  映射文件: " << syncConfig_.mappingFile << std::endl;

    return true;
}

// ==================== 保存配置文件 ====================

bool ConfigManager::SaveConfig(const std::string& configFilePath)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::string savePath = configFilePath.empty() ? configFilePath_ : configFilePath;
    if (savePath.empty()) {
        std::cerr << "[ConfigManager] 未指定配置文件路径" << std::endl;
        return false;
    }

    std::ofstream ofs(savePath);
    if (!ofs.is_open()) {
        std::cerr << "[ConfigManager] 无法写入配置文件: " << savePath << std::endl;
        return false;
    }

    // 写入配置文件
    ofs << "# DbSync Configuration File" << std::endl;
    ofs << "# 管家婆 grasp.db <-> SALES.FDB 实时同步" << std::endl;
    ofs << std::endl;

    // 节点配置
    ofs << "[node]" << std::endl;
    ofs << "id = " << nodeId_ << std::endl;
    ofs << std::endl;

    // 本地数据库配置
    ofs << "[local_database]" << std::endl;
    ofs << "# 本地数据库：管家婆 grasp.db (SQLite加密)" << std::endl;
    ofs << "database = " << localDbConfig_.database << std::endl;
    ofs << "db_type = " << localDbConfig_.dbType << std::endl;
    ofs << "encryption_key = " << localDbConfig_.encryptionKey << std::endl;
    ofs << "charset = " << localDbConfig_.charset << std::endl;
    ofs << std::endl;

    // 远程数据库配置
    ofs << "[remote_database]" << std::endl;
    ofs << "# 远程数据库：SALES.FDB (Firebird Embedded)" << std::endl;
    ofs << "host = " << remoteDbConfig_.host << std::endl;
    ofs << "database = " << remoteDbConfig_.database << std::endl;
    ofs << "db_type = " << remoteDbConfig_.dbType << std::endl;
    ofs << "username = " << remoteDbConfig_.username << std::endl;
    ofs << "password = " << remoteDbConfig_.password << std::endl;
    ofs << "charset = " << remoteDbConfig_.charset << std::endl;
    ofs << "embedded = " << (remoteDbConfig_.embedded ? "true" : "false") << std::endl;
    ofs << std::endl;

    // 网络配置
    ofs << "[network]" << std::endl;
    ofs << "local_ip = " << networkConfig_.localIp << std::endl;
    ofs << "local_port = " << networkConfig_.localPort << std::endl;
    ofs << "remote_ip = " << networkConfig_.remoteIp << std::endl;
    ofs << "remote_port = " << networkConfig_.remotePort << std::endl;
    ofs << "connection_timeout_ms = " << networkConfig_.connectionTimeoutMs << std::endl;
    ofs << "retry_interval_ms = " << networkConfig_.retryIntervalMs << std::endl;
    ofs << std::endl;

    // 同步配置
    ofs << "[sync]" << std::endl;
    ofs << "auto_start = " << (syncConfig_.autoStart ? "true" : "false") << std::endl;
    ofs << "minimize_to_tray = " << (syncConfig_.minimizeToTray ? "true" : "false") << std::endl;
    ofs << "start_with_windows = " << (syncConfig_.startWithWindows ? "true" : "false") << std::endl;
    ofs << "sync_interval_ms = " << syncConfig_.syncIntervalMs << std::endl;
    ofs << "max_retries = " << syncConfig_.maxRetries << std::endl;
    ofs << "resolve_conflicts_automatically = "
        << (syncConfig_.resolveConflictsAutomatically ? "true" : "false") << std::endl;
    ofs << "conflict_resolution_strategy = " << syncConfig_.conflictResolutionStrategy << std::endl;
    ofs << "mapping_file = " << syncConfig_.mappingFile << std::endl;

    ofs.close();

    std::cout << "[ConfigManager] 配置已保存到: " << savePath << std::endl;
    return true;
}

} // namespace dbsync
