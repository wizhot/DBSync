#pragma once

#include "Common.h"
#include "FirebirdManager.h"
#include "ChangeTracker.h"
#include "NetworkManager.h"
#include "ConflictResolver.h"

namespace dbsync {

// 同步管理器 - 协调所有同步操作
class SyncManager {
public:
    SyncManager();
    ~SyncManager();
    
    // 初始化
    bool Initialize();
    void Shutdown();
    
    // 同步控制
    bool StartSync();
    void StopSync();
    bool IsSyncing() const { return syncing_; }
    
    // 手动触发同步
    bool TriggerSync();
    
    // 状态查询
    bool IsConnected() const;
    int GetPendingChangesCount() const;
    int GetSyncedRecordsCount() const;
    std::string GetLastSyncTime() const;
    std::string GetStatus() const;
    
    // 设置回调
    using SyncStatusCallback = std::function<void(const std::string& status, int progress)>;
    using ErrorCallback = std::function<void(const std::string& error)>;
    
    void SetSyncStatusCallback(SyncStatusCallback callback) { statusCallback_ = callback; }
    void SetErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }
    
private:
    // 同步线程函数
    void SyncThreadFunc();
    void MonitorThreadFunc();
    
    // 同步操作
    bool SyncLocalToRemote();
    bool SyncRemoteToLocal();
    bool ApplyChangeToLocal(const ChangeRecord& change);
    bool ApplyChangeToRemote(const ChangeRecord& change);
    
    // 消息处理
    void OnMessageReceived(const SyncMessage& message);
    void OnConnectionStatusChanged(bool connected);
    
    // 变更处理
    bool ProcessLocalChanges();
    bool ProcessRemoteChanges();
    bool HandleConflict(const ChangeRecord& localChange, const ChangeRecord& remoteChange);
    
    // 序列化/反序列化
    std::string SerializeChangeRecord(const ChangeRecord& record);
    bool DeserializeChangeRecord(const std::string& data, ChangeRecord& record);
    
    // 组件
    std::unique_ptr<FirebirdManager> localDb_;
    std::unique_ptr<FirebirdManager> remoteDb_;
    std::unique_ptr<ChangeTracker> changeTracker_;
    std::unique_ptr<NetworkManager> networkManager_;
    std::unique_ptr<ConflictResolver> conflictResolver_;
    
    // 线程
    std::atomic<bool> syncing_;
    std::atomic<bool> shouldStop_;
    std::thread syncThread_;
    std::thread monitorThread_;
    
    // 统计
    std::atomic<int> syncedRecordsCount_;
    std::string lastSyncTime_;
    mutable std::mutex statsMutex_;
    
    // 回调
    SyncStatusCallback statusCallback_;
    ErrorCallback errorCallback_;
    
    // 配置
    SyncConfig syncConfig_;
};

} // namespace dbsync
