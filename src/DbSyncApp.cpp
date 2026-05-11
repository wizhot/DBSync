/**
 * @file DbSyncApp.cpp
 * @brief 数据库同步应用程序主类实现
 * @details 实现应用程序的初始化、数据库连接、主循环等功能。
 *          支持 SQLite (grasp.db) <-> Firebird (SALES.FDB) 异构数据库同步。
 */

#include "DbSyncApp.h"
#include "SyncManager.h"
#include "ConfigManager.h"
#include "MappingManager.h"
#include "Logger.h"
#include <iostream>

namespace dbsync {

// ==================== 构造与析构 ====================

DbSyncApp::DbSyncApp()
    : initialized_(false)
{
}

DbSyncApp::~DbSyncApp()
{
    Shutdown();
}

// ==================== 初始化 ====================

bool DbSyncApp::Initialize()
{
    std::cout << "[DbSyncApp] 正在初始化 DbSync 应用程序..." << std::endl;

    // 1. 初始化日志系统
    if (!InitializeLogger()) {
        std::cerr << "[DbSyncApp] 日志系统初始化失败" << std::endl;
        // 日志失败不阻塞应用启动
    }

    // 2. 初始化数据库连接（加载映射配置、连接数据库）
    if (!InitializeDatabase()) {
        std::cerr << "[DbSyncApp] 数据库初始化失败" << std::endl;
        return false;
    }

    initialized_ = true;
    std::cout << "[DbSyncApp] 应用程序初始化完成" << std::endl;
    return true;
}

// ==================== 数据库初始化 ====================

bool DbSyncApp::InitializeDatabase()
{
    std::cout << "[DbSyncApp] 正在初始化数据库..." << std::endl;

    // 1. 加载配置文件
    auto& configMgr = ConfigManager::GetInstance();
    if (!configMgr.LoadConfig()) {
        std::cerr << "[DbSyncApp] 加载配置文件失败，使用默认配置" << std::endl;
    }

    // 获取数据库配置
    auto localConfig = configMgr.GetLocalDbConfig();
    auto remoteConfig = configMgr.GetRemoteDbConfig();
    auto syncConfig = configMgr.GetSyncConfig();

    // 2. 在日志中显示数据库类型信息
    std::cout << "[DbSyncApp] 本地数据库: " << localConfig.database
              << " (类型: " << localConfig.dbType << ")" << std::endl;
    std::cout << "[DbSyncApp] 远程数据库: " << remoteConfig.database
              << " (类型: " << remoteConfig.dbType << ")" << std::endl;
    std::cout << "[DbSyncApp] 同步模式: SQLite (" << localConfig.database
              << ") <-> Firebird (" << remoteConfig.database << ")" << std::endl;

    // 3. 加载映射配置文件
    auto& mappingMgr = MappingManager::GetInstance();
    if (!mappingMgr.LoadMapping(syncConfig.mappingFile)) {
        std::cerr << "[DbSyncApp] 加载映射配置文件失败: "
                  << syncConfig.mappingFile << std::endl;
        return false;
    }

    const auto& mappings = mappingMgr.GetTableMappings();
    std::cout << "[DbSyncApp] 映射配置加载成功，共 " << mappings.size()
              << " 条表映射规则" << std::endl;

    // 打印映射表列表
    for (const auto& m : mappings) {
        std::cout << "  - " << m.sourceTable << " (" << m.sourceDb << ") -> "
                  << m.targetTable << " (" << m.targetDb << ")" << std::endl;
    }

    // 4. 创建并初始化同步管理器
    syncManager_ = std::make_unique<SyncManager>();
    if (!syncManager_->Initialize()) {
        std::cerr << "[DbSyncApp] 同步管理器初始化失败" << std::endl;
        return false;
    }

    std::cout << "[DbSyncApp] 数据库初始化完成" << std::endl;
    return true;
}

// ==================== 日志初始化 ====================

bool DbSyncApp::InitializeLogger()
{
    // TODO: 实现日志系统初始化
    // logger_ = std::make_unique<Logger>();
    // if (!logger_->Initialize("config/logging.conf")) {
    //     return false;
    // }
    return true;
}

// ==================== 运行 ====================

int DbSyncApp::Run()
{
    if (!initialized_) {
        std::cerr << "[DbSyncApp] 应用程序未初始化，请先调用 Initialize()" << std::endl;
        return -1;
    }

    std::cout << "[DbSyncApp] DbSync 正在运行..." << std::endl;

    // 启动自动同步
    if (syncManager_) {
        auto& configMgr = ConfigManager::GetInstance();
        if (configMgr.GetSyncConfig().autoStart) {
            syncManager_->StartSync();
        }
    }

    // 主循环
    // TODO: 集成 GUI 事件循环或控制台命令处理
    // 当前为简单实现，按回车退出
    std::cout << "[DbSyncApp] 按 Enter 键退出..." << std::endl;
    std::cin.get();

    return 0;
}

// ==================== 关闭 ====================

void DbSyncApp::Shutdown()
{
    if (!initialized_) {
        return;
    }

    std::cout << "[DbSyncApp] 正在关闭应用程序..." << std::endl;

    // 停止同步
    if (syncManager_) {
        syncManager_->Shutdown();
        syncManager_.reset();
    }

    // 关闭日志
    if (logger_) {
        logger_.reset();
    }

    initialized_ = false;
    std::cout << "[DbSyncApp] 应用程序已关闭" << std::endl;
}

} // namespace dbsync
