#include "NetworkManager.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <json/json.h>

namespace dbsync {

NetworkManager::NetworkManager()
    : serverSocket_(INVALID_SOCKET)
    , clientSocket_(INVALID_SOCKET)
    , serverRunning_(false)
    , clientConnected_(false)
    , heartbeatRunning_(false)
    , sentMessageCount_(0)
    , receivedMessageCount_(0)
    , wsaInitialized_(false) {
}

NetworkManager::~NetworkManager() {
    Shutdown();
}

bool NetworkManager::Initialize(const NetworkConfig& config) {
    config_ = config;
    
    // 初始化Winsock
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG_ERROR("WSAStartup failed: " + std::to_string(result));
        return false;
    }
    
    wsaInitialized_ = true;
    LOG_INFO("NetworkManager initialized");
    return true;
}

void NetworkManager::Shutdown() {
    StopServer();
    DisconnectFromRemote();
    StopHeartbeat();
    
    if (wsaInitialized_) {
        WSACleanup();
        wsaInitialized_ = false;
    }
    
    LOG_INFO("NetworkManager shutdown");
}

bool NetworkManager::StartServer() {
    if (serverRunning_) {
        return true;
    }
    
    // 创建服务器socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket_ == INVALID_SOCKET) {
        LOG_ERROR("Failed to create server socket: " + std::to_string(WSAGetLastError()));
        return false;
    }
    
    // 设置地址重用
    int reuse = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse)) == SOCKET_ERROR) {
        LOG_WARNING("Failed to set SO_REUSEADDR: " + std::to_string(WSAGetLastError()));
    }
    
    // 绑定地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(config_.localPort);
    serverAddr.sin_addr.s_addr = inet_addr(config_.localIp.c_str());
    
    if (bind(serverSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        LOG_ERROR("Failed to bind server socket: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }
    
    // 开始监听
    if (listen(serverSocket_, SOMAXCONN) == SOCKET_ERROR) {
        LOG_ERROR("Failed to listen on server socket: " + std::to_string(WSAGetLastError()));
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
        return false;
    }
    
    serverRunning_ = true;
    serverThread_ = std::thread(&NetworkManager::ServerThreadFunc, this);
    
    LOG_INFO("Server started on " + config_.localIp + ":" + std::to_string(config_.localPort));
    return true;
}

void NetworkManager::StopServer() {
    serverRunning_ = false;
    
    // 关闭所有客户端连接
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        for (auto& client : clientSockets_) {
            closesocket(client);
        }
        clientSockets_.clear();
    }
    
    // 关闭服务器socket
    if (serverSocket_ != INVALID_SOCKET) {
        closesocket(serverSocket_);
        serverSocket_ = INVALID_SOCKET;
    }
    
    // 等待服务器线程结束
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    
    LOG_INFO("Server stopped");
}

void NetworkManager::ServerThreadFunc() {
    LOG_INFO("Server thread started");
    
    while (serverRunning_) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(serverSocket_, &readSet);
        
        timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int result = select(0, &readSet, nullptr, nullptr, &timeout);
        if (result == SOCKET_ERROR) {
            LOG_ERROR("Select failed: " + std::to_string(WSAGetLastError()));
            break;
        }
        
        if (result > 0 && FD_ISSET(serverSocket_, &readSet)) {
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);
            SOCKET clientSocket = accept(serverSocket_, (sockaddr*)&clientAddr, &clientAddrLen);
            
            if (clientSocket != INVALID_SOCKET) {
                char* clientIp = inet_ntoa(clientAddr.sin_addr);
                LOG_INFO("Client connected from: " + std::string(clientIp));
                
                {
                    std::lock_guard<std::mutex> lock(clientSocketsMutex_);
                    clientSockets_.push_back(clientSocket);
                }
                
                // 启动客户端处理线程
                std::thread handlerThread(&NetworkManager::ClientHandlerThreadFunc, this, clientSocket);
                handlerThread.detach();
                
                if (connectionHandler_) {
                    connectionHandler_(true);
                }
            }
        }
    }
    
    LOG_INFO("Server thread ended");
}

