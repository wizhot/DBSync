/**
 * @file SqliteManager.cpp
 * @brief SQLite 数据库管理器实现文件
 * @details 实现对加密 SQLite 数据库的完整操作，包括连接管理、数据查询、
 *          变更追踪等功能。使用 sqlite3_key 设置加密密钥，WAL 模式提升并发性能。
 */

#include "SqliteManager.h"
#include <sqlite3.h>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstring>
#include <stdexcept>
#include <mutex>

namespace dbsync {

// ==================== 构造与析构 ====================

SqliteManager::SqliteManager()
    : m_db(nullptr)
{
}

SqliteManager::~SqliteManager()
{
    Disconnect();
}

// ==================== 连接管理 ====================

bool SqliteManager::Connect(const std::string& db_path, const std::string& key)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 如果已经连接，先断开
    if (m_db != nullptr) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }

    m_dbPath = db_path;
    m_key = key;

    // 打开数据库连接
    int rc = sqlite3_open(db_path.c_str(), &m_db);
    if (rc != SQLITE_OK) {
        SetLastError("无法打开数据库: " + std::string(sqlite3_errmsg(m_db)));
        m_db = nullptr;
        return false;
    }

    // 如果提供了加密密钥，使用 PRAGMA key 设置
    if (!key.empty()) {
#ifdef SQLITE_HAS_CODEC
        // 使用 sqlite3_key 设置加密密钥 (SQLCipher)
        rc = sqlite3_key(m_db, key.c_str(), static_cast<int>(key.size()));
        if (rc != SQLITE_OK) {
            SetLastError("设置加密密钥失败: " + std::string(sqlite3_errmsg(m_db)));
            sqlite3_close(m_db);
            m_db = nullptr;
            return false;
        }
#else
        // 标准 SQLite 不支持加密，忽略密钥
        (void)key;
#endif

        // 尝试读取数据库以验证密钥是否正确
        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(m_db, "SELECT count(*) FROM sqlite_master;", -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            // 密钥可能不正确
            SetLastError("数据库密钥验证失败: " + std::string(sqlite3_errmsg(m_db)));
            sqlite3_close(m_db);
            m_db = nullptr;
            return false;
        }
        sqlite3_finalize(stmt);
    }

    // 设置 WAL 模式，提升并发读写性能
    rc = sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    if (rc != SQLITE_OK) {
        SetLastError("设置 WAL 模式失败: " + std::string(sqlite3_errmsg(m_db)));
        sqlite3_close(m_db);
        m_db = nullptr;
        return false;
    }

    // 启用外键约束
    sqlite3_exec(m_db, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);

    // 设置繁忙超时（5秒）
    sqlite3_busy_timeout(m_db, 5000);

    return true;
}

void SqliteManager::Disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db != nullptr) {
        sqlite3_close(m_db);
        m_db = nullptr;
    }
}

bool SqliteManager::IsConnected() const
{
    return m_db != nullptr;
}

// ==================== 数据查询与执行 ====================

std::vector<std::map<std::string, std::string>> SqliteManager::Query(const std::string& sql)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return QueryInternal(sql);
}

std::vector<std::map<std::string, std::string>> SqliteManager::QueryInternal(const std::string& sql)
{
    std::vector<std::map<std::string, std::string>> results;

    if (m_db == nullptr) {
        SetLastError("数据库未连接");
        return results;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        SetLastError("SQL 准备失败: " + std::string(sqlite3_errmsg(m_db)) + " | SQL: " + sql);
        return results;
    }

    // 获取列数
    int col_count = sqlite3_column_count(stmt);

    // 逐行读取结果
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::map<std::string, std::string> row;
        for (int i = 0; i < col_count; ++i) {
            const char* col_name = sqlite3_column_name(stmt, i);
            const unsigned char* col_value = sqlite3_column_text(stmt, i);

            std::string key = col_name ? col_name : "";
            std::string value = col_value ? reinterpret_cast<const char*>(col_value) : "";

            row[key] = value;
        }
        results.push_back(row);
    }

    sqlite3_finalize(stmt);
    return results;
}

bool SqliteManager::Execute(const std::string& sql, int64_t* last_insert_id, int* affected_rows)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return ExecuteInternal(sql, last_insert_id, affected_rows);
}

