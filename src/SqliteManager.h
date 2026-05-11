/**
 * @file SqliteManager.h
 * @brief SQLite 数据库管理器头文件
 * @details 支持加密 SQLite 数据库的连接、查询、变更追踪等功能
 *          使用 sqlite3 C API，支持 PRAGMA key 加密和 WAL 模式
 */

#ifndef DBSYNC_SQLITE_MANAGER_H
#define DBSYNC_SQLITE_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

// MinGW 需要显式包含 mutex
#ifdef _WIN32
#include <windows.h>
#endif
#include <mutex>

// 前向声明 sqlite3 结构体，避免头文件依赖
struct sqlite3;
struct sqlite3_stmt;

namespace dbsync {

/**
 * @brief 变更记录结构体
 * @details 描述一条数据库变更记录
 */
struct ChangeRecord {
    int64_t change_id;          ///< 变更日志ID
    std::string table_name;     ///< 表名
    std::string operation;      ///< 操作类型: INSERT / UPDATE / DELETE
    std::string primary_key;    ///< 主键值
    std::string change_time;    ///< 变更时间
    std::string old_data;       ///< 变更前的数据（JSON格式，仅UPDATE/DELETE）
    std::string new_data;       ///< 变更后的数据（JSON格式，仅INSERT/UPDATE）
};

/**
 * @brief 列信息结构体
 */
struct ColumnInfo {
    std::string name;           ///< 列名
    std::string type;           ///< 数据类型
    bool not_null;              ///< 是否非空
    std::string default_value;  ///< 默认值
    bool is_primary_key;        ///< 是否为主键
};

/**
 * @class SqliteManager
 * @brief SQLite 数据库管理器
 * @details 提供对加密 SQLite 数据库的完整操作接口，包括：
 *          - 连接管理（支持加密密钥和WAL模式）
 *          - 数据查询与执行
 *          - 表结构查询
 *          - 变更追踪（基于触发器和日志表）
 *          - 线程安全（内部使用mutex）
 */
class SqliteManager {
public:
    /**
     * @brief 构造函数
     */
    SqliteManager();

    /**
     * @brief 析构函数，自动关闭数据库连接
     */
    ~SqliteManager();

    // 禁用拷贝构造和赋值
    SqliteManager(const SqliteManager&) = delete;
    SqliteManager& operator=(const SqliteManager&) = delete;

    // ==================== 连接管理 ====================

    /**
     * @brief 连接 SQLite 数据库
     * @param db_path 数据库文件路径
     * @param key 加密密钥（空字符串表示不加密）
     * @return 成功返回 true，失败返回 false
     * @note 连接成功后会自动设置 WAL 模式
     */
    bool Connect(const std::string& db_path, const std::string& key = "");

    /**
     * @brief 断开数据库连接
     */
    void Disconnect();

    /**
     * @brief 检查数据库是否已连接
     * @return 已连接返回 true
     */
    bool IsConnected() const;

    // ==================== 数据查询与执行 ====================

    /**
     * @brief 执行 SQL 查询
     * @param sql SQL 查询语句
     * @return 查询结果，每行是一个 map<string,string>，键为列名，值为字符串形式的值
     */
    std::vector<std::map<std::string, std::string>> Query(const std::string& sql);

    /**
     * @brief 执行 SQL 语句（INSERT/UPDATE/DELETE 等）
     * @param sql SQL 语句
     * @param[out] last_insert_id 返回最后插入的行ID（可为nullptr）
     * @param[out] affected_rows 返回受影响的行数（可为nullptr）
     * @return 成功返回 true
     */
    bool Execute(const std::string& sql, int64_t* last_insert_id = nullptr, int* affected_rows = nullptr);

    /**
     * @brief 执行带参数的 SQL 语句
     * @param sql SQL 语句，使用 ? 作为参数占位符
     * @param params 参数值列表
     * @param[out] last_insert_id 返回最后插入的行ID（可为nullptr）
     * @param[out] affected_rows 返回受影响的行数（可为nullptr）
     * @return 成功返回 true
     */
    bool ExecuteParam(const std::string& sql,
                      const std::vector<std::string>& params,
                      int64_t* last_insert_id = nullptr,
                      int* affected_rows = nullptr);

    // ==================== 表结构查询 ====================

    /**
     * @brief 获取所有用户表名
     * @return 表名列表（排除 sqlite_ 开头的系统表）
     */
    std::vector<std::string> GetTables();

    /**
     * @brief 获取指定表的所有列名
     * @param table_name 表名
     * @return 列名列表
     */
    std::vector<std::string> GetTableColumns(const std::string& table_name);

    /**
     * @brief 获取指定表的列详细信息
     * @param table_name 表名
     * @return 列信息列表
     */
    std::vector<ColumnInfo> GetTableColumnInfos(const std::string& table_name);

    /**
     * @brief 获取指定表的主键列名
     * @param table_name 表名
     * @return 主键列名，无主键时返回空字符串
     */
    std::string GetPrimaryKey(const std::string& table_name);

    /**
     * @brief 获取指定表的所有数据
     * @param table_name 表名
     * @return 查询结果，每行是一个 map
     */
    std::vector<std::map<std::string, std::string>> GetTableData(const std::string& table_name);

    /**
     * @brief 获取指定表中指定主键值的记录
     * @param table_name 表名
     * @param pk_column 主键列名
     * @param pk_value 主键值
     * @return 查询结果（单行或多行）
     */
    std::vector<std::map<std::string, std::string>> GetRecordByKey(
        const std::string& table_name,
        const std::string& pk_column,
        const std::string& pk_value);

