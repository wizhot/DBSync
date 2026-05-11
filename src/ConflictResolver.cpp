#include "ConflictResolver.h"
#include "Logger.h"
#include <json/json.h>

namespace dbsync {

ConflictResolver::ConflictResolver() : strategy_(ConflictStrategy::TIMESTAMP) {
}

ConflictResolver::~ConflictResolver() {
}

void ConflictResolver::SetStrategy(ConflictStrategy strategy) {
    std::lock_guard<std::mutex> lock(mutex_);
    strategy_ = strategy;
    LOG_INFO("Conflict resolution strategy set to: " + std::to_string(static_cast<int>(strategy)));
}

void ConflictResolver::SetStrategy(const std::string& strategyName) {
    if (strategyName == "timestamp") {
        SetStrategy(ConflictStrategy::TIMESTAMP);
    } else if (strategyName == "local") {
        SetStrategy(ConflictStrategy::LOCAL_PRIORITY);
    } else if (strategyName == "remote") {
        SetStrategy(ConflictStrategy::REMOTE_PRIORITY);
    } else if (strategyName == "manual") {
        SetStrategy(ConflictStrategy::MANUAL);
    } else {
        LOG_WARNING("Unknown conflict resolution strategy: " + strategyName + ", using timestamp");
        SetStrategy(ConflictStrategy::TIMESTAMP);
    }
}

bool ConflictResolver::DetectConflict(const ChangeRecord& localChange, const ChangeRecord& remoteChange) {
    // 检查是否是同一张表
    if (localChange.tableName != remoteChange.tableName) {
        return false;
    }
    
    // 检查是否是同一条记录（主键相同）
    if (localChange.primaryKey != remoteChange.primaryKey) {
        return false;
    }
    
    if (localChange.primaryKeyValue != remoteChange.primaryKeyValue) {
        return false;
    }
    
    // 如果两者都是DELETE，不算冲突（结果一样）
    if (localChange.changeType == ChangeType::DELETE && 
        remoteChange.changeType == ChangeType::DELETE) {
        return false;
    }
    
    // 检测到冲突
    return true;
}

bool ConflictResolver::ResolveConflict(const ConflictInfo& conflict, ChangeRecord& winningChange) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    switch (strategy_) {
        case ConflictStrategy::TIMESTAMP:
            return ResolveByTimestamp(conflict, winningChange);
        case ConflictStrategy::LOCAL_PRIORITY:
            return ResolveByLocalPriority(conflict, winningChange);
        case ConflictStrategy::REMOTE_PRIORITY:
            return ResolveByRemotePriority(conflict, winningChange);
        case ConflictStrategy::MANUAL:
            return ResolveManually(conflict, winningChange);
        default:
            return ResolveByTimestamp(conflict, winningChange);
    }
}

void ConflictResolver::SetManualResolutionCallback(ManualResolutionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    manualCallback_ = callback;
}

bool ConflictResolver::ResolveByTimestamp(const ConflictInfo& conflict, ChangeRecord& winningChange) {
    int comparison = CompareTimestamps(conflict.localTimestamp, conflict.remoteTimestamp);
    
    if (comparison >= 0) {
        // 本地时间戳更新或相同，使用本地变更
        winningChange = conflict.localChange;
        LOG_INFO("Conflict resolved by timestamp: local wins (local=" + 
                 conflict.localTimestamp + ", remote=" + conflict.remoteTimestamp + ")");
    } else {
        // 远程时间戳更新，使用远程变更
        winningChange = conflict.remoteChange;
        LOG_INFO("Conflict resolved by timestamp: remote wins (local=" + 
                 conflict.localTimestamp + ", remote=" + conflict.remoteTimestamp + ")");
    }
    
    return true;
}

bool ConflictResolver::ResolveByLocalPriority(const ConflictInfo& conflict, ChangeRecord& winningChange) {
    winningChange = conflict.localChange;
    LOG_INFO("Conflict resolved by local priority: local wins");
    return true;
}

bool ConflictResolver::ResolveByRemotePriority(const ConflictInfo& conflict, ChangeRecord& winningChange) {
    winningChange = conflict.remoteChange;
    LOG_INFO("Conflict resolved by remote priority: remote wins");
    return true;
}

bool ConflictResolver::ResolveManually(const ConflictInfo& conflict, ChangeRecord& winningChange) {
    if (manualCallback_) {
        return manualCallback_(conflict, winningChange);
    }
    
    // 如果没有设置手动回调，默认使用时间戳策略
    LOG_WARNING("No manual resolution callback set, falling back to timestamp strategy");
    return ResolveByTimestamp(conflict, winningChange);
}

bool ConflictResolver::GetConflictHistory(std::vector<ConflictInfo>& conflicts) {
    // TODO: 从持久化存储中读取冲突历史
    // 这里可以实现将冲突记录到文件或数据库
    return true;
}

bool ConflictResolver::RecordConflict(const ConflictInfo& conflict, const ChangeRecord& resolution) {
    // TODO: 记录冲突到日志或数据库
    LOG_INFO("Conflict recorded: table=" + conflict.tableName + 
             ", pk=" + conflict.primaryKeyValue + 
             ", resolution=" + (resolution.sourceNode.empty() ? "local" : "remote"));
    return true;
}

bool ConflictResolver::MergeData(const std::map<std::string, std::string>& localData,
                                  const std::map<std::string, std::string>& remoteData,
                                  std::map<std::string, std::string>& mergedData) {
    mergedData.clear();
    
    // 合并两个数据集
    // 对于相同的字段，使用远程的值（可以根据策略调整）
    for (const auto& pair : localData) {
        mergedData[pair.first] = pair.second;
    }
    
    for (const auto& pair : remoteData) {
        mergedData[pair.first] = pair.second;
    }
    
    return true;
}

int ConflictResolver::CompareTimestamps(const std::string& ts1, const std::string& ts2) {
    // 简单字符串比较（格式：YYYY-MM-DD HH:MM:SS）
    // 这种格式可以直接按字典序比较
    if (ts1 == ts2) {
        return 0;
    }
    return (ts1 > ts2) ? 1 : -1;
}

std::map<std::string, std::string> ConflictResolver::ParseData(const std::string& data) {
    std::map<std::string, std::string> result;
    
    try {
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (reader->parse(data.c_str(), data.c_str() + data.length(), &root, &errors)) {
            for (const auto& member : root.getMemberNames()) {
                result[member] = root[member].asString();
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to parse data: " + std::string(e.what()));
    }
    
    return result;
}

std::string ConflictResolver::SerializeData(const std::map<std::string, std::string>& data) {
    Json::Value root;
    for (const auto& pair : data) {
        root[pair.first] = pair.second;
    }
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

} // namespace dbsync