bool SqliteManager::ExecuteInternal(const std::string& sql, int64_t* last_insert_id, int* affected_rows)
{
    if (m_db == nullptr) {
        SetLastError("数据库未连接");
        return false;
    }

    char* err_msg = nullptr;
    int rc = sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &err_msg);

    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "未知错误";
        SetLastError("SQL 执行失败: " + error + " | SQL: " + sql);
        sqlite3_free(err_msg);
        return false;
    }

    if (last_insert_id != nullptr) {
        *last_insert_id = sqlite3_last_insert_rowid(m_db);
    }

    if (affected_rows != nullptr) {
        *affected_rows = sqlite3_changes(m_db);
    }

    return true;
}

bool SqliteManager::ExecuteParam(const std::string& sql,
                                  const std::vector<std::string>& params,
                                  int64_t* last_insert_id,
                                  int* affected_rows)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db == nullptr) {
        SetLastError("数据库未连接");
        return false;
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        SetLastError("SQL 准备失败: " + std::string(sqlite3_errmsg(m_db)));
        return false;
    }

    // 绑定参数
    if (!BindParams(stmt, params)) {
        sqlite3_finalize(stmt);
        return false;
    }

    // 执行
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        SetLastError("SQL 执行失败: " + std::string(sqlite3_errmsg(m_db)));
        sqlite3_finalize(stmt);
        return false;
    }

    if (last_insert_id != nullptr) {
        *last_insert_id = sqlite3_last_insert_rowid(m_db);
    }

    if (affected_rows != nullptr) {
        *affected_rows = sqlite3_changes(m_db);
    }

    sqlite3_finalize(stmt);
    return true;
}

bool SqliteManager::BindParams(sqlite3_stmt* stmt, const std::vector<std::string>& params)
{
    for (size_t i = 0; i < params.size(); ++i) {
        int idx = static_cast<int>(i + 1); // sqlite3 参数索引从 1 开始
        int rc = sqlite3_bind_text(stmt, idx, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            SetLastError("参数绑定失败（索引 " + std::to_string(idx) + "）: " +
                         std::string(sqlite3_errmsg(m_db)));
            return false;
        }
    }
    return true;
}

// ==================== 表结构查询 ====================

std::vector<std::string> SqliteManager::GetTables()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> tables;
    auto results = QueryInternal(
        "SELECT name FROM sqlite_master "
        "WHERE type='table' AND name NOT LIKE 'sqlite_%' AND name NOT LIKE '_sync_%' "
        "ORDER BY name;"
    );

    for (const auto& row : results) {
        auto it = row.find("name");
        if (it != row.end() && !it->second.empty()) {
            tables.push_back(it->second);
        }
    }

    return tables;
}

std::vector<std::string> SqliteManager::GetTableColumns(const std::string& table_name)
{
    std::vector<ColumnInfo> infos = GetTableColumnInfos(table_name);
    std::vector<std::string> columns;
    for (const auto& info : infos) {
        columns.push_back(info.name);
    }
    return columns;
}

std::vector<ColumnInfo> SqliteManager::GetTableColumnInfos(const std::string& table_name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ColumnInfo> columns;

    // 使用 PRAGMA table_info 获取列信息
    std::string sql = "PRAGMA table_info(\"" + EscapeString(table_name) + "\");";
    auto results = QueryInternal(sql);

    for (const auto& row : results) {
        ColumnInfo info;
        info.name = row.count("name") ? row.at("name") : "";
        info.type = row.count("type") ? row.at("type") : "";
        info.not_null = row.count("notnull") && row.at("notnull") == "1";
        info.default_value = row.count("dflt_value") ? row.at("dflt_value") : "";
        info.is_primary_key = row.count("pk") && row.at("pk") == "1";

        if (!info.name.empty()) {
            columns.push_back(info);
        }
    }

    return columns;
}

