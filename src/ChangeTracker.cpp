#include "ChangeTracker.h"
#include "Logger.h"

namespace dbsync {

ChangeTracker::ChangeTracker() : initialized_(false) {
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
        // 创建或打开LiteSQL数据库
        db_ = std::make_unique<litesql::Database>("sqlite3", "database=" + dbPath);
        
        // 创建表结构
        if (!CreateSchema()) {
            LOG_ERROR("Failed to create database schema");
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
        db_->close();
        db_.reset();
    }
    
    initialized_ = false;
    LOG_INFO("ChangeTracker shutdown");
}

bool ChangeTracker::CreateSchema() {
    try {
        // 创建本地变更表
        litesql::Record localChanges;
        localChanges["change_id"] = litesql::FieldType(litesql::FieldType::String);
        localChanges["table_name"] = litesql::FieldType(litesql::FieldType::String);
        localChanges["change_type"] = litesql::FieldType(litesql::FieldType::String);
        localChanges["primary_key"] = litesql::FieldType(litesql::FieldType::String);
        localChanges["primary_key_value"] = litesql::FieldType(litesql::FieldType::String);
        localChanges["data"] = litesql::FieldType(litesql::FieldType::String);
        localChanges["timestamp"] = litesql::FieldType(litesql::FieldType::String);
        localChanges["synced"] = litesql::FieldType(litesql::FieldType::Integer);
        
        db_->createTable("local_changes", localChanges);
        
        // 创建远程变更表
        litesql::Record remoteChanges;
        remoteChanges["change_id"] = litesql::FieldType(litesql::FieldType::String);
        remoteChanges["table_name"] = litesql::FieldType(litesql::FieldType::String);
        remoteChanges["change_type"] = litesql::FieldType(litesql::FieldType::String);
        remoteChanges["primary_key"] = litesql::FieldType(litesql::FieldType::String);
        remoteChanges["primary_key_value"] = litesql::FieldType(litesql::FieldType::String);
        remoteChanges["data"] = litesql::FieldType(litesql::FieldType::String);
        remoteChanges["timestamp"] = litesql::FieldType(litesql::FieldType::String);
        remoteChanges["source_node"] = litesql::FieldType(litesql::FieldType::String);
        remoteChanges["synced"] = litesql::FieldType(litesql::FieldType::Integer);
        
        db_->createTable("remote_changes", remoteChanges);
        
        // 创建同步点表
        litesql::Record syncPoints;
        syncPoints["node_id"] = litesql::FieldType(litesql::FieldType::String);
        syncPoints["timestamp"] = litesql::FieldType(litesql::FieldType::String);
        
        db_->createTable("sync_points", syncPoints);
        
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
        litesql::Record record;
        record["change_id"] = change.id;
        record["table_name"] = change.tableName;
        record["change_type"] = (change.changeType == ChangeType::INSERT) ? "INSERT" :
                                (change.changeType == ChangeType::UPDATE) ? "UPDATE" : "DELETE";
        record["primary_key"] = change.primaryKey;
        record["primary_key_value"] = change.primaryKeyValue;
        record["data"] = change.data;
        record["timestamp"] = change.timestamp;
        record["synced"] = 0;
        
        db_->insert("local_changes", record);
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
        litesql::Record record;
        record["change_id"] = change.id;
        record["table_name"] = change.tableName;
        record["change_type"] = (change.changeType == ChangeType::INSERT) ? "INSERT" :
                                (change.changeType == ChangeType::UPDATE) ? "UPDATE" : "DELETE";
        record["primary_key"] = change.primaryKey;
        record["primary_key_value"] = change.primaryKeyValue;
        record["data"] = change.data;
        record["timestamp"] = change.timestamp;
        record["source_node"] = change.sourceNode;
        record["synced"] = 0;
        
        db_->insert("remote_changes", record);
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
        litesql::Cursor cursor = db_->query(
            "SELECT * FROM local_changes WHERE synced = 0 ORDER BY timestamp"
        );
        
        while (cursor.next()) {
            ChangeRecord record;
            record.id = cursor["change_id"].toString();
            record.tableName = cursor["table_name"].toString();
            
            std::string changeType = cursor["change_type"].toString();
            if (changeType == "INSERT") record.changeType = ChangeType::INSERT;
            else if (changeType == "UPDATE") record.changeType = ChangeType::UPDATE;
            else record.changeType = ChangeType::DELETE;
            
            record.primaryKey = cursor["primary_key"].toString();
            record.primaryKeyValue = cursor["primary_key_value"].toString();
            record.data = cursor["data"].toString();
            record.timestamp = cursor["timestamp"].toString();
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
        litesql::Cursor cursor = db_->query(
            "SELECT * FROM remote_changes WHERE synced = 0 ORDER BY timestamp"
        );
        
        while (cursor.next()) {
            ChangeRecord record;
            record.id = cursor["change_id"].toString();
            record.tableName = cursor["table_name"].toString();
            
            std::string changeType = cursor["change_type"].toString();
            if (changeType == "INSERT") record.changeType = ChangeType::INSERT;
            else if (changeType == "UPDATE") record.changeType = ChangeType::UPDATE;
            else record.changeType = ChangeType::DELETE;
            
            record.primaryKey = cursor["primary_key"].toString();
            record.primaryKeyValue = cursor["primary_key_value"].toString();
            record.data = cursor["data"].toString();
            record.timestamp = cursor["timestamp"].toString();
            record.sourceNode = cursor["source_node"].toString();
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
        litesql::Cursor localCursor = db_->query("SELECT * FROM local_changes ORDER BY timestamp");
        while (localCursor.next()) {
            ChangeRecord record;
            record.id = localCursor["change_id"].toString();
            record.tableName = localCursor["table_name"].toString();
            
            std::string changeType = localCursor["change_type"].toString();
            if (changeType == "INSERT") record.changeType = ChangeType::INSERT;
            else if (changeType == "UPDATE") record.changeType = ChangeType::UPDATE;
            else record.changeType = ChangeType::DELETE;
            
            record.primaryKey = localCursor["primary_key"].toString();
            record.primaryKeyValue = localCursor["primary_key_value"].toString();
            record.data = localCursor["data"].toString();
            record.timestamp = localCursor["timestamp"].toString();
            record.status = localCursor["synced"].toInt() ? SyncStatus::SUCCESS : SyncStatus::PENDING;
            
            changes.push_back(record);
        }
        
        // 获取远程变更
        litesql::Cursor remoteCursor = db_->query("SELECT * FROM remote_changes ORDER BY timestamp");
        while (remoteCursor.next()) {
            ChangeRecord record;
            record.id = remoteCursor["change_id"].toString();
            record.tableName = remoteCursor["table_name"].toString();
            
            std::string changeType = remoteCursor["change_type"].toString();
            if (changeType == "INSERT") record.changeType = ChangeType::INSERT;
            else if (changeType == "UPDATE") record.changeType = ChangeType::UPDATE;
            else record.changeType = ChangeType::DELETE;
            
            record.primaryKey = remoteCursor["primary_key"].toString();
            record.primaryKeyValue = remoteCursor["primary_key_value"].toString();
            record.data = remoteCursor["data"].toString();
            record.timestamp = remoteCursor["timestamp"].toString();
            record.sourceNode = remoteCursor["source_node"].toString();
            record.status = remoteCursor["synced"].toInt() ? SyncStatus::SUCCESS : SyncStatus::PENDING;
            
            changes.push_back(record);
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
        db_->execute("UPDATE local_changes SET synced = 1 WHERE change_id = '" + changeId + "'");
        return true;
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
        db_->execute("UPDATE remote_changes SET synced = 1 WHERE change_id = '" + changeId + "'");
        return true;
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
        
        // 删除旧的本地变更
        db_->execute("DELETE FROM local_changes WHERE timestamp < '" + cutoffStr + "' AND synced = 1");
        
        // 删除旧的远程变更
        db_->execute("DELETE FROM remote_changes WHERE timestamp < '" + cutoffStr + "' AND synced = 1");
        
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
        // 先删除旧的同步点
        db_->execute("DELETE FROM sync_points WHERE node_id = '" + nodeId + "'");
        
        // 插入新的同步点
        litesql::Record record;
        record["node_id"] = nodeId;
        record["timestamp"] = timestamp;
        
        db_->insert("sync_points", record);
        return true;
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
        litesql::Cursor cursor = db_->query(
            "SELECT timestamp FROM sync_points WHERE node_id = '" + nodeId + "'"
        );
        
        if (cursor.next()) {
            timestamp = cursor["timestamp"].toString();
            return true;
        }
        
        return false;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to get last sync point: " + std::string(e.what()));
        return false;
    }
}

} // namespace dbsync
