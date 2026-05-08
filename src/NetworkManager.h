#pragma once

#include "Common.h"

namespace dbsync {

// 网络消息处理器回调
using MessageHandler = std::function<void(const SyncMessage& message)>;
using ConnectionHandler = std::function<void(bool connected)>;

// 网络管理器 - 处理TCP通信
class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    
    // 初始化
    bool Initialize(const NetworkConfig& config);
    void Shutdown();
    
    // 服务器功能
    bool StartServer();
    void StopServer();
    bool IsServerRunning() const { return serverRunning_; }
    
    // 客户端功能
    bool ConnectToRemote();
    void DisconnectFromRemote();
    bool IsConnectedToRemote() const { return clientConnected_; }
    
    // 消息发送
    bool SendMessage(const SyncMessage& message);
    bool BroadcastMessage(const SyncMessage& message);
    
    // 设置回调
    void SetMessageHandler(MessageHandler handler) { messageHandler_ = handler; }
    void SetConnectionHandler(ConnectionHandler handler) { connectionHandler_ = handler; }
    
    // 心跳管理
    void StartHeartbeat();
    void StopHeartbeat();
    
    // 获取统计信息
    int GetSentMessageCount() const { return sentMessageCount_; }
    int GetReceivedMessageCount() const { return receivedMessageCount_; }
    
private:
    // 服务器线程函数
    void ServerThreadFunc();
    void ClientHandlerThreadFunc(SOCKET clientSocket);
    
    // 客户端线程函数
    void ClientThreadFunc();
    void ClientReceiveThreadFunc();
    
    // 心跳线程函数
    void HeartbeatThreadFunc();
    
    // 消息处理
    void ProcessReceivedData(const std::string& data);
    std::string SerializeMessage(const SyncMessage& message);
    bool DeserializeMessage(const std::string& data, SyncMessage& message);
    
    // 网络工具函数
    bool SendData(SOCKET socket, const std::string& data);
    bool ReceiveData(SOCKET socket, std::string& data);
    
    NetworkConfig config_;
    
    // 服务器相关
    SOCKET serverSocket_;
    std::vector<SOCKET> clientSockets_;
    std::mutex clientSocketsMutex_;
    std::atomic<bool> serverRunning_;
    std::thread serverThread_;
    
    // 客户端相关
    SOCKET clientSocket_;
    std::atomic<bool> clientConnected_;
    std::thread clientThread_;
    std::thread clientReceiveThread_;
    
    // 心跳相关
    std::atomic<bool> heartbeatRunning_;
    std::thread heartbeatThread_;
    std::chrono::steady_clock::time_point lastHeartbeatReceived_;
    
    // 回调
    MessageHandler messageHandler_;
    ConnectionHandler connectionHandler_;
    
    // 统计
    std::atomic<int> sentMessageCount_;
    std::atomic<int> receivedMessageCount_;
    
    // WSA初始化标志
    bool wsaInitialized_;
};

} // namespace dbsync