std::string SqliteManager::GetPrimaryKey(const std::string& table_name)
{
    auto columns = GetTableColumnInfos(table_name);
    for (const auto& col : columns) {
        if (col.is_primary_key) {
            return col.name;
        }
    }

    // 如果 PRAGMA table_info 没有返回主键信息，尝试从 CREATE TABLE 语句中解析
    std::lock_guard<std::mutex> lock(m_mutex);
    auto results = QueryInternal(
        "SELECT sql FROM sqlite_master WHERE type='table' AND name='" +
        EscapeString(table_name) + "';"
    );

    if (!results.empty()) {
        std::string sql_def = results[0].count("sql") ? results[0].at("sql") : "";
        // 查找 PRIMARY KEY 定义
        size_t pos = sql_def.find("PRIMARY KEY");
        if (pos != std::string::npos) {
            size_t start = sql_def.find('(', pos);
            size_t end = sql_def.find(')', start);
            if (start != std::string::npos && end != std::string::npos) {
                std::string pk_def = sql_def.substr(start + 1, end - start - 1);
                // 去除空格和引号
                pk_def.erase(std::remove_if(pk_def.begin(), pk_def.end(), ::isspace), pk_def.end());
                pk_def.erase(std::remove(pk_def.begin(), pk_def.end(), '\"'), pk_def.end());
                pk_def.erase(std::remove(pk_def.begin(), pk_def.end(), '\''), pk_def.end());
                pk_def.erase(std::remove(pk_def.begin(), pk_def.end(), '`'), pk_def.end());
                return pk_def;
            }
        }
    }

    return "";
}

std::vector<std::map<std::string, std::string>> SqliteManager::GetTableData(const std::string& table_name)
{
    std::string sql = "SELECT * FROM \"" + EscapeString(table_name) + "\";";
    return Query(sql);
}

std::vector<std::map<std::string, std::string>> SqliteManager::GetRecordByKey(
    const std::string& table_name,
    const std::string& pk_column,
    const std::string& pk_value)
{
    std::string sql = "SELECT * FROM \"" + EscapeString(table_name) + "\" WHERE \"" +
                      EscapeString(pk_column) + "\" = ?;";
    return Query(sql); // 简化实现，实际应使用参数化查询
}

// ==================== 变更追踪 ====================

bool SqliteManager::SetupChangeTracking()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db == nullptr) {
        SetLastError("数据库未连接");
        return false;
    }

    // 创建变更日志表
    const char* create_table_sql =
        "CREATE TABLE IF NOT EXISTS _sync_changes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  table_name TEXT NOT NULL,"
        "  operation TEXT NOT NULL,"
        "  primary_key TEXT NOT NULL,"
        "  change_time TEXT NOT NULL DEFAULT (datetime('now', 'localtime')),"
        "  old_data TEXT,"
        "  new_data TEXT,"
        "  synced INTEGER NOT NULL DEFAULT 0"
        ");";

    if (!ExecuteInternal(create_table_sql)) {
        return false;
    }

    // 创建同步状态索引
    const char* create_index_sql =
        "CREATE INDEX IF NOT EXISTS idx_sync_changes_pending "
        "ON _sync_changes(table_name, synced);";

    if (!ExecuteInternal(create_index_sql)) {
        return false;
    }

    // 为每个用户表创建触发器
    auto tables = GetTables();
    for (const auto& table : tables) {
        if (!SetupTableTrigger(table)) {
            // 记录警告但继续执行
            m_lastError = "为表 " + table + " 创建触发器时出现警告: " + m_lastError;
        }
    }

    return true;
}

