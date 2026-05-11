#include "ChangeTracker.h"
#include "Logger.h"
#include <sqlite3.h>
#include <sstream>
#include <chrono>

namespace dbsync {

ChangeTracker::ChangeTracker() : db_(nullptr), initialized_(false) {
}

ChangeTracker::~ChangeTracker() {
    Shutdown();
}

bool ChangeTracker::Initialize(const std::string& dbPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }
    
    dbPath_ = dbPath;
    
    try {
        // 打开或创建SQLite数据库
        int rc = sqlite3_open(dbPath.c_str(), &db_);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to open ChangeTracker database: " + std::string(sqlite3_errmsg(db_)));
            sqlite3_close(db_);
            db_ = nullptr;
            return false;
        }
        
        // 创建表结构
        if (!CreateSchema()) {
            LOG_ERROR("Failed to create database schema");
            sqlite3_close(db_);
            db_ = nullptr;
            return false;
        }
        
        initialized_ = true;
        LOG_INFO("ChangeTracker initialized with database: " + dbPath);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize ChangeTracker: " + std::string(e.what()));
        return false;
    }
}

void ChangeTracker::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    
    initialized_ = false;
    LOG_INFO("ChangeTracker shutdown");
}

bool ChangeTracker::ExecuteSQL(const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        if (errMsg) {
            LOG_ERROR("SQL error: " + std::string(errMsg) + " | SQL: " + sql);
            sqlite3_free(errMsg);
        }
        return false;
    }
    return true;
}

bool ChangeTracker::QuerySQL(const std::string& sql, std::vector<std::vector<std::string>>& results) {
    results.clear();
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        LOG_ERROR("Failed to prepare SQL: " + std::string(sqlite3_errmsg(db_)));
        return false;
    }
    
    int cols = sqlite3_column_count(stmt);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::vector<std::string> row;
        for (int i = 0; i < cols; ++i) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
            row.push_back(val ? val : "");
        }
        results.push_back(row);
    }
    
    sqlite3_finalize(stmt);
    return true;
}

