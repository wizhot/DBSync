#pragma once

#include "Common.h"

namespace dbsync {

// 冲突解决策略
enum class ConflictStrategy {
    TIMESTAMP,      // 时间戳优先
    LOCAL_PRIORITY, // 本地优先
    REMOTE_PRIORITY,// 远程优先
    MANUAL          // 手动解决
};

// 冲突信息
struct ConflictInfo {
    ChangeRecord localChange;
    ChangeRecord remoteChange;
    std::string tableName;
    std::string primaryKey;
    std::string primaryKeyValue;
    std::string localTimestamp;
    std::string remoteTimestamp;
};

// 冲突解决器
class ConflictResolver {
public:
    ConflictResolver();
    ~ConflictResolver();
    
    // 设置策略
    void SetStrategy(ConflictStrategy strategy);
    void SetStrategy(const std::string& strategyName);
    
    // 冲突检测
    bool DetectConflict(const ChangeRecord& localChange, const ChangeRecord& remoteChange);
    
    // 冲突解决
    bool ResolveConflict(const ConflictInfo& conflict, ChangeRecord& winningChange);
    
    // 手动解决回调
    using ManualResolutionCallback = std::function<bool(const ConflictInfo& conflict, ChangeRecord& winningChange)>;
    void SetManualResolutionCallback(ManualResolutionCallback callback);
    
    // 获取冲突历史
    bool GetConflictHistory(std::vector<ConflictInfo>& conflicts);
    bool RecordConflict(const ConflictInfo& conflict, const ChangeRecord& resolution);
    
    // 合并数据（用于UPDATE冲突）
    bool MergeData(const std::map<std::string, std::string>& localData,
                   const std::map<std::string, std::string>& remoteData,
                   std::map<std::string, std::string>& mergedData);
    
private:
    // 具体策略实现
    bool ResolveByTimestamp(const ConflictInfo& conflict, ChangeRecord& winningChange);
    bool ResolveByLocalPriority(const ConflictInfo& conflict, ChangeRecord& winningChange);
    bool ResolveByRemotePriority(const ConflictInfo& conflict, ChangeRecord& winningChange);
    bool ResolveManually(const ConflictInfo& conflict, ChangeRecord& winningChange);
    
    // 工具函数
    int CompareTimestamps(const std::string& ts1, const std::string& ts2);
    std::map<std::string, std::string> ParseData(const std::string& data);
    std::string SerializeData(const std::map<std::string, std::string>& data);
    
    ConflictStrategy strategy_;
    ManualResolutionCallback manualCallback_;
    std::mutex mutex_;
};

} // namespace dbsync