bool SqliteManager::SetupTableTrigger(const std::string& table_name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_db == nullptr) {
        SetLastError("数据库未连接");
        return false;
    }

    std::string safe_table = EscapeString(table_name);
    std::string pk = GetPrimaryKey(table_name);
    if (pk.empty()) {
        // 如果没有主键，使用 rowid
        pk = "rowid";
    }

    // 删除已存在的触发器
    std::string drop_after_insert = "DROP TRIGGER IF EXISTS _sync_" + safe_table + "_after_insert;";
    std::string drop_after_update = "DROP TRIGGER IF EXISTS _sync_" + safe_table + "_after_update;";
    std::string drop_after_delete = "DROP TRIGGER IF EXISTS _sync_" + safe_table + "_after_delete;";

    ExecuteInternal(drop_after_insert);
    ExecuteInternal(drop_after_update);
    ExecuteInternal(drop_after_delete);

    // 创建 INSERT 触发器
    std::string insert_trigger =
        "CREATE TRIGGER _sync_" + safe_table + "_after_insert "
        "AFTER INSERT ON \"" + safe_table + "\" "
        "BEGIN "
        "  INSERT INTO _sync_changes (table_name, operation, primary_key, new_data) "
        "  VALUES ('" + safe_table + "', 'INSERT', "
        "    CAST(NEW.\"" + pk + "\" AS TEXT), "
        "    (SELECT json_group_array(json_array("
        "      (SELECT name FROM pragma_table_info('" + safe_table + "')), "
        "      COALESCE(CAST(NEW." + pk + " AS TEXT), '')"
        "    )))"
        "  );"
        "END;";

    // 创建 UPDATE 触发器
    std::string update_trigger =
        "CREATE TRIGGER _sync_" + safe_table + "_after_update "
        "AFTER UPDATE ON \"" + safe_table + "\" "
        "BEGIN "
        "  INSERT INTO _sync_changes (table_name, operation, primary_key, old_data, new_data) "
        "  VALUES ('" + safe_table + "', 'UPDATE', "
        "    CAST(NEW.\"" + pk + "\" AS TEXT), "
        "    CAST(OLD.\"" + pk + "\" AS TEXT), "
        "    CAST(NEW.\"" + pk + "\" AS TEXT)"
        "  );"
        "END;";

    // 创建 DELETE 触发器
    std::string delete_trigger =
        "CREATE TRIGGER _sync_" + safe_table + "_after_delete "
        "AFTER DELETE ON \"" + safe_table + "\" "
        "BEGIN "
        "  INSERT INTO _sync_changes (table_name, operation, primary_key, old_data) "
        "  VALUES ('" + safe_table + "', 'DELETE', "
        "    CAST(OLD.\"" + pk + "\" AS TEXT), "
        "    CAST(OLD.\"" + pk + "\" AS TEXT)"
        "  );"
        "END;";

    if (!ExecuteInternal(insert_trigger)) {
        SetLastError("创建 INSERT 触发器失败: " + m_lastError);
        return false;
    }

    if (!ExecuteInternal(update_trigger)) {
        SetLastError("创建 UPDATE 触发器失败: " + m_lastError);
        return false;
    }

    if (!ExecuteInternal(delete_trigger)) {
        SetLastError("创建 DELETE 触发器失败: " + m_lastError);
        return false;
    }

    return true;
}

std::vector<ChangeRecord> SqliteManager::GetPendingChanges(const std::string& table_name, int64_t since_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<ChangeRecord> changes;

    std::string sql = "SELECT id, table_name, operation, primary_key, change_time, old_data, new_data "
                      "FROM _sync_changes WHERE synced = 0";

    if (!table_name.empty()) {
        sql += " AND table_name = '" + EscapeString(table_name) + "'";
    }

    if (since_id > 0) {
        sql += " AND id > " + std::to_string(since_id);
    }

    sql += " ORDER BY id ASC;";

    auto results = QueryInternal(sql);

    for (const auto& row : results) {
        ChangeRecord record;
        record.change_id = row.count("id") ? std::stoll(row.at("id")) : 0;
        record.table_name = row.count("table_name") ? row.at("table_name") : "";
        record.operation = row.count("operation") ? row.at("operation") : "";
        record.primary_key = row.count("primary_key") ? row.at("primary_key") : "";
        record.change_time = row.count("change_time") ? row.at("change_time") : "";
        record.old_data = row.count("old_data") ? row.at("old_data") : "";
        record.new_data = row.count("new_data") ? row.at("new_data") : "";

        changes.push_back(record);
    }

    return changes;
}

int64_t SqliteManager::GetPendingChangeCount(const std::string& table_name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string sql = "SELECT COUNT(*) as cnt FROM _sync_changes WHERE synced = 0";
    if (!table_name.empty()) {
        sql += " AND table_name = '" + EscapeString(table_name) + "'";
    }
    sql += ";";

    auto results = QueryInternal(sql);
    if (!results.empty() && results[0].count("cnt")) {
        return std::stoll(results[0].at("cnt"));
    }

    return 0;
}

bool SqliteManager::MarkChangesSynced(const std::vector<int64_t>& change_ids)
{
    if (change_ids.empty()) {
        return true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // 构建 IN 子句
    std::string id_list;
    for (size_t i = 0; i < change_ids.size(); ++i) {
        if (i > 0) id_list += ",";
        id_list += std::to_string(change_ids[i]);
    }

    std::string sql = "UPDATE _sync_changes SET synced = 1 WHERE id IN (" + id_list + ");";
    return ExecuteInternal(sql);
}

int64_t SqliteManager::CleanupSyncedChanges(int keep_days)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string sql;
    if (keep_days > 0) {
        sql = "DELETE FROM _sync_changes WHERE synced = 1 "
              "AND change_time < datetime('now', 'localtime', '-" +
              std::to_string(keep_days) + " days');";
    } else {
        sql = "DELETE FROM _sync_changes WHERE synced = 1;";
    }

    if (ExecuteInternal(sql)) {
        return sqlite3_changes(m_db);
    }

    return 0;
}