bool ChangeTracker::CreateSchema() {
    try {
        // 创建本地变更表
        ExecuteSQL(
            "CREATE TABLE IF NOT EXISTS local_changes ("
            "  change_id TEXT PRIMARY KEY,"
            "  table_name TEXT NOT NULL,"
            "  change_type TEXT NOT NULL,"
            "  primary_key TEXT,"
            "  primary_key_value TEXT,"
            "  data TEXT,"
            "  timestamp TEXT NOT NULL,"
            "  synced INTEGER DEFAULT 0"
            ")"
        );
        
        // 创建远程变更表
        ExecuteSQL(
            "CREATE TABLE IF NOT EXISTS remote_changes ("
            "  change_id TEXT PRIMARY KEY,"
            "  table_name TEXT NOT NULL,"
            "  change_type TEXT NOT NULL,"
            "  primary_key TEXT,"
            "  primary_key_value TEXT,"
            "  data TEXT,"
            "  timestamp TEXT NOT NULL,"
            "  source_node TEXT,"
            "  synced INTEGER DEFAULT 0"
            ")"
        );
        
        // 创建同步点表
        ExecuteSQL(
            "CREATE TABLE IF NOT EXISTS sync_points ("
            "  node_id TEXT PRIMARY KEY,"
            "  timestamp TEXT NOT NULL"
            ")"
        );
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to create schema: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::RecordLocalChange(const ChangeRecord& change) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        std::string changeType = (change.changeType == ChangeType::INSERT) ? "INSERT" :
                                (change.changeType == ChangeType::UPDATE) ? "UPDATE" : "DELETE";
        
        // 使用参数化查询防止 SQL 注入
        sqlite3_stmt* stmt = nullptr;
        std::string sql = "INSERT OR REPLACE INTO local_changes "
                          "(change_id, table_name, change_type, primary_key, primary_key_value, data, timestamp, synced) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, 0)";
        
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to prepare insert local change: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, change.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, change.tableName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, changeType.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, change.primaryKey.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, change.primaryKeyValue.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, change.data.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, change.timestamp.c_str(), -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc != SQLITE_DONE) {
            LOG_ERROR("Failed to record local change: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to record local change: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::RecordRemoteChange(const ChangeRecord& change) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        std::string changeType = (change.changeType == ChangeType::INSERT) ? "INSERT" :
                                (change.changeType == ChangeType::UPDATE) ? "UPDATE" : "DELETE";
        
        sqlite3_stmt* stmt = nullptr;
        std::string sql = "INSERT OR REPLACE INTO remote_changes "
                          "(change_id, table_name, change_type, primary_key, primary_key_value, data, timestamp, source_node, synced) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0)";
        
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to prepare insert remote change: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, change.id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, change.tableName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, changeType.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, change.primaryKey.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, change.primaryKeyValue.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, change.data.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, change.timestamp.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, change.sourceNode.c_str(), -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc != SQLITE_DONE) {
            LOG_ERROR("Failed to record remote change: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to record remote change: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::GetUnsyncedLocalChanges(std::vector<ChangeRecord>& changes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    changes.clear();
    
    if (!initialized_) {
        return false;
    }
    
    try {
        std::vector<std::vector<std::string>> results;
        if (!QuerySQL("SELECT change_id, table_name, change_type, primary_key, primary_key_value, data, timestamp "
                      "FROM local_changes WHERE synced = 0 ORDER BY timestamp", results)) {
            return false;
        }
        
        for (const auto& row : results) {
            if (row.size() < 7) continue;
            ChangeRecord record;
            record.id = row[0];
            record.tableName = row[1];
            
            if (row[2] == "INSERT") record.changeType = ChangeType::INSERT;
            else if (row[2] == "UPDATE") record.changeType = ChangeType::UPDATE;
            else record.changeType = ChangeType::DELETE;
            
            record.primaryKey = row[3];
            record.primaryKeyValue = row[4];
            record.data = row[5];
            record.timestamp = row[6];
            record.status = SyncStatus::PENDING;
            
            changes.push_back(record);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to get unsynced local changes: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::GetUnsyncedRemoteChanges(std::vector<ChangeRecord>& changes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    changes.clear();
    
    if (!initialized_) {
        return false;
    }
    
    try {
        std::vector<std::vector<std::string>> results;
        if (!QuerySQL("SELECT change_id, table_name, change_type, primary_key, primary_key_value, data, timestamp, source_node "
                      "FROM remote_changes WHERE synced = 0 ORDER BY timestamp", results)) {
            return false;
        }
        
        for (const auto& row : results) {
            if (row.size() < 8) continue;
            ChangeRecord record;
            record.id = row[0];
            record.tableName = row[1];
            
            if (row[2] == "INSERT") record.changeType = ChangeType::INSERT;
            else if (row[2] == "UPDATE") record.changeType = ChangeType::UPDATE;
            else record.changeType = ChangeType::DELETE;
            
            record.primaryKey = row[3];
            record.primaryKeyValue = row[4];
            record.data = row[5];
            record.timestamp = row[6];
            record.sourceNode = row[7];
            record.status = SyncStatus::PENDING;
            
            changes.push_back(record);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to get unsynced remote changes: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::GetAllChanges(std::vector<ChangeRecord>& changes) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    changes.clear();
    
    if (!initialized_) {
        return false;
    }
    
    try {
        // 获取本地变更
        std::vector<std::vector<std::string>> localResults;
        if (QuerySQL("SELECT change_id, table_name, change_type, primary_key, primary_key_value, data, timestamp, synced "
                      "FROM local_changes ORDER BY timestamp", localResults)) {
            for (const auto& row : localResults) {
                if (row.size() < 8) continue;
                ChangeRecord record;
                record.id = row[0];
                record.tableName = row[1];
                
                if (row[2] == "INSERT") record.changeType = ChangeType::INSERT;
                else if (row[2] == "UPDATE") record.changeType = ChangeType::UPDATE;
                else record.changeType = ChangeType::DELETE;
                
                record.primaryKey = row[3];
                record.primaryKeyValue = row[4];
                record.data = row[5];
                record.timestamp = row[6];
                record.status = (row[7] == "1") ? SyncStatus::SUCCESS : SyncStatus::PENDING;
                
                changes.push_back(record);
            }
        }
        
        // 获取远程变更
        std::vector<std::vector<std::string>> remoteResults;
        if (QuerySQL("SELECT change_id, table_name, change_type, primary_key, primary_key_value, data, timestamp, source_node, synced "
                      "FROM remote_changes ORDER BY timestamp", remoteResults)) {
            for (const auto& row : remoteResults) {
                if (row.size() < 9) continue;
                ChangeRecord record;
                record.id = row[0];
                record.tableName = row[1];
                
                if (row[2] == "INSERT") record.changeType = ChangeType::INSERT;
                else if (row[2] == "UPDATE") record.changeType = ChangeType::UPDATE;
                else record.changeType = ChangeType::DELETE;
                
                record.primaryKey = row[3];
                record.primaryKeyValue = row[4];
                record.data = row[5];
                record.timestamp = row[6];
                record.sourceNode = row[7];
                record.status = (row[8] == "1") ? SyncStatus::SUCCESS : SyncStatus::PENDING;
                
                changes.push_back(record);
            }
        }
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to get all changes: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::MarkLocalChangeAsSynced(const std::string& changeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        sqlite3_stmt* stmt = nullptr;
        std::string sql = "UPDATE local_changes SET synced = 1 WHERE change_id = ?";
        
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to prepare mark local synced: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, changeId.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        return rc == SQLITE_DONE;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to mark local change as synced: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::MarkRemoteChangeAsSynced(const std::string& changeId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        sqlite3_stmt* stmt = nullptr;
        std::string sql = "UPDATE remote_changes SET synced = 1 WHERE change_id = ?";
        
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to prepare mark remote synced: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, changeId.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        return rc == SQLITE_DONE;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to mark remote change as synced: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::DetectConflict(const ChangeRecord& localChange, const ChangeRecord& remoteChange) {
    // 检查是否是同一条记录的变更
    if (localChange.tableName != remoteChange.tableName) {
        return false;
    }
    
    if (localChange.primaryKey != remoteChange.primaryKey) {
        return false;
    }
    
    if (localChange.primaryKeyValue != remoteChange.primaryKeyValue) {
        return false;
    }
    
    // 如果两者都是删除，不算冲突
    if (localChange.changeType == ChangeType::DELETE && 
        remoteChange.changeType == ChangeType::DELETE) {
        return false;
    }
    
    // 其他情况都是冲突
    return true;
}

bool ChangeTracker::ClearOldChanges(int daysToKeep) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        // 计算截止日期
        auto now = std::chrono::system_clock::now();
        auto cutoff = now - std::chrono::hours(24 * daysToKeep);
        auto time = std::chrono::system_clock::to_time_t(cutoff);
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        std::string cutoffStr = ss.str();
        
        // 使用参数化查询
        sqlite3_stmt* stmt = nullptr;
        
        // 删除旧的本地变更
        std::string sqlLocal = "DELETE FROM local_changes WHERE timestamp < ? AND synced = 1";
        int rc = sqlite3_prepare_v2(db_, sqlLocal.c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, cutoffStr.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        
        // 删除旧的远程变更
        std::string sqlRemote = "DELETE FROM remote_changes WHERE timestamp < ? AND synced = 1";
        rc = sqlite3_prepare_v2(db_, sqlRemote.c_str(), -1, &stmt, nullptr);
        if (rc == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, cutoffStr.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
        
        LOG_INFO("Cleared old changes before: " + cutoffStr);
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to clear old changes: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::SaveSyncPoint(const std::string& nodeId, const std::string& timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        sqlite3_stmt* stmt = nullptr;
        std::string sql = "INSERT OR REPLACE INTO sync_points (node_id, timestamp) VALUES (?, ?)";
        
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to prepare save sync point: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, timestamp.c_str(), -1, SQLITE_TRANSIENT);
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        return rc == SQLITE_DONE;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to save sync point: " + std::string(e.what()));
        return false;
    }
}

bool ChangeTracker::GetLastSyncPoint(const std::string& nodeId, std::string& timestamp) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return false;
    }
    
    try {
        sqlite3_stmt* stmt = nullptr;
        std::string sql = "SELECT timestamp FROM sync_points WHERE node_id = ?";
        
        int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            LOG_ERROR("Failed to prepare get sync point: " + std::string(sqlite3_errmsg(db_)));
            return false;
        }
        
        sqlite3_bind_text(stmt, 1, nodeId.c_str(), -1, SQLITE_TRANSIENT);
        
        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            timestamp = val ? val : "";
            found = true;
        }
        
        sqlite3_finalize(stmt);
        return found;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to get last sync point: " + std::string(e.what()));
        return false;
    }
}

} // namespace dbsync
