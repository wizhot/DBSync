/**
 * @file SyncManager.h
 * @brief 同步管理器头文件
 * @details 管理 SQLite ↔ Firebird 异构数据库之间的双向数据同步。
 *          通过 MappingManager 获取映射规则，只同步有映射的表和字段。
 *          支持双向同步（SQLite→Firebird 和 Firebird→SQLite）。
 */

#pragma once

#include "Common.h"
#include "FirebirdManager.h"
#include "SqliteManager.h"
#include "NetworkManager.h"
#include "ConflictResolver.h"
#include "MappingManager.h"

namespace dbsync {

/**
 * @class SyncManager
 * @brief 同步管理器
 * @details 负责：
 *          - 初始化本地 SQLite 和远程 Firebird 数据库连接
 *          - 加载映射规则，按规则同步指定的表和字段
 *          - 支持双向同步（SQLite→Firebird 和 Firebird→SQLite）
 *          - 通过网络管理器实现远程节点间的变更推送
 *          - 使用冲突解决器处理同步冲突
 *          - 线程安全（使用 atomic 和 mutex）
 */
class SyncManager {
public:
    /**
     * @brief 构造函数
     */
    SyncManager();

    /**
     * @brief 析构函数，自动停止同步并释放资源
     */
    ~SyncManager();

    // ==================== 生命周期管理 ====================

    /**
     * @brief 初始化同步管理器
     * @details 读取配置，连接 SQLite 和 Firebird 数据库，
     *          加载映射规则，初始化网络管理器，设置变更追踪
     * @return 成功返回 true
     */
    bool Initialize();

    /**
     * @brief 关闭同步管理器
     * @details 停止同步线程，断开数据库连接，释放资源
     */
    void Shutdown();

    // ==================== 同步控制 ====================

    /**
     * @brief 启动自动同步
     * @details 启动同步线程和监控线程，按配置的间隔定时同步
     * @return 成功返回 true
     */
    bool StartSync();

    /**
     * @brief 停止自动同步
     * @details 停止同步线程和监控线程
     */
    void StopSync();

    /**
     * @brief 查询是否正在同步中
     * @return 正在同步返回 true
     */
    bool IsSyncing() const { return syncing_; }

    /**
     * @brief 手动触发一次同步
     * @details 如果当前没有在同步中，则立即执行一次同步
     * @return 成功触发返回 true
     */
    bool TriggerSync();

    // ==================== 状态查询 ====================

    /**
     * @brief 查询数据库是否已连接
     * @return 两个数据库都已连接返回 true
     */
    bool IsConnected() const;

    /**
     * @brief 获取待同步的变更数量
     * @return 两个数据库的待同步变更总数
     */
    int GetPendingChangesCount() const;

    /**
     * @brief 获取上次同步时间
     * @return 上次成功同步的时间字符串
     */
    std::string GetLastSyncTime() const;

    /**
     * @brief 获取当前同步状态描述
     * @return 状态描述字符串
     */
    std::string GetStatus() const;

    // ==================== 回调设置 ====================

    /// 同步状态回调函数类型（状态描述, 进度百分比）
    using SyncStatusCallback = std::function<void(const std::string& status, int progress)>;

    /// 错误回调函数类型（错误信息）
    using ErrorCallback = std::function<void(const std::string& error)>;

    /**
     * @brief 设置同步状态回调
     * @param callback 回调函数
     */
    void SetSyncStatusCallback(SyncStatusCallback callback) { statusCallback_ = callback; }

    /**
     * @brief 设置错误回调
     * @param callback 回调函数
     */
    void SetErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }

private:
    // ==================== 线程函数 ====================

    /**
     * @brief 同步线程主函数
     * @details 按配置的间隔定时执行同步操作
     */
    void SyncThreadFunc();

    /**
     * @brief 监控线程主函数
     * @details 监控网络连接状态和数据库变更
     */
    void MonitorThreadFunc();

    // ==================== 按映射规则同步 ====================

    /**
     * @brief 同步所有有映射的表（双向）
     * @return 成功返回 true
     */
    bool SyncAllTables();

    /**
     * @brief 同步单个表
     * @param mapping 表映射配置
     * @param sqliteToFirebird 同步方向：true=SQLite→Firebird, false=Firebird→SQLite
     * @return 成功返回 true
     */
    bool SyncTable(const TableMapping& mapping, bool sqliteToFirebird);

    // ==================== 数据转换和写入 ====================

    /**
     * @brief 同步插入的记录
     * @param mapping 表映射配置
     * @param sqliteToFirebird 同步方向
     * @param allChanges 已获取的待同步变更列表（Common::ChangeRecord 格式）
     * @return 成功返回 true
     */
    bool SyncInsertedRecords(const TableMapping& mapping, bool sqliteToFirebird,
                             const std::vector<ChangeRecord>& allChanges);

    /**
     * @brief 同步更新的记录
     * @param mapping 表映射配置
     * @param sqliteToFirebird 同步方向
     * @param allChanges 已获取的待同步变更列表（Common::ChangeRecord 格式）
     * @return 成功返回 true
     */
    bool SyncUpdatedRecords(const TableMapping& mapping, bool sqliteToFirebird,
                            const std::vector<ChangeRecord>& allChanges);

    /**
     * @brief 同步删除的记录
     * @param mapping 表映射配置
     * @param sqliteToFirebird 同步方向
     * @param allChanges 已获取的待同步变更列表（Common::ChangeRecord 格式）
     * @return 成功返回 true
     */
    bool SyncDeletedRecords(const TableMapping& mapping, bool sqliteToFirebird,
                            const std::vector<ChangeRecord>& allChanges);

    // ==================== 网络消息处理 ====================

    /**
     * @brief 处理接收到的网络消息
     * @param message 同步消息
     */
    void OnMessageReceived(const SyncMessage& message);

    /**
     * @brief 处理网络连接状态变化
     * @param connected 是否已连接
     */
    void OnConnectionStatusChanged(bool connected);

    // ==================== 序列化 ====================

    /**
     * @brief 序列化变更记录为字符串
     * @param record 变更记录
     * @return 序列化后的字符串
     */
    std::string SerializeChangeRecord(const ChangeRecord& record);

    /**
     * @brief 反序列化字符串为变更记录
     * @param data 序列化字符串
     * @param record 输出变更记录
     * @return 成功返回 true
     */
    bool DeserializeChangeRecord(const std::string& data, ChangeRecord& record);

    // ==================== 成员变量 ====================

    std::unique_ptr<SqliteManager> sqliteDb_;           ///< SQLite 数据库管理器（本地 grasp.db）
    std::unique_ptr<FirebirdManager> firebirdDb_;       ///< Firebird 数据库管理器（远程 SALES.FDB）
    std::unique_ptr<NetworkManager> networkManager_;    ///< 网络管理器
    std::unique_ptr<ConflictResolver> conflictResolver_; ///< 冲突解决器

    std::atomic<bool> syncing_;       ///< 是否正在同步中
    std::atomic<bool> shouldStop_;    ///< 是否应该停止同步线程
    std::thread syncThread_;          ///< 同步线程
    std::thread monitorThread_;       ///< 监控线程

    std::string lastSyncTime_;        ///< 上次同步时间
    mutable std::mutex statsMutex_;   ///< 统计信息互斥锁

    SyncStatusCallback statusCallback_;  ///< 同步状态回调
    ErrorCallback errorCallback_;        ///< 错误回调

    SyncConfig syncConfig_;           ///< 同步配置
};

} // namespace dbsync