void NetworkManager::ClientHandlerThreadFunc(SOCKET clientSocket) {
    LOG_INFO("Client handler thread started");
    
    while (serverRunning_) {
        std::string data;
        if (!ReceiveData(clientSocket, data)) {
            break;
        }
        
        if (!data.empty()) {
            ProcessReceivedData(data);
        }
    }
    
    // 从列表中移除客户端
    {
        std::lock_guard<std::mutex> lock(clientSocketsMutex_);
        auto it = std::find(clientSockets_.begin(), clientSockets_.end(), clientSocket);
        if (it != clientSockets_.end()) {
            clientSockets_.erase(it);
        }
    }
    
    closesocket(clientSocket);
    LOG_INFO("Client handler thread ended");
    
    if (connectionHandler_) {
        connectionHandler_(false);
    }
}

bool NetworkManager::ConnectToRemote() {
    if (clientConnected_) {
        return true;
    }
    
    // 创建客户端socket
    clientSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket_ == INVALID_SOCKET) {
        LOG_ERROR("Failed to create client socket: " + std::to_string(WSAGetLastError()));
        return false;
    }
    
    // 设置连接超时
    DWORD timeout = config_.connectionTimeoutMs;
    setsockopt(clientSocket_, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(clientSocket_, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));
    
    // 连接到远程服务器
    sockaddr_in remoteAddr;
    remoteAddr.sin_family = AF_INET;
    remoteAddr.sin_port = htons(config_.remotePort);
    remoteAddr.sin_addr.s_addr = inet_addr(config_.remoteIp.c_str());
    
    if (connect(clientSocket_, (sockaddr*)&remoteAddr, sizeof(remoteAddr)) == SOCKET_ERROR) {
        LOG_ERROR("Failed to connect to remote server: " + std::to_string(WSAGetLastError()));
        closesocket(clientSocket_);
        clientSocket_ = INVALID_SOCKET;
        return false;
    }
    
    clientConnected_ = true;
    
    // 启动接收线程
    clientReceiveThread_ = std::thread(&NetworkManager::ClientReceiveThreadFunc, this);
    
    // 启动心跳
    StartHeartbeat();
    
    LOG_INFO("Connected to remote server: " + config_.remoteIp + ":" + std::to_string(config_.remotePort));
    
    if (connectionHandler_) {
        connectionHandler_(true);
    }
    
    return true;
}

void NetworkManager::DisconnectFromRemote() {
    clientConnected_ = false;
    StopHeartbeat();
    
    if (clientSocket_ != INVALID_SOCKET) {
        closesocket(clientSocket_);
        clientSocket_ = INVALID_SOCKET;
    }
    
    if (clientReceiveThread_.joinable()) {
        clientReceiveThread_.join();
    }
    
    LOG_INFO("Disconnected from remote server");
    
    if (connectionHandler_) {
        connectionHandler_(false);
    }
}

void NetworkManager::ClientReceiveThreadFunc() {
    LOG_INFO("Client receive thread started");
    
    while (clientConnected_) {
        std::string data;
        if (!ReceiveData(clientSocket_, data)) {
            // 连接断开
            LOG_WARNING("Connection lost, will retry...");
            clientConnected_ = false;
            break;
        }
        
        if (!data.empty()) {
            ProcessReceivedData(data);
        }
    }
    
    LOG_INFO("Client receive thread ended");
    
    // 尝试重新连接
    if (connectionHandler_) {
        connectionHandler_(false);
    }
}

bool NetworkManager::SendMessage(const SyncMessage& message) {
    if (!clientConnected_) {
        LOG_WARNING("Cannot send message: not connected to remote");
        return false;
    }
    
    std::string data = SerializeMessage(message);
    if (!SendData(clientSocket_, data)) {
        LOG_ERROR("Failed to send message to remote");
        clientConnected_ = false;
        return false;
    }
    
    sentMessageCount_++;
    return true;
}

bool NetworkManager::BroadcastMessage(const SyncMessage& message) {
    std::lock_guard<std::mutex> lock(clientSocketsMutex_);
    
    std::string data = SerializeMessage(message);
    bool allSuccess = true;
    
    for (auto it = clientSockets_.begin(); it != clientSockets_.end();) {
        if (!SendData(*it, data)) {
            LOG_WARNING("Failed to send message to a client, removing from list");
            closesocket(*it);
            it = clientSockets_.erase(it);
            allSuccess = false;
        } else {
            ++it;
        }
    }
    
    if (allSuccess) {
        sentMessageCount_++;
    }
    
    return allSuccess;
}

