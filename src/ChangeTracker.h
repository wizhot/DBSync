#pragma once

#include "Common.h"
#include "FirebirdManager.h"

namespace dbsync {

// 变更追踪器 - 使用LiteSQL存储本地变更历史
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
    
    std::unique_ptr<litesql::Database> db_;
    std::string dbPath_;
    bool initialized_;
    std::mutex mutex_;
};

} // namespace dbsync
