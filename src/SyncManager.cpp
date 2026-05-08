#include "SyncManager.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <json/json.h>

namespace dbsync {

SyncManager::SyncManager()
    : syncing_(false)
    , shouldStop_(false)
    , syncedRecordsCount_(0) {
}

SyncManager::~SyncManager() {
    Shutdown();
}

bool SyncManager::Initialize() {
    LOG_INFO("Initializing SyncManager...");
    
    // 获取配置
    DatabaseConfig& localConfig = ConfigManager::GetInstance().GetLocalDbConfig();
    DatabaseConfig& remoteConfig = ConfigManager::GetInstance().GetRemoteDbConfig();
    NetworkConfig& networkConfig = ConfigManager::GetInstance().GetNetworkConfig();
    syncConfig_ = ConfigManager::GetInstance().GetSyncConfig();
    
    // 初始化本地数据库管理器
    localDb_ = std::make_unique<FirebirdManager>();
    if (!localDb_->Connect(localConfig)) {
        LOG_ERROR("Failed to connect to local database");
        if (errorCallback_) {
            errorCallback_("Failed to connect to local database: " + localDb_->GetLastError());
        }
        return false;
    }
    
    // 设置变更追踪
    if (!localDb_->SetupChangeTracking()) {
        LOG_WARNING("Failed to setup change tracking on local database");
    }
    
    // 初始化远程数据库管理器
    remoteDb_ = std::make_unique<FirebirdManager>();
    if (!remoteDb_->Connect(remoteConfig)) {
        LOG_ERROR("Failed to connect to remote database");
        if (errorCallback_) {
            errorCallback_("Failed to connect to remote database: " + remoteDb_->GetLastError());
        }
        // 不返回false，因为可能稍后通过网络同步
    }
    
    // 初始化变更追踪器
    changeTracker_ = std::make_unique<ChangeTracker>();
    std::string trackerDbPath = "dbsync_tracker.db";
    if (!changeTracker_->Initialize(trackerDbPath)) {
        LOG_ERROR("Failed to initialize change tracker");
        return false;
    }
    
    // 初始化网络管理器
    networkManager_ = std::make_unique<NetworkManager>();
    if (!networkManager_->Initialize(networkConfig)) {
        LOG_ERROR("Failed to initialize network manager");
        return false;
    }
    
    // 设置网络回调
    networkManager_->SetMessageHandler(
        [this](const SyncMessage& msg) { OnMessageReceived(msg); });
    networkManager_->SetConnectionHandler(
        [this](bool connected) { OnConnectionStatusChanged(connected); });
    
    // 启动服务器
    if (!networkManager_->StartServer()) {
        LOG_ERROR("Failed to start network server");
        return false;
    }
    
    // 初始化冲突解决器
    conflictResolver_ = std::make_unique<ConflictResolver>();
    conflictResolver_->SetStrategy(syncConfig_.conflictResolutionStrategy);
    
    LOG_INFO("SyncManager initialized successfully");
    return true;
}

void SyncManager::Shutdown() {
    LOG_INFO("Shutting down SyncManager...");
    
    StopSync();
    
    if (networkManager_) {
        networkManager_->Shutdown();
        networkManager_.reset();
    }
    
    if (changeTracker_) {
        changeTracker_->Shutdown();
        changeTracker_.reset();
    }
    
    if (localDb_) {
        localDb_->Disconnect();
        localDb_.reset();
    }
    
    if (remoteDb_) {
        remoteDb_->Disconnect();
        remoteDb_.reset();
    }
    
    LOG_INFO("SyncManager shutdown complete");
}

bool SyncManager::StartSync() {
    if (syncing_) {
        return true;
    }
    
    syncing_ = true;
    shouldStop_ = false;
    
    // 启动同步线程
    syncThread_ = std::thread(&SyncManager::SyncThreadFunc, this);
    
    // 启动监控线程
    monitorThread_ = std::thread(&SyncManager::MonitorThreadFunc, this);
    
    LOG_INFO("Synchronization started");
    return true;
}

void SyncManager::StopSync() {
    shouldStop_ = true;
    syncing_ = false;
    
    if (syncThread_.joinable()) {
        syncThread_.join();
    }
    
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
    
    LOG_INFO("Synchronization stopped");
}

bool SyncManager::TriggerSync() {
    if (!syncing_) {
        LOG_WARNING("Cannot trigger sync: sync is not running");
        return false;
    }
    
    LOG_INFO("Manual sync triggered");
    
    // 执行一次同步
    bool success = true;
    
    if (!SyncLocalToRemote()) {
        LOG_ERROR("Local to remote sync failed");
        success = false;
    }
    
    if (!SyncRemoteToLocal()) {
        LOG_ERROR("Remote to local sync failed");
        success = false;
    }
    
    return success;
}

void SyncManager::SyncThreadFunc() {
    LOG_INFO("Sync thread started");
    
    // 尝试连接到远程
    if (!networkManager_->IsConnectedToRemote()) {
        networkManager_->ConnectToRemote();
    }
    
    while (!shouldStop_) {
        if (statusCallback_) {
            statusCallback_("Synchronizing...", 0);
        }
        
        // 同步本地变更到远程
        if (!SyncLocalToRemote()) {
            LOG_WARNING("Local to remote sync encountered issues");
        }
        
        if (statusCallback_) {
            statusCallback_("Synchronizing...", 50);
        }
        
        // 同步远程变更到本地
        if (!SyncRemoteToLocal()) {
            LOG_WARNING("Remote to local sync encountered issues");
        }
        
        if (statusCallback_) {
            statusCallback_("Idle", 100);
        }
        
        // 更新最后同步时间
        {
            std::lock_guard<std::mutex> lock(statsMutex_);
            lastSyncTime_ = Utils::GetCurrentTimestamp();
        }
        
        // 等待下一次同步
        std::this_thread::sleep_for(std::chrono::milliseconds(syncConfig_.syncIntervalMs));
    }
    
    LOG_INFO("Sync thread ended");
}

void SyncManager::MonitorThreadFunc() {
    LOG_INFO("Monitor thread started");
    
    while (!shouldStop_) {
        // 检查数据库连接
        if (!localDb_->IsConnected()) {
            LOG_WARNING("Local database disconnected, attempting to reconnect...");
            if (!localDb_->Reconnect()) {
                LOG_ERROR("Failed to reconnect to local database");
            }
        }
        
        // 检查网络连接
        if (!networkManager_->IsConnectedToRemote()) {
            LOG_INFO("Attempting to connect to remote...");
            networkManager_->ConnectToRemote();
        }
        
        // 每秒检查一次
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    LOG_INFO("Monitor thread ended");
}

bool SyncManager::SyncLocalToRemote() {
    // 获取本地待同步的变更
    std::vector<ChangeRecord> localChanges;
    if (!localDb_->GetPendingChanges(localChanges)) {
        LOG_ERROR("Failed to get pending local changes");
        return false;
    }
    
    if (localChanges.empty()) {
        return true; // 没有变更需要同步
    }
    
    LOG_INFO("Found " + std::to_string(localChanges.size()) + " local changes to sync");
    
    // 通过网络发送变更
    for (const auto& change : localChanges) {
        if (shouldStop_) break;
        
        // 序列化变更记录
        std::string serialized = SerializeChangeRecord(change);
        
        // 创建同步消息
        SyncMessage message;
        message.messageType = 1; // change
        message.sourceNode = ConfigManager::GetInstance().GetNodeId();
        message.payload = serialized;
        message.timestamp = Utils::GetCurrentTimestamp();
        
        // 发送消息
        if (networkManager_->SendMessage(message)) {
            // 标记为已同步
            localDb_->MarkChangeAsSynced(change.id);
            changeTracker_->MarkLocalChangeAsSynced(change.id);
            
            syncedRecordsCount_++;
            LOG_INFO("Synced local change: " + change.tableName + "." + change.primaryKeyValue);
        } else {
            LOG_WARNING("Failed to send local change: " + change.tableName + "." + change.primaryKeyValue);
        }
    }
    
    return true;
}

bool SyncManager::SyncRemoteToLocal() {
    // 远程变更通过网络消息接收并处理
    // 这里处理从变更追踪器中获取的未同步远程变更
    
    std::vector<ChangeRecord> remoteChanges;
    if (!changeTracker_->GetUnsyncedRemoteChanges(remoteChanges)) {
        LOG_ERROR("Failed to get unsynced remote changes");
        return false;
    }
    
    for (const auto& change : remoteChanges) {
        if (shouldStop_) break;
        
        if (ApplyChangeToLocal(change)) {
            changeTracker_->MarkRemoteChangeAsSynced(change.id);
            syncedRecordsCount_++;
        }
    }
    
    return true;
}

bool SyncManager::ApplyChangeToLocal(const ChangeRecord& change) {
    // 检查是否需要同步此表
    if (!syncConfig_.syncAllTables) {
        auto it = std::find(syncConfig_.syncTables.begin(), syncConfig_.syncTables.end(), change.tableName);
        if (it == syncConfig_.syncTables.end()) {
            return true; // 跳过不同步的表
        }
    }
    
    // 应用变更
    switch (change.changeType) {
        case ChangeType::INSERT: {
            std::map<std::string, std::string> data;
            // 解析data字段获取完整数据
            // TODO: 实现数据解析
            if (!localDb_->InsertRecord(change.tableName, data)) {
                LOG_ERROR("Failed to apply INSERT to local: " + localDb_->GetLastError());
                return false;
            }
            break;
        }
        case ChangeType::UPDATE: {
            std::map<std::string, std::string> data;
            // 解析data字段获取完整数据
            // TODO: 实现数据解析
            if (!localDb_->UpdateRecord(change.tableName, change.primaryKey, change.primaryKeyValue, data)) {
                LOG_ERROR("Failed to apply UPDATE to local: " + localDb_->GetLastError());
                return false;
            }
            break;
        }
        case ChangeType::DELETE: {
            if (!localDb_->DeleteRecord(change.tableName, change.primaryKey, change.primaryKeyValue)) {
                LOG_ERROR("Failed to apply DELETE to local: " + localDb_->GetLastError());
                return false;
            }
            break;
        }
    }
    
    LOG_INFO("Applied remote change to local: " + change.tableName + "." + change.primaryKeyValue);
    return true;
}

bool SyncManager::ApplyChangeToRemote(const ChangeRecord& change) {
    // 通过网络发送变更
    std::string serialized = SerializeChangeRecord(change);
    
    SyncMessage message;
    message.messageType = 1; // change
    message.sourceNode = ConfigManager::GetInstance().GetNodeId();
    message.payload = serialized;
    message.timestamp = Utils::GetCurrentTimestamp();
    
    return networkManager_->SendMessage(message);
}

void SyncManager::OnMessageReceived(const SyncMessage& message) {
    if (message.messageType == 1) { // change
        // 反序列化变更记录
        ChangeRecord change;
        if (DeserializeChangeRecord(message.payload, change)) {
            // 记录远程变更
            change.sourceNode = message.sourceNode;
            changeTracker_->RecordRemoteChange(change);
            
            LOG_INFO("Received remote change: " + change.tableName + "." + change.primaryKeyValue);
        }
    }
}

void SyncManager::OnConnectionStatusChanged(bool connected) {
    if (connected) {
        LOG_INFO("Connection established with remote node");
    } else {
        LOG_WARNING("Connection lost with remote node");
    }
}

std::string SyncManager::SerializeChangeRecord(const ChangeRecord& record) {
    Json::Value root;
    root["id"] = record.id;
    root["tableName"] = record.tableName;
    root["changeType"] = (record.changeType == ChangeType::INSERT) ? "INSERT" :
                         (record.changeType == ChangeType::UPDATE) ? "UPDATE" : "DELETE";
    root["primaryKey"] = record.primaryKey;
    root["primaryKeyValue"] = record.primaryKeyValue;
    root["data"] = record.data;
    root["timestamp"] = record.timestamp;
    root["sourceNode"] = record.sourceNode;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, root);
}

bool SyncManager::DeserializeChangeRecord(const std::string& data, ChangeRecord& record) {
    try {
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(data.c_str(), data.c_str() + data.length(), &root, &errors)) {
            LOG_ERROR("Failed to parse change record: " + errors);
            return false;
        }
        
        record.id = root["id"].asString();
        record.tableName = root["tableName"].asString();
        
        std::string changeType = root["changeType"].asString();
        if (changeType == "INSERT") record.changeType = ChangeType::INSERT;
        else if (changeType == "UPDATE") record.changeType = ChangeType::UPDATE;
        else record.changeType = ChangeType::DELETE;
        
        record.primaryKey = root["primaryKey"].asString();
        record.primaryKeyValue = root["primaryKeyValue"].asString();
        record.data = root["data"].asString();
        record.timestamp = root["timestamp"].asString();
        record.sourceNode = root["sourceNode"].asString();
        record.status = SyncStatus::PENDING;
        record.retryCount = 0;
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception deserializing change record: " + std::string(e.what()));
        return false;
    }
}

bool SyncManager::IsConnected() const {
    return networkManager_ && networkManager_->IsConnectedToRemote();
}

int SyncManager::GetPendingChangesCount() const {
    if (!localDb_) return 0;
    
    std::vector<ChangeRecord> changes;
    if (!localDb_->GetPendingChanges(changes)) {
        return 0;
    }
    return static_cast<int>(changes.size());
}

int SyncManager::GetSyncedRecordsCount() const {
    return syncedRecordsCount_.load();
}

std::string SyncManager::GetLastSyncTime() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return lastSyncTime_;
}

std::string SyncManager::GetStatus() const {
    if (!syncing_) {
        return "Stopped";
    }
    
    if (!IsConnected()) {
        return "Disconnected";
    }
    
    int pending = GetPendingChangesCount();
    if (pending > 0) {
        return "Syncing (" + std::to_string(pending) + " pending)";
    }
    
    return "Idle";
}

} // namespace dbsync
