#pragma once

#include "Common.h"
#include "SyncManager.h"
#include "SystemTray.h"

namespace dbsync {

// 应用程序主类
class DbSyncApp {
public:
    DbSyncApp();
    ~DbSyncApp();
    
    // 禁止拷贝
    DbSyncApp(const DbSyncApp&) = delete;
    DbSyncApp& operator=(const DbSyncApp&) = delete;
    
    // 初始化和运行
    bool Initialize(HINSTANCE hInstance, int nCmdShow, LPWSTR lpCmdLine);
    int Run();
    void Shutdown();
    
    // 获取实例
    static DbSyncApp* GetInstance() { return instance_; }
    
    // 获取组件
    SyncManager* GetSyncManager() { return syncManager_.get(); }
    MainWindow* GetMainWindow() { return &mainWindow_; }
    
    // 命令行参数
    bool IsMinimizedStart() const { return minimizedStart_; }
    bool IsAutoStart() const { return autoStart_; }
    
private:
    // 初始化组件
    bool InitializeLogging();
    bool InitializeConfiguration();
    bool InitializeDatabase();
    bool InitializeNetwork();
    bool InitializeUI(int nCmdShow);
    
    // 解析命令行
    void ParseCommandLine(LPWSTR lpCmdLine);
    
    // 设置开机自启
    bool SetupAutoStart();
    
    // 信号处理
    static BOOL WINAPI ConsoleHandler(DWORD signal);
    
    // 单例实例
    static DbSyncApp* instance_;
    
    // 组件
    std::unique_ptr<SyncManager> syncManager_;
    MainWindow mainWindow_;
    
    // Windows实例
    HINSTANCE hInstance_;
    
    // 命令行参数
    bool minimizedStart_;
    bool autoStart_;
    bool showConsole_;
    
    // 运行状态
    std::atomic<bool> running_;
};

} // namespace dbsync