    // ==================== 变更追踪 ====================

    /**
     * @brief 设置变更追踪
     * @details 创建 _sync_changes 日志表和对应的触发器
     *          对每个用户表创建 INSERT/UPDATE/DELETE 触发器
     * @return 成功返回 true
     */
    bool SetupChangeTracking();

    /**
     * @brief 为指定表设置变更追踪触发器
     * @param table_name 表名
     * @return 成功返回 true
     */
    bool SetupTableTrigger(const std::string& table_name);

    /**
     * @brief 获取待同步的变更记录
     * @param table_name 表名（空字符串表示获取所有表的变更）
     * @param since_id 起始变更ID（0表示获取全部）
     * @return 变更记录列表
     */
    std::vector<ChangeRecord> GetPendingChanges(const std::string& table_name = "", int64_t since_id = 0);

    /**
     * @brief 获取待同步的变更数量
     * @param table_name 表名（空字符串表示统计所有表）
     * @return 待同步的变更数量
     */
    int64_t GetPendingChangeCount(const std::string& table_name = "");

    /**
     * @brief 标记变更记录为已同步
     * @param change_ids 要标记的变更ID列表
     * @return 成功返回 true
     */
    bool MarkChangesSynced(const std::vector<int64_t>& change_ids);

    /**
     * @brief 清理已同步的变更记录
     * @param keep_days 保留最近几天的记录（0表示全部清理已同步记录）
     * @return 清理的记录数
     */
    int64_t CleanupSyncedChanges(int keep_days = 7);

    // ==================== 数据操作 ====================

    /**
     * @brief 插入一条记录
     * @param table_name 表名
     * @param fields 字段名列表
     * @param values 对应的值列表
     * @param[out] last_insert_id 返回插入的行ID（可为nullptr）
     * @return 成功返回 true
     */
    bool InsertRecord(const std::string& table_name,
                      const std::vector<std::string>& fields,
                      const std::vector<std::string>& values,
                      int64_t* last_insert_id = nullptr);

    /**
     * @brief 更新一条记录
     * @param table_name 表名
     * @param fields 要更新的字段名列表
     * @param values 对应的新值列表
     * @param where_clause WHERE 条件子句（不含 WHERE 关键字）
     * @param where_params WHERE 条件的参数值
     * @param[out] affected_rows 返回受影响的行数（可为nullptr）
     * @return 成功返回 true
     */
    bool UpdateRecord(const std::string& table_name,
                      const std::vector<std::string>& fields,
                      const std::vector<std::string>& values,
                      const std::string& where_clause,
                      const std::vector<std::string>& where_params = {},
                      int* affected_rows = nullptr);

    /**
     * @brief 删除记录
     * @param table_name 表名
     * @param where_clause WHERE 条件子句（不含 WHERE 关键字）
     * @param where_params WHERE 条件的参数值
     * @param[out] affected_rows 返回受影响的行数（可为nullptr）
     * @return 成功返回 true
     */
    bool DeleteRecord(const std::string& table_name,
                      const std::string& where_clause,
                      const std::vector<std::string>& where_params = {},
                      int* affected_rows = nullptr);

    // ==================== 事务管理 ====================

    /**
     * @brief 开始事务
     * @return 成功返回 true
     */
    bool BeginTransaction();

    /**
     * @brief 提交事务
     * @return 成功返回 true
     */
    bool CommitTransaction();

    /**
     * @brief 回滚事务
     * @return 成功返回 true
     */
    bool RollbackTransaction();

    /**
     * @brief 在事务中执行操作
     * @param func 事务内要执行的函数
     * @return 成功返回 true，失败时自动回滚
     */
    bool InTransaction(std::function<bool()> func);

    // ==================== 工具方法 ====================

    /**
     * @brief 获取最后的错误信息
     * @return 错误信息字符串
     */
    std::string GetLastError() const;

    /**
     * @brief 转义字符串用于 SQL（防止 SQL 注入）
     * @param value 原始字符串
     * @return 转义后的字符串
     */
    static std::string EscapeString(const std::string& value);

    /**
     * @brief 检查表是否存在
     * @param table_name 表名
     * @return 存在返回 true
     */
    bool TableExists(const std::string& table_name);

private:
    sqlite3* m_db;                      ///< SQLite 数据库句柄
    std::string m_dbPath;               ///< 数据库文件路径
    std::string m_key;                  ///< 加密密钥
    mutable std::mutex m_mutex;         ///< 线程安全互斥锁
    std::string m_lastError;            ///< 最后的错误信息

    /**
     * @brief 设置最后的错误信息
     * @param error 错误信息
     */
    void SetLastError(const std::string& error);

    /**
     * @brief 绑定参数到预编译语句
     * @param stmt 预编译语句
     * @param params 参数值列表
     * @return 成功返回 true
     */
    bool BindParams(sqlite3_stmt* stmt, const std::vector<std::string>& params);

    /**
     * @brief 内部查询方法（不加锁，由调用方保证线程安全）
     * @param sql SQL 语句
     * @return 查询结果
     */
    std::vector<std::map<std::string, std::string>> QueryInternal(const std::string& sql);

    /**
     * @brief 内部执行方法（不加锁，由调用方保证线程安全）
     * @param sql SQL 语句
     * @param[out] last_insert_id 最后插入行ID
     * @param[out] affected_rows 受影响行数
     * @return 成功返回 true
     */
    bool ExecuteInternal(const std::string& sql,
                         int64_t* last_insert_id = nullptr,
                         int* affected_rows = nullptr);
};

} // namespace dbsync

#endif // DBSYNC_SQLITE_MANAGER_H
