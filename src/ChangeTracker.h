#pragma once

#include "Common.h"

struct sqlite3;

namespace dbsync {

// 变更追踪器 - 使用SQLite存储本地变更历史
class ChangeTracker {
public:
    ChangeTracker();
    ~ChangeTracker();
    
    // 初始化
    bool Initialize(const std::string& dbPath);
    void Shutdown();
    
    // 记录变更
    bool RecordLocalChange(const ChangeRecord& change);
    bool RecordRemoteChange(const ChangeRecord& change);
    
    // 查询变更
    bool GetUnsyncedLocalChanges(std::vector<ChangeRecord>& changes);
    bool GetUnsyncedRemoteChanges(std::vector<ChangeRecord>& changes);
    bool GetAllChanges(std::vector<ChangeRecord>& changes);
    
    // 标记同步状态
    bool MarkLocalChangeAsSynced(const std::string& changeId);
    bool MarkRemoteChangeAsSynced(const std::string& changeId);
    
    // 冲突检测
    bool DetectConflict(const ChangeRecord& localChange, const ChangeRecord& remoteChange);
    
    // 清理历史记录
    bool ClearOldChanges(int daysToKeep);
    
    // 同步点管理
    bool SaveSyncPoint(const std::string& nodeId, const std::string& timestamp);
    bool GetLastSyncPoint(const std::string& nodeId, std::string& timestamp);
    
private:
    // 数据库表结构定义
    bool CreateSchema();
    
    // 内部查询辅助
    bool ExecuteSQL(const std::string& sql);
    bool QuerySQL(const std::string& sql, std::vector<std::vector<std::string>>& results);
    
    sqlite3* db_;
    std::string dbPath_;
    bool initialized_;
    std::mutex mutex_;
};

} // namespace dbsync
