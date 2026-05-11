/**
 * @file DbSyncApp.h
 * @brief 数据库同步应用程序主类头文件
 * @details DbSyncApp 是整个同步工具的应用层入口，负责：
 *          - 应用程序生命周期管理（初始化、运行、退出）
 *          - 数据库初始化（加载映射配置、连接 SQLite 和 Firebird）
 *          - 同步管理器的创建和协调
 *          - 系统托盘集成
 *          - 日志管理
 */

#pragma once

#include <string>
#include <memory>

namespace dbsync {

class SyncManager;
class Logger;

/**
 * @class DbSyncApp
 * @brief 数据库同步应用程序主类
 */
class DbSyncApp {
public:
    DbSyncApp();
    ~DbSyncApp();

    /**
     * @brief 初始化应用程序
     * @return 成功返回 true
     */
    bool Initialize();

    /**
     * @brief 运行应用程序主循环
     * @return 退出码
     */
    int Run();

    /**
     * @brief 关闭应用程序
     */
    void Shutdown();

private:
    /**
     * @brief 初始化数据库连接
     * @details 加载映射配置文件，连接 SQLite 和 Firebird 数据库，
     *          在日志中显示数据库类型信息
     * @return 成功返回 true
     */
    bool InitializeDatabase();

    /**
     * @brief 初始化日志系统
     * @return 成功返回 true
     */
    bool InitializeLogger();

    std::unique_ptr<SyncManager> syncManager_;  ///< 同步管理器
    std::unique_ptr<Logger> logger_;            ///< 日志管理器
    bool initialized_;                          ///< 是否已初始化
};

} // namespace dbsync