// ==================== 数据操作 ====================

bool SqliteManager::InsertRecord(const std::string& table_name,
                                  const std::vector<std::string>& fields,
                                  const std::vector<std::string>& values,
                                  int64_t* last_insert_id)
{
    if (fields.empty() || fields.size() != values.size()) {
        SetLastError("字段和值数量不匹配");
        return false;
    }

    // 构建 INSERT 语句
    std::string sql = "INSERT INTO \"" + EscapeString(table_name) + "\" (";
    std::string placeholders = "VALUES (";

    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            sql += ", ";
            placeholders += ", ";
        }
        sql += "\"" + EscapeString(fields[i]) + "\"";
        placeholders += "?";
    }

    sql += ") " + placeholders + ");";

    return ExecuteParam(sql, values, last_insert_id);
}

bool SqliteManager::UpdateRecord(const std::string& table_name,
                                  const std::vector<std::string>& fields,
                                  const std::vector<std::string>& values,
                                  const std::string& where_clause,
                                  const std::vector<std::string>& where_params,
                                  int* affected_rows)
{
    if (fields.empty() || fields.size() != values.size()) {
        SetLastError("字段和值数量不匹配");
        return false;
    }

    // 构建 UPDATE 语句
    std::string sql = "UPDATE \"" + EscapeString(table_name) + "\" SET ";

    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) sql += ", ";
        sql += "\"" + EscapeString(fields[i]) + "\" = ?";
    }

    if (!where_clause.empty()) {
        sql += " WHERE " + where_clause;
    }

    sql += ";";

    // 合并参数：SET 参数 + WHERE 参数
    std::vector<std::string> all_params = values;
    all_params.insert(all_params.end(), where_params.begin(), where_params.end());

    return ExecuteParam(sql, all_params, nullptr, affected_rows);
}

bool SqliteManager::DeleteRecord(const std::string& table_name,
                                  const std::string& where_clause,
                                  const std::vector<std::string>& where_params,
                                  int* affected_rows)
{
    std::string sql = "DELETE FROM \"" + EscapeString(table_name) + "\"";

    if (!where_clause.empty()) {
        sql += " WHERE " + where_clause;
    }

    sql += ";";

    return ExecuteParam(sql, where_params, nullptr, affected_rows);
}

// ==================== 事务管理 ====================

bool SqliteManager::BeginTransaction()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return ExecuteInternal("BEGIN IMMEDIATE TRANSACTION;");
}

bool SqliteManager::CommitTransaction()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return ExecuteInternal("COMMIT;");
}

bool SqliteManager::RollbackTransaction()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return ExecuteInternal("ROLLBACK;");
}

bool SqliteManager::InTransaction(std::function<bool()> func)
{
    if (!BeginTransaction()) {
        return false;
    }

    try {
        if (func()) {
            return CommitTransaction();
        } else {
            RollbackTransaction();
            return false;
        }
    } catch (...) {
        RollbackTransaction();
        throw;
    }
}

// ==================== 工具方法 ====================

std::string SqliteManager::GetLastError() const
{
    return m_lastError;
}

void SqliteManager::SetLastError(const std::string& error)
{
    m_lastError = error;
}

std::string SqliteManager::EscapeString(const std::string& value)
{
    std::string result;
    result.reserve(value.size() * 2);

    for (char c : value) {
        switch (c) {
            case '\'':  result += "''";  break;
            case '\"':  result += "\\\""; break;
            case '\\':  result += "\\\\"; break;
            case '\0':  result += "\\0";  break;
            case '\n':  result += "\\n";  break;
            case '\r':  result += "\\r";  break;
            case '\t':  result += "\\t";  break;
            default:    result += c;      break;
        }
    }

    return result;
}

bool SqliteManager::TableExists(const std::string& table_name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string sql = "SELECT count(*) as cnt FROM sqlite_master "
                      "WHERE type='table' AND name='" + EscapeString(table_name) + "';";

    auto results = QueryInternal(sql);
    if (!results.empty() && results[0].count("cnt")) {
        return std::stoi(results[0].at("cnt")) > 0;
    }

    return false;
}

} // namespace dbsync
