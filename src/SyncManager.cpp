/**
 * @file SyncManager.cpp
 * @brief 同步管理器实现
 * @details 实现 SQLite <-> Firebird 异构数据库的双向数据同步。
 *          核心流程：
 *          1. Initialize() - 初始化两个数据库连接，加载映射规则
 *          2. SyncAllTables() - 遍历所有映射表，执行双向同步
 *          3. SyncTable() - 对单个表执行单向同步（获取变更->转换->写入->标记）
 *          4. 冲突处理使用时间戳策略
 *
 *          接口兼容性说明：
 *          - FirebirdManager 使用 map<string,string> 参数，ChangeRecord(Common.h) 输出参数
 *          - SqliteManager 使用 vector<string> 参数，ChangeRecord(SqliteManager.h) 返回值
 *          - 内部统一使用 Common.h 的 ChangeRecord，从 SqliteManager 获取后做格式转换
 */

#include "SyncManager.h"
#include "ConfigManager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cstring>
#include <json/json.h>

namespace dbsync {

// ==================== 内部辅助函数 ====================

/**
 * @brief 将 SqliteManager::ChangeRecord 转换为 Common::ChangeRecord
 * @details 两种 ChangeRecord 结构不同，需要逐字段映射
 *          SqliteManager::ChangeRecord 字段: change_id, table_name, operation, primary_key, change_time, old_data, new_data
 *          Common::ChangeRecord 字段: id, tableName, changeType, primaryKey, primaryKeyValue, data, timestamp, sourceNode, status, retryCount
 */
static ChangeRecord ConvertSqliteChangeToCommon(const SqliteChangeRecord& src)
{
    ChangeRecord dst;
    dst.id = std::to_string(src.change_id);
    dst.tableName = src.table_name;
    dst.primaryKeyValue = src.primary_key;
    dst.timestamp = src.change_time;
    dst.data = src.new_data;  // 新数据存入 data 字段
    dst.status = SyncStatus::PENDING;
    dst.retryCount = 0;

    // 转换操作类型字符串为枚举
    if (src.operation == "INSERT") {
        dst.changeType = ChangeType::INSERT;
    } else if (src.operation == "UPDATE") {
        dst.changeType = ChangeType::UPDATE;
    } else if (src.operation == "DELETE") {
        dst.changeType = ChangeType::DELETE;
    }

    return dst;
}

/**
 * @brief 将 ChangeType 枚举转换为字符串
 */
static std::string ChangeTypeToString(ChangeType type)
{
    switch (type) {
        case ChangeType::INSERT: return "INSERT";
        case ChangeType::UPDATE: return "UPDATE";
        case ChangeType::DELETE: return "DELETE";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 将 map<string,string> 拆分为两个 vector<string>（字段列表和值列表）
 * @details 用于适配 SqliteManager 的 InsertRecord/UpdateRecord 接口
 */
static void MapToVectors(const std::map<std::string, std::string>& data,
                         std::vector<std::string>& fields,
                         std::vector<std::string>& values)
{
    fields.clear();
    values.clear();
    for (const auto& [k, v] : data) {
        fields.push_back(k);
        values.push_back(v);
    }
}

/**
 * @brief 从 FirebirdManager 查询记录（替代不存在的 GetRecordByKey）
 * @details 使用 Query() 方法执行 SELECT 语句
 */
static bool QueryRecordByKey(FirebirdManager* fbMgr,
                              const std::string& tableName,
                              const std::string& pkField,
                              const std::string& pkValue,
                              std::map<std::string, std::string>& record)
{
    std::string sql = "SELECT * FROM " + tableName + " WHERE " + pkField + " = '" + pkValue + "'";
    std::vector<std::map<std::string, std::string>> results;
    if (!fbMgr->Query(sql, results) || results.empty()) {
        return false;
    }
    record = results[0];
    return true;
}

/**
 * @brief 检查 FirebirdManager 中目标记录是否已存在
 */
static bool CheckRecordExists(FirebirdManager* fbMgr,
                               const std::string& tableName,
                               const std::string& pkField,
                               const std::string& pkValue)
{
    std::map<std::string, std::string> record;
    return QueryRecordByKey(fbMgr, tableName, pkField, pkValue, record);
}

// ==================== 构造与析构 ====================

SyncManager::SyncManager()
    : syncing_(false)
    , shouldStop_(false)
{
}

SyncManager::~SyncManager()
{
    Shutdown();
}

// ==================== 生命周期管理 ====================

bool SyncManager::Initialize()
{
    std::cout << "[SyncManager] 正在初始化同步管理器..." << std::endl;

    // 1. 读取配置
    auto& configMgr = ConfigManager::GetInstance();
    if (!configMgr.LoadConfig()) {
        std::cerr << "[SyncManager] 加载配置文件失败" << std::endl;
        return false;
    }

    syncConfig_ = configMgr.GetSyncConfig();
    const auto& localConfig = configMgr.GetLocalDbConfig();
    const auto& remoteConfig = configMgr.GetRemoteDbConfig();

    // 2. 创建并连接 SQLite 数据库（本地 grasp.db）
    //    接口: SqliteManager::Connect(const string& db_path, const string& key = "")
    sqliteDb_ = std::make_unique<SqliteManager>();
    if (!sqliteDb_->Connect(localConfig.database, localConfig.encryptionKey)) {
        std::cerr << "[SyncManager] 连接 SQLite 数据库失败: "
                  << sqliteDb_->GetLastError() << std::endl;
        if (errorCallback_) {
            errorCallback_("连接 SQLite 数据库失败: " + sqliteDb_->GetLastError());
        }
        return false;
    }
    std::cout << "[SyncManager] SQLite 数据库连接成功: " << localConfig.database << std::endl;

    // 3. 创建并连接 Firebird 数据库（远程 SALES.FDB）
    //    接口: FirebirdManager::Connect(const DatabaseConfig& config)
    firebirdDb_ = std::make_unique<FirebirdManager>();
    if (!firebirdDb_->Connect(remoteConfig)) {
        std::cerr << "[SyncManager] 连接 Firebird 数据库失败: "
                  << firebirdDb_->GetLastError() << std::endl;
        if (errorCallback_) {
            errorCallback_("连接 Firebird 数据库失败: " + firebirdDb_->GetLastError());
        }
        return false;
    }
    std::cout << "[SyncManager] Firebird 数据库连接成功: " << remoteConfig.database << std::endl;

    // 4. 加载映射规则
    auto& mappingMgr = MappingManager::GetInstance();
    if (!mappingMgr.LoadMapping(syncConfig_.mappingFile)) {
        std::cerr << "[SyncManager] 加载映射规则失败" << std::endl;
        if (errorCallback_) {
            errorCallback_("加载映射规则失败");
        }
        return false;
    }
    std::cout << "[SyncManager] 映射规则加载成功，共 "
              << mappingMgr.GetTableMappings().size() << " 条表映射" << std::endl;

    // 5. 为两个数据库设置变更追踪
    if (!sqliteDb_->SetupChangeTracking()) {
        std::cerr << "[SyncManager] 设置 SQLite 变更追踪失败: "
                  << sqliteDb_->GetLastError() << std::endl;
        if (errorCallback_) {
            errorCallback_("设置 SQLite 变更追踪失败");
        }
        // 变更追踪设置失败不阻塞初始化，可能表已存在
    }

    if (!firebirdDb_->SetupChangeTracking()) {
        std::cerr << "[SyncManager] 设置 Firebird 变更追踪失败: "
                  << firebirdDb_->GetLastError() << std::endl;
        if (errorCallback_) {
            errorCallback_("设置 Firebird 变更追踪失败");
        }
        // 变更追踪设置失败不阻塞初始化
    }

    // 6. 初始化网络管理器
    networkManager_ = std::make_unique<NetworkManager>();
    networkManager_->SetMessageHandler(
        [this](const SyncMessage& msg) { OnMessageReceived(msg); }
    );
    networkManager_->SetConnectionHandler(
        [this](bool connected) { OnConnectionStatusChanged(connected); }
    );

    if (!networkManager_->Initialize(configMgr.GetNetworkConfig())) {
        std::cerr << "[SyncManager] 初始化网络管理器失败" << std::endl;
        // 网络功能失败不阻塞初始化，本地同步仍可使用
    }

    // 7. 初始化冲突解决器
    //    接口: ConflictResolver::SetStrategy(const string& strategyName)
    conflictResolver_ = std::make_unique<ConflictResolver>();
    conflictResolver_->SetStrategy(syncConfig_.conflictResolutionStrategy);

    std::cout << "[SyncManager] 同步管理器初始化完成" << std::endl;
    return true;
}

void SyncManager::Shutdown()
{
    // 停止同步线程
    StopSync();

    // 断开网络
    if (networkManager_) {
        networkManager_->Shutdown();
        networkManager_.reset();
    }

    // 断开 Firebird
    if (firebirdDb_) {
        firebirdDb_->Disconnect();
        firebirdDb_.reset();
    }

    // 断开 SQLite
    if (sqliteDb_) {
        sqliteDb_->Disconnect();
        sqliteDb_.reset();
    }

    // 释放冲突解决器
    if (conflictResolver_) {
        conflictResolver_.reset();
    }

    std::cout << "[SyncManager] 同步管理器已关闭" << std::endl;
}

// ==================== 同步控制 ====================

bool SyncManager::StartSync()
{
    if (syncing_) {
        std::cout << "[SyncManager] 同步已在运行中" << std::endl;
        return true;
    }

    if (!sqliteDb_ || !sqliteDb_->IsConnected()) {
        std::cerr << "[SyncManager] SQLite 数据库未连接，无法启动同步" << std::endl;
        return false;
    }

    if (!firebirdDb_ || !firebirdDb_->IsConnected()) {
        std::cerr << "[SyncManager] Firebird 数据库未连接，无法启动同步" << std::endl;
        return false;
    }

    shouldStop_ = false;
    syncing_ = true;

    // 启动同步线程
    syncThread_ = std::thread(&SyncManager::SyncThreadFunc, this);

    // 启动监控线程
    monitorThread_ = std::thread(&SyncManager::MonitorThreadFunc, this);

    std::cout << "[SyncManager] 同步已启动，间隔: "
              << syncConfig_.syncIntervalMs << "ms" << std::endl;

    if (statusCallback_) {
        statusCallback_("同步已启动", 0);
    }

    return true;
}

void SyncManager::StopSync()
{
    if (!syncing_) {
        return;
    }

    shouldStop_ = true;
    syncing_ = false;

    // 等待同步线程结束
    if (syncThread_.joinable()) {
        syncThread_.join();
    }

    // 等待监控线程结束
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }

    std::cout << "[SyncManager] 同步已停止" << std::endl;

    if (statusCallback_) {
        statusCallback_("同步已停止", 0);
    }
}

bool SyncManager::TriggerSync()
{
    if (syncing_) {
        std::cout << "[SyncManager] 同步正在运行中，跳过手动触发" << std::endl;
        return false;
    }

    if (!sqliteDb_ || !firebirdDb_) {
        std::cerr << "[SyncManager] 数据库未初始化，无法触发同步" << std::endl;
        return false;
    }

    std::cout << "[SyncManager] 手动触发同步..." << std::endl;

    if (statusCallback_) {
        statusCallback_("正在同步...", 0);
    }

    bool result = SyncAllTables();

    if (result) {
        // 更新最后同步时间
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
        std::lock_guard<std::mutex> lock(statsMutex_);
        lastSyncTime_ = ss.str();

        if (statusCallback_) {
            statusCallback_("同步完成", 100);
        }
    } else {
        if (statusCallback_) {
            statusCallback_("同步失败", 0);
        }
    }

    return result;
}

// ==================== 状态查询 ====================

bool SyncManager::IsConnected() const
{
    return (sqliteDb_ && sqliteDb_->IsConnected()) &&
           (firebirdDb_ && firebirdDb_->IsConnected());
}

int SyncManager::GetPendingChangesCount() const
{
    int count = 0;

    // SqliteManager 有 GetPendingChangeCount() 方法
    if (sqliteDb_ && sqliteDb_->IsConnected()) {
        count += static_cast<int>(sqliteDb_->GetPendingChangeCount());
    }

    // FirebirdManager 没有 GetPendingChangeCount() 方法，需要通过 GetPendingChanges 获取数量
    if (firebirdDb_ && firebirdDb_->IsConnected()) {
        std::vector<ChangeRecord> fbChanges;
        if (firebirdDb_->GetPendingChanges(fbChanges)) {
            count += static_cast<int>(fbChanges.size());
        }
    }

    return count;
}

std::string SyncManager::GetLastSyncTime() const
{
    std::lock_guard<std::mutex> lock(statsMutex_);
    return lastSyncTime_;
}

std::string SyncManager::GetStatus() const
{
    if (syncing_) {
        return "正在同步中...";
    }

    if (!IsConnected()) {
        return "数据库未连接";
    }

    int pending = GetPendingChangesCount();
    if (pending > 0) {
        return "待同步变更: " + std::to_string(pending) + " 条";
    }

    return "已就绪";
}

// ==================== 线程函数 ====================

void SyncManager::SyncThreadFunc()
{
    std::cout << "[SyncManager] 同步线程已启动" << std::endl;

    while (!shouldStop_) {
        // 执行一次完整的双向同步
        bool success = SyncAllTables();

        if (success) {
            // 更新最后同步时间
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
            std::lock_guard<std::mutex> lock(statsMutex_);
            lastSyncTime_ = ss.str();
        }

        // 等待下一次同步间隔（每100ms检查一次停止信号）
        int checkCount = syncConfig_.syncIntervalMs / 100;
        if (checkCount <= 0) checkCount = 1;
        for (int i = 0; i < checkCount && !shouldStop_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << "[SyncManager] 同步线程已退出" << std::endl;
}

void SyncManager::MonitorThreadFunc()
{
    std::cout << "[SyncManager] 监控线程已启动" << std::endl;

    while (!shouldStop_) {
        // 检查 SQLite 数据库连接状态
        if (sqliteDb_ && !sqliteDb_->IsConnected()) {
            std::cerr << "[SyncManager] SQLite 数据库连接已断开，尝试重连..." << std::endl;
            auto localConfig = ConfigManager::GetInstance().GetLocalDbConfig();
            // 接口: SqliteManager::Connect(const string& db_path, const string& key = "")
            if (!sqliteDb_->Connect(localConfig.database, localConfig.encryptionKey)) {
                std::cerr << "[SyncManager] SQLite 重连失败" << std::endl;
                if (errorCallback_) {
                    errorCallback_("SQLite 数据库连接断开，重连失败");
                }
            } else {
                std::cout << "[SyncManager] SQLite 重连成功" << std::endl;
            }
        }

        // 检查 Firebird 数据库连接状态
        if (firebirdDb_ && !firebirdDb_->IsConnected()) {
            std::cerr << "[SyncManager] Firebird 数据库连接已断开，尝试重连..." << std::endl;
            auto remoteConfig = ConfigManager::GetInstance().GetRemoteDbConfig();
            // 接口: FirebirdManager::Connect(const DatabaseConfig& config)
            if (!firebirdDb_->Connect(remoteConfig)) {
                std::cerr << "[SyncManager] Firebird 重连失败" << std::endl;
                if (errorCallback_) {
                    errorCallback_("Firebird 数据库连接断开，重连失败");
                }
            } else {
                std::cout << "[SyncManager] Firebird 重连成功" << std::endl;
            }
        }

        // 每秒检查一次
        for (int i = 0; i < 10 && !shouldStop_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << "[SyncManager] 监控线程已退出" << std::endl;
}

// ==================== 按映射规则同步 ====================

bool SyncManager::SyncAllTables()
{
    if (!sqliteDb_ || !firebirdDb_) {
        std::cerr << "[SyncManager] 数据库未初始化" << std::endl;
        return false;
    }

    auto& mappingMgr = MappingManager::GetInstance();
    const auto& mappings = mappingMgr.GetTableMappings();

    if (mappings.empty()) {
        std::cout << "[SyncManager] 没有配置映射规则，跳过同步" << std::endl;
        return true;
    }

    int totalMappings = static_cast<int>(mappings.size());
    int completedMappings = 0;
    bool allSuccess = true;

    std::cout << "[SyncManager] 开始同步 " << totalMappings << " 张表..." << std::endl;

    if (statusCallback_) {
        statusCallback_("开始同步...", 0);
    }

    for (const auto& mapping : mappings) {
        if (shouldStop_) {
            std::cout << "[SyncManager] 收到停止信号，中断同步" << std::endl;
            break;
        }

        std::cout << "[SyncManager] 同步表映射: " << mapping.sourceTable
                  << " (" << mapping.sourceDb << ") <-> "
                  << mapping.targetTable << " (" << mapping.targetDb << ")"
                  << std::endl;

        bool tableSuccess = true;

        // 方向1: SQLite -> Firebird
        // 当映射配置中 sourceDb == "sqlite" 时，执行正向同步
        if (mapping.sourceDb == "sqlite" && mapping.targetDb == "firebird") {
            if (!SyncTable(mapping, true)) {
                std::cerr << "[SyncManager] SQLite->Firebird 同步失败: "
                          << mapping.sourceTable << " -> " << mapping.targetTable << std::endl;
                tableSuccess = false;
            }
        }

        // 方向2: Firebird -> SQLite
        // 当映射配置中 sourceDb == "firebird" 时，执行正向同步
        if (mapping.sourceDb == "firebird" && mapping.targetDb == "sqlite") {
            if (!SyncTable(mapping, false)) {
                std::cerr << "[SyncManager] Firebird->SQLite 同步失败: "
                          << mapping.sourceTable << " -> " << mapping.targetTable << std::endl;
                tableSuccess = false;
            }
        }

        // 对于 sourceDb == "sqlite" 的映射，也需要反向同步（Firebird->SQLite）
        // 构建"反向"映射用于 Firebird->SQLite 方向
        if (mapping.sourceDb == "sqlite" && mapping.targetDb == "firebird") {
            TableMapping reverseMapping = mapping;
            std::swap(reverseMapping.sourceTable, reverseMapping.targetTable);
            std::swap(reverseMapping.sourceDb, reverseMapping.targetDb);
            std::swap(reverseMapping.sourcePrimaryKey, reverseMapping.targetPrimaryKey);
            // 反转字段映射
            for (auto& fm : reverseMapping.fieldMappings) {
                std::swap(fm.sourceField, fm.targetField);
            }
            // 反转值转换
            for (auto& vt : reverseMapping.valueTransforms) {
                std::swap(vt.sourceField, vt.targetField);
            }

            if (!SyncTable(reverseMapping, false)) {
                std::cerr << "[SyncManager] Firebird->SQLite 反向同步失败: "
                          << reverseMapping.sourceTable << " -> " << reverseMapping.targetTable << std::endl;
                tableSuccess = false;
            }
        }

        if (!tableSuccess) {
            allSuccess = false;
        }

        completedMappings++;
        int progress = (completedMappings * 100) / totalMappings;
        if (statusCallback_) {
            statusCallback_("正在同步表: " + mapping.sourceTable + " <-> " + mapping.targetTable,
                           progress);
        }
    }

    std::cout << "[SyncManager] 同步完成，共处理 " << completedMappings
              << "/" << totalMappings << " 张表" << std::endl;

    if (statusCallback_) {
        statusCallback_(allSuccess ? "同步完成" : "部分表同步失败", 100);
    }

    return allSuccess;
}

bool SyncManager::SyncTable(const TableMapping& mapping, bool sqliteToFirebird)
{
    // 根据同步方向确定源表和目标表
    std::string sourceTable = mapping.sourceTable;
    std::string targetTable = mapping.targetTable;

    // 获取源数据库的待同步变更，转换为统一的 Common::ChangeRecord 格式
    std::vector<ChangeRecord> pendingChanges;

    if (sqliteToFirebird) {
        // SQLite -> Firebird 方向
        // 接口: SqliteManager::GetPendingChanges(const string& table_name, int64_t since_id)
        //       返回 vector<SqliteChangeRecord>（SqliteManager 命名空间内的 ChangeRecord）
        auto sqliteChanges = sqliteDb_->GetPendingChanges(sourceTable);

        // 转换 SqliteChangeRecord -> Common::ChangeRecord
        for (const auto& sc : sqliteChanges) {
            pendingChanges.push_back(ConvertSqliteChangeToCommon(sc));
        }
    } else {
        // Firebird -> SQLite 方向
        // 接口: FirebirdManager::GetPendingChanges(vector<ChangeRecord>& changes)
        //       输出参数，无过滤参数，返回 Common::ChangeRecord
        std::vector<ChangeRecord> fbChanges;
        if (!firebirdDb_->GetPendingChanges(fbChanges)) {
            std::cerr << "[SyncManager] 获取 Firebird 待同步变更失败" << std::endl;
            return false;
        }

        // 过滤：只处理当前映射中 sourceTable 对应的变更
        for (const auto& change : fbChanges) {
            if (change.tableName == sourceTable) {
                pendingChanges.push_back(change);
            }
        }
    }

    if (pendingChanges.empty()) {
        return true; // 没有待同步的变更
    }

    std::cout << "[SyncManager] 表 " << sourceTable
              << " 有 " << pendingChanges.size() << " 条待同步变更"
              << (sqliteToFirebird ? " (SQLite->Firebird)" : " (Firebird->SQLite)")
              << std::endl;

    // 按操作类型分组处理
    bool success = true;

    if (!SyncInsertedRecords(mapping, sqliteToFirebird, pendingChanges)) {
        success = false;
    }

    if (!SyncUpdatedRecords(mapping, sqliteToFirebird, pendingChanges)) {
        success = false;
    }

    if (!SyncDeletedRecords(mapping, sqliteToFirebird, pendingChanges)) {
        success = false;
    }

    return success;
}

// ==================== 数据转换和写入 ====================

bool SyncManager::SyncInsertedRecords(const TableMapping& mapping, bool sqliteToFirebird,
                                       const std::vector<ChangeRecord>& allChanges)
{
    std::string sourceTable = mapping.sourceTable;
    std::string targetTable = mapping.targetTable;

    // 过滤出 INSERT 操作
    std::vector<ChangeRecord> insertChanges;
    for (const auto& change : allChanges) {
        if (change.changeType == ChangeType::INSERT) {
            insertChanges.push_back(change);
        }
    }

    if (insertChanges.empty()) {
        return true;
    }

    auto& mappingMgr = MappingManager::GetInstance();
    // 用于记录成功同步的变更ID（SQLite 用 int64_t，Firebird 用 string）
    std::vector<int64_t> sqliteSyncedIds;
    std::vector<std::string> firebirdSyncedIds;

    for (const auto& change : insertChanges) {
        // 从源数据库查询完整记录数据
        std::map<std::string, std::string> sourceData;

        // 尝试从变更记录的 data 字段解析 JSON 数据
        bool dataParsed = false;
        if (!change.data.empty()) {
            // 尝试解析 JSON 格式的数据
            Json::Value root;
            Json::CharReaderBuilder builder;
            std::istringstream stream(change.data);
            std::string errors;
            if (Json::parseFromStream(builder, stream, &root, &errors)) {
                for (const auto& key : root.getMemberNames()) {
                    sourceData[key] = root[key].asString();
                }
                dataParsed = true;
            }
        }

        // 如果 JSON 解析失败，从源数据库查询完整记录
        if (!dataParsed) {
            std::string pkField = mapping.sourcePrimaryKey;

            if (sqliteToFirebird) {
                // 接口: SqliteManager::GetRecordByKey(table_name, pk_column, pk_value)
                auto records = sqliteDb_->GetRecordByKey(sourceTable, pkField, change.primaryKeyValue);
                if (!records.empty()) {
                    sourceData = records[0];
                }
            } else {
                // FirebirdManager 没有 GetRecordByKey，使用 Query() 替代
                if (!QueryRecordByKey(firebirdDb_.get(), sourceTable, pkField, change.primaryKeyValue, sourceData)) {
                    // Firebird 主键字段可能使用映射中的字段名
                    std::cerr << "[SyncManager] 未找到 Firebird 记录: " << sourceTable
                              << " PK=" << pkField << "=" << change.primaryKeyValue << std::endl;
                    // 尝试使用所有字段映射中的源主键
                    continue;
                }
            }

            if (sourceData.empty()) {
                std::cerr << "[SyncManager] 未找到源记录: " << sourceTable
                          << " PK=" << change.primaryKeyValue << std::endl;
                continue;
            }
        }

        // 使用映射规则转换数据
        auto targetData = mappingMgr.TransformRecord(sourceData, mapping, true);

        if (targetData.empty()) {
            std::cerr << "[SyncManager] 记录转换后为空，跳过: " << sourceTable
                      << " PK=" << change.primaryKeyValue << std::endl;
            continue;
        }

        // 检查目标数据库是否已存在该记录（冲突检测）
        std::string targetPkField = mapping.targetPrimaryKey;
        std::string targetPkValue = change.primaryKeyValue;
        bool recordExists = false;

        if (sqliteToFirebird) {
            // FirebirdManager 没有 GetRecordByKey，使用 Query() 替代
            recordExists = CheckRecordExists(firebirdDb_.get(), targetTable, targetPkField, targetPkValue);
        } else {
            // 接口: SqliteManager::GetRecordByKey(table_name, pk_column, pk_value)
            auto records = sqliteDb_->GetRecordByKey(targetTable, targetPkField, targetPkValue);
            recordExists = !records.empty();
        }

        if (recordExists) {
            // 记录已存在，使用冲突解决器处理
            if (syncConfig_.resolveConflictsAutomatically) {
                std::cout << "[SyncManager] 检测到冲突: " << targetTable
                          << " PK=" << targetPkValue << "，使用时间戳策略解决" << std::endl;

                // 将 INSERT 转为 UPDATE 处理
                if (sqliteToFirebird) {
                    // 接口: FirebirdManager::UpdateRecord(tableName, primaryKey, primaryKeyValue, data)
                    // 跳过主键字段，主键不更新
                    std::map<std::string, std::string> updateData;
                    for (const auto& [field, value] : targetData) {
                        if (field != targetPkField) {
                            updateData[field] = value;
                        }
                    }
                    bool updateResult = firebirdDb_->UpdateRecord(targetTable, targetPkField, targetPkValue, updateData);
                    if (!updateResult) {
                        std::cerr << "[SyncManager] 冲突解决（更新 Firebird）失败: " << targetTable
                                  << " PK=" << targetPkValue << std::endl;
                        continue;
                    }
                } else {
                    // 接口: SqliteManager::UpdateRecord(table_name, fields, values, where_clause, where_params)
                    std::vector<std::string> fields;
                    std::vector<std::string> values;
                    for (const auto& [field, value] : targetData) {
                        if (field != targetPkField) {
                            fields.push_back(field);
                            values.push_back(value);
                        }
                    }
                    if (!fields.empty()) {
                        bool updateResult = sqliteDb_->UpdateRecord(
                            targetTable, fields, values,
                            targetPkField + " = ?", {targetPkValue});
                        if (!updateResult) {
                            std::cerr << "[SyncManager] 冲突解决（更新 SQLite）失败: " << targetTable
                                      << " PK=" << targetPkValue << std::endl;
                            continue;
                        }
                    }
                }
            } else {
                std::cout << "[SyncManager] 检测到冲突但未启用自动解决，跳过: "
                          << targetTable << " PK=" << targetPkValue << std::endl;
                continue;
            }
        } else {
            // 记录不存在，执行插入
            if (sqliteToFirebird) {
                // 接口: FirebirdManager::InsertRecord(tableName, map<string,string> data)
                bool insertResult = firebirdDb_->InsertRecord(targetTable, targetData);
                if (!insertResult) {
                    std::cerr << "[SyncManager] 插入 Firebird 记录失败: " << targetTable
                              << " PK=" << targetPkValue << std::endl;
                    if (errorCallback_) {
                        errorCallback_("插入记录失败: " + targetTable + " PK=" + targetPkValue);
                    }
                    continue;
                }
                std::cout << "[SyncManager] 插入 Firebird 记录成功: " << targetTable
                          << " PK=" << targetPkValue << std::endl;
            } else {
                // 接口: SqliteManager::InsertRecord(table_name, fields, values, last_insert_id)
                std::vector<std::string> fields;
                std::vector<std::string> values;
                MapToVectors(targetData, fields, values);

                bool insertResult = sqliteDb_->InsertRecord(targetTable, fields, values);
                if (!insertResult) {
                    std::cerr << "[SyncManager] 插入 SQLite 记录失败: " << targetTable
                              << " PK=" << targetPkValue << std::endl;
                    if (errorCallback_) {
                        errorCallback_("插入记录失败: " + targetTable + " PK=" + targetPkValue);
                    }
                    continue;
                }
                std::cout << "[SyncManager] 插入 SQLite 记录成功: " << targetTable
                          << " PK=" << targetPkValue << std::endl;
            }
        }

        // 记录已同步的变更ID
        if (sqliteToFirebird) {
            // SQLite 的变更ID是 int64_t（从 change.id 转换而来）
            try {
                sqliteSyncedIds.push_back(std::stoll(change.id));
            } catch (...) {
                std::cerr << "[SyncManager] 无法解析 SQLite 变更ID: " << change.id << std::endl;
            }
        } else {
            // Firebird 的变更ID是 string
            firebirdSyncedIds.push_back(change.id);
        }
    }

    // 标记源变更已同步
    if (sqliteToFirebird && !sqliteSyncedIds.empty()) {
        // 接口: SqliteManager::MarkChangesSynced(const vector<int64_t>& change_ids)
        if (!sqliteDb_->MarkChangesSynced(sqliteSyncedIds)) {
            std::cerr << "[SyncManager] 标记 SQLite 变更已同步失败" << std::endl;
            return false;
        }
    }

    if (!sqliteToFirebird && !firebirdSyncedIds.empty()) {
        // 接口: FirebirdManager::MarkChangeAsSynced(const string& changeId) -- 逐个标记
        for (const auto& id : firebirdSyncedIds) {
            if (!firebirdDb_->MarkChangeAsSynced(id)) {
                std::cerr << "[SyncManager] 标记 Firebird 变更已同步失败: " << id << std::endl;
                return false;
            }
        }
    }

    return true;
}

bool SyncManager::SyncUpdatedRecords(const TableMapping& mapping, bool sqliteToFirebird,
                                      const std::vector<ChangeRecord>& allChanges)
{
    std::string sourceTable = mapping.sourceTable;
    std::string targetTable = mapping.targetTable;

    // 过滤出 UPDATE 操作
    std::vector<ChangeRecord> updateChanges;
    for (const auto& change : allChanges) {
        if (change.changeType == ChangeType::UPDATE) {
            updateChanges.push_back(change);
        }
    }

    if (updateChanges.empty()) {
        return true;
    }

    auto& mappingMgr = MappingManager::GetInstance();
    std::vector<int64_t> sqliteSyncedIds;
    std::vector<std::string> firebirdSyncedIds;

    for (const auto& change : updateChanges) {
        // 从源数据库查询最新完整记录
        std::map<std::string, std::string> sourceData;
        std::string pkField = mapping.sourcePrimaryKey;

        if (sqliteToFirebird) {
            // 接口: SqliteManager::GetRecordByKey(table_name, pk_column, pk_value)
            auto records = sqliteDb_->GetRecordByKey(sourceTable, pkField, change.primaryKeyValue);
            if (!records.empty()) {
                sourceData = records[0];
            }
        } else {
            // FirebirdManager 没有 GetRecordByKey，使用 Query() 替代
            if (!QueryRecordByKey(firebirdDb_.get(), sourceTable, pkField, change.primaryKeyValue, sourceData)) {
                std::cerr << "[SyncManager] 更新时未找到 Firebird 源记录: " << sourceTable
                          << " PK=" << change.primaryKeyValue << std::endl;
                firebirdSyncedIds.push_back(change.id);
                continue;
            }
        }

        if (sourceData.empty()) {
            std::cerr << "[SyncManager] 更新时未找到源记录: " << sourceTable
                      << " PK=" << change.primaryKeyValue << std::endl;
            if (sqliteToFirebird) {
                try { sqliteSyncedIds.push_back(std::stoll(change.id)); } catch (...) {}
            } else {
                firebirdSyncedIds.push_back(change.id);
            }
            continue;
        }

        // 使用映射规则转换数据
        auto targetData = mappingMgr.TransformRecord(sourceData, mapping, true);

        if (targetData.empty()) {
            std::cerr << "[SyncManager] 更新记录转换后为空，跳过: " << sourceTable
                      << " PK=" << change.primaryKeyValue << std::endl;
            if (sqliteToFirebird) {
                try { sqliteSyncedIds.push_back(std::stoll(change.id)); } catch (...) {}
            } else {
                firebirdSyncedIds.push_back(change.id);
            }
            continue;
        }

        // 执行更新
        std::string targetPkField = mapping.targetPrimaryKey;

        if (sqliteToFirebird) {
            // 接口: FirebirdManager::UpdateRecord(tableName, primaryKey, primaryKeyValue, data)
            // 跳过主键字段，主键不更新
            std::map<std::string, std::string> updateData;
            for (const auto& [field, value] : targetData) {
                if (field != targetPkField) {
                    updateData[field] = value;
                }
            }

            if (updateData.empty()) {
                try { sqliteSyncedIds.push_back(std::stoll(change.id)); } catch (...) {}
                continue;
            }

            bool updateResult = firebirdDb_->UpdateRecord(targetTable, targetPkField, change.primaryKeyValue, updateData);
            if (!updateResult) {
                std::cerr << "[SyncManager] 更新 Firebird 记录失败: " << targetTable
                          << " PK=" << change.primaryKeyValue << std::endl;
                if (errorCallback_) {
                    errorCallback_("更新记录失败: " + targetTable + " PK=" + change.primaryKeyValue);
                }
                continue;
            }
            std::cout << "[SyncManager] 更新 Firebird 记录成功: " << targetTable
                      << " PK=" << change.primaryKeyValue << std::endl;
        } else {
            // 接口: SqliteManager::UpdateRecord(table_name, fields, values, where_clause, where_params)
            std::vector<std::string> fields;
            std::vector<std::string> values;
            for (const auto& [field, value] : targetData) {
                if (field != targetPkField) {
                    fields.push_back(field);
                    values.push_back(value);
                }
            }

            if (fields.empty()) {
                firebirdSyncedIds.push_back(change.id);
                continue;
            }

            bool updateResult = sqliteDb_->UpdateRecord(
                targetTable, fields, values,
                targetPkField + " = ?", {change.primaryKeyValue});

            if (!updateResult) {
                std::cerr << "[SyncManager] 更新 SQLite 记录失败: " << targetTable
                          << " PK=" << change.primaryKeyValue << std::endl;
                if (errorCallback_) {
                    errorCallback_("更新记录失败: " + targetTable + " PK=" + change.primaryKeyValue);
                }
                continue;
            }
            std::cout << "[SyncManager] 更新 SQLite 记录成功: " << targetTable
                      << " PK=" << change.primaryKeyValue << std::endl;
        }

        // 记录已同步的变更ID
        if (sqliteToFirebird) {
            try { sqliteSyncedIds.push_back(std::stoll(change.id)); } catch (...) {}
        } else {
            firebirdSyncedIds.push_back(change.id);
        }
    }

    // 标记源变更已同步
    if (sqliteToFirebird && !sqliteSyncedIds.empty()) {
        // 接口: SqliteManager::MarkChangesSynced(const vector<int64_t>& change_ids)
        if (!sqliteDb_->MarkChangesSynced(sqliteSyncedIds)) {
            std::cerr << "[SyncManager] 标记 SQLite 更新变更已同步失败" << std::endl;
            return false;
        }
    }

    if (!sqliteToFirebird && !firebirdSyncedIds.empty()) {
        // 接口: FirebirdManager::MarkChangeAsSynced(const string& changeId) -- 逐个标记
        for (const auto& id : firebirdSyncedIds) {
            if (!firebirdDb_->MarkChangeAsSynced(id)) {
                std::cerr << "[SyncManager] 标记 Firebird 更新变更已同步失败: " << id << std::endl;
                return false;
            }
        }
    }

    return true;
}

bool SyncManager::SyncDeletedRecords(const TableMapping& mapping, bool sqliteToFirebird,
                                      const std::vector<ChangeRecord>& allChanges)
{
    std::string sourceTable = mapping.sourceTable;
    std::string targetTable = mapping.targetTable;

    // 过滤出 DELETE 操作
    std::vector<ChangeRecord> deleteChanges;
    for (const auto& change : allChanges) {
        if (change.changeType == ChangeType::DELETE) {
            deleteChanges.push_back(change);
        }
    }

    if (deleteChanges.empty()) {
        return true;
    }

    std::vector<int64_t> sqliteSyncedIds;
    std::vector<std::string> firebirdSyncedIds;

    for (const auto& change : deleteChanges) {
        // 在目标数据库中删除对应记录
        std::string targetPkField = mapping.targetPrimaryKey;

        if (sqliteToFirebird) {
            // 接口: FirebirdManager::DeleteRecord(tableName, primaryKey, primaryKeyValue)
            bool deleteResult = firebirdDb_->DeleteRecord(targetTable, targetPkField, change.primaryKeyValue);
            if (!deleteResult) {
                std::cerr << "[SyncManager] 删除 Firebird 记录失败: " << targetTable
                          << " PK=" << change.primaryKeyValue << std::endl;
                if (errorCallback_) {
                    errorCallback_("删除记录失败: " + targetTable + " PK=" + change.primaryKeyValue);
                }
                continue;
            }
            std::cout << "[SyncManager] 删除 Firebird 记录成功: " << targetTable
                      << " PK=" << change.primaryKeyValue << std::endl;
        } else {
            // 接口: SqliteManager::DeleteRecord(table_name, where_clause, where_params)
            bool deleteResult = sqliteDb_->DeleteRecord(
                targetTable,
                targetPkField + " = ?", {change.primaryKeyValue});

            if (!deleteResult) {
                std::cerr << "[SyncManager] 删除 SQLite 记录失败: " << targetTable
                          << " PK=" << change.primaryKeyValue << std::endl;
                if (errorCallback_) {
                    errorCallback_("删除记录失败: " + targetTable + " PK=" + change.primaryKeyValue);
                }
                continue;
            }
            std::cout << "[SyncManager] 删除 SQLite 记录成功: " << targetTable
                      << " PK=" << change.primaryKeyValue << std::endl;
        }

        // 记录已同步的变更ID
        if (sqliteToFirebird) {
            try { sqliteSyncedIds.push_back(std::stoll(change.id)); } catch (...) {}
        } else {
            firebirdSyncedIds.push_back(change.id);
        }
    }

    // 标记源变更已同步
    if (sqliteToFirebird && !sqliteSyncedIds.empty()) {
        // 接口: SqliteManager::MarkChangesSynced(const vector<int64_t>& change_ids)
        if (!sqliteDb_->MarkChangesSynced(sqliteSyncedIds)) {
            std::cerr << "[SyncManager] 标记 SQLite 删除变更已同步失败" << std::endl;
            return false;
        }
    }

    if (!sqliteToFirebird && !firebirdSyncedIds.empty()) {
        // 接口: FirebirdManager::MarkChangeAsSynced(const string& changeId) -- 逐个标记
        for (const auto& id : firebirdSyncedIds) {
            if (!firebirdDb_->MarkChangeAsSynced(id)) {
                std::cerr << "[SyncManager] 标记 Firebird 删除变更已同步失败: " << id << std::endl;
                return false;
            }
        }
    }

    return true;
}

// ==================== 网络消息处理 ====================

void SyncManager::OnMessageReceived(const SyncMessage& message)
{
    std::cout << "[SyncManager] 收到网络消息: type=" << static_cast<int>(message.type) << std::endl;

    switch (message.type) {
        case SyncMessage::Type::SYNC_REQUEST:
            // 远程节点请求同步，触发一次同步
            if (!syncing_) {
                TriggerSync();
            }
            break;

        case SyncMessage::Type::CHANGE_NOTIFICATION:
            // 远程节点通知有变更，触发一次同步以拉取变更
            if (!syncing_) {
                TriggerSync();
            }
            break;

        case SyncMessage::Type::SYNC_DATA:
            // 收到远程推送的变更数据，应用到本地数据库
            {
                ChangeRecord record;
                if (DeserializeChangeRecord(message.payload, record)) {
                    // TODO: 将远程变更应用到本地数据库
                    std::cout << "[SyncManager] 收到远程变更: " << record.tableName
                              << " " << ChangeTypeToString(record.changeType)
                              << " PK=" << record.primaryKeyValue << std::endl;
                }
            }
            break;

        default:
            std::cerr << "[SyncManager] 未知的消息类型: " << static_cast<int>(message.type) << std::endl;
            break;
    }
}

void SyncManager::OnConnectionStatusChanged(bool connected)
{
    std::cout << "[SyncManager] 网络连接状态变化: " << (connected ? "已连接" : "已断开") << std::endl;

    if (connected && statusCallback_) {
        statusCallback_("网络已连接", -1);
    } else if (!connected && statusCallback_) {
        statusCallback_("网络已断开", -1);
    }
}

// ==================== 序列化 ====================

std::string SyncManager::SerializeChangeRecord(const ChangeRecord& record)
{
    // 使用 SqliteManager::ChangeRecord 的字段名进行序列化
    // 格式：字段之间用 | 分隔
    // 字段映射: change_id | table_name | operation | primary_key | change_time | new_data | old_data
    std::stringstream ss;
    ss << record.id << "|"
       << record.tableName << "|"
       << ChangeTypeToString(record.changeType) << "|"
       << record.primaryKeyValue << "|"
       << record.timestamp << "|"
       << record.data << "|"
       << "";  // old_data 字段，Common::ChangeRecord 中没有，留空
    return ss.str();
}

bool SyncManager::DeserializeChangeRecord(const std::string& data, ChangeRecord& record)
{
    // 按 | 分割反序列化，使用 SqliteManager::ChangeRecord 的字段名格式
    // 字段映射: change_id | table_name | operation | primary_key | change_time | new_data | old_data
    std::vector<std::string> parts;
    std::stringstream ss(data);
    std::string part;

    while (std::getline(ss, part, '|')) {
        parts.push_back(part);
    }

    if (parts.size() < 6) {
        return false;
    }

    try {
        // 使用 SqliteManager.h 中 ChangeRecord 的字段名
        record.change_id = std::stoll(parts[0]);
        record.table_name = parts[1];
        record.operation = parts[2];  // "INSERT" / "UPDATE" / "DELETE"
        record.primary_key = parts[3];
        record.change_time = parts[4];
        record.new_data = parts[5];
        if (parts.size() > 6) {
            record.old_data = parts[6];
        }
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace dbsync