void NetworkManager::StartHeartbeat() {
    if (heartbeatRunning_) {
        return;
    }
    
    heartbeatRunning_ = true;
    lastHeartbeatReceived_ = std::chrono::steady_clock::now();
    heartbeatThread_ = std::thread(&NetworkManager::HeartbeatThreadFunc, this);
    
    LOG_INFO("Heartbeat started");
}

void NetworkManager::StopHeartbeat() {
    heartbeatRunning_ = false;
    
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
    
    LOG_INFO("Heartbeat stopped");
}

void NetworkManager::HeartbeatThreadFunc() {
    LOG_INFO("Heartbeat thread started");
    
    while (heartbeatRunning_) {
        // 发送心跳
        SyncMessage heartbeat;
        heartbeat.messageType = 2; // heartbeat
        heartbeat.sourceNode = ConfigManager::GetInstance().GetNodeId();
        heartbeat.timestamp = Utils::GetCurrentTimestamp();
        
        if (clientConnected_) {
            SendMessage(heartbeat);
        }
        
        // 检查是否收到心跳
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeatReceived_).count();
        
        if (elapsed > 30) { // 30秒未收到心跳认为断开
            LOG_WARNING("Heartbeat timeout detected");
            clientConnected_ = false;
        }
        
        // 等待5秒
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    LOG_INFO("Heartbeat thread ended");
}

void NetworkManager::ProcessReceivedData(const std::string& data) {
    SyncMessage message;
    if (!DeserializeMessage(data, message)) {
        LOG_WARNING("Failed to deserialize message");
        return;
    }
    
    receivedMessageCount_++;
    
    // 更新心跳时间
    if (message.messageType == 2) { // heartbeat
        lastHeartbeatReceived_ = std::chrono::steady_clock::now();
        return;
    }
    
    // 调用消息处理器
    if (messageHandler_) {
        messageHandler_(message);
    }
}

std::string NetworkManager::SerializeMessage(const SyncMessage& message) {
    Json::Value root;
    root["messageType"] = message.messageType;
    root["sourceNode"] = message.sourceNode;
    root["targetNode"] = message.targetNode;
    root["payload"] = message.payload;
    root["timestamp"] = message.timestamp;
    
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string jsonStr = Json::writeString(builder, root);
    
    // 添加长度前缀
    uint32_t length = htonl(static_cast<uint32_t>(jsonStr.length()));
    std::string result;
    result.append(reinterpret_cast<char*>(&length), sizeof(length));
    result.append(jsonStr);
    
    return result;
}

bool NetworkManager::DeserializeMessage(const std::string& data, SyncMessage& message) {
    try {
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        if (!reader->parse(data.c_str(), data.c_str() + data.length(), &root, &errors)) {
            LOG_ERROR("JSON parse error: " + errors);
            return false;
        }
        
        message.messageType = root["messageType"].asInt();
        message.sourceNode = root["sourceNode"].asString();
        message.targetNode = root["targetNode"].asString();
        message.payload = root["payload"].asString();
        message.timestamp = root["timestamp"].asString();
        
        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to deserialize message: " + std::string(e.what()));
        return false;
    }
}

bool NetworkManager::SendData(SOCKET socket, const std::string& data) {
    int totalSent = 0;
    int dataLen = static_cast<int>(data.length());
    
    while (totalSent < dataLen) {
        int sent = send(socket, data.c_str() + totalSent, dataLen - totalSent, 0);
        if (sent == SOCKET_ERROR) {
            LOG_ERROR("Send failed: " + std::to_string(WSAGetLastError()));
            return false;
        }
        totalSent += sent;
    }
    
    return true;
}

bool NetworkManager::ReceiveData(SOCKET socket, std::string& data) {
    data.clear();
    
    // 首先读取长度前缀
    uint32_t length;
    int received = recv(socket, reinterpret_cast<char*>(&length), sizeof(length), MSG_WAITALL);
    if (received == 0 || received == SOCKET_ERROR) {
        return false;
    }
    
    length = ntohl(length);
    if (length > 1024 * 1024) { // 最大1MB
        LOG_ERROR("Message too large: " + std::to_string(length));
        return false;
    }
    
    // 读取消息体
    data.resize(length);
    int totalReceived = 0;
    
    while (totalReceived < static_cast<int>(length)) {
        received = recv(socket, &data[0] + totalReceived, length - totalReceived, 0);
        if (received == 0 || received == SOCKET_ERROR) {
            return false;
        }
        totalReceived += received;
    }
    
    return true;
}

} // namespace dbsync
