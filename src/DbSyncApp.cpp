#include "DbSyncApp.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <shellapi.h>

namespace dbsync {

DbSyncApp* DbSyncApp::instance_ = nullptr;

DbSyncApp::DbSyncApp()
    : hInstance_(nullptr)
    , minimizedStart_(false)
    , autoStart_(false)
    , showConsole_(false)
    , running_(false) {
    instance_ = this;
}

DbSyncApp::~DbSyncApp() {
    Shutdown();
    instance_ = nullptr;
}

bool DbSyncApp::Initialize(HINSTANCE hInstance, int nCmdShow, LPWSTR lpCmdLine) {
    hInstance_ = hInstance;
    
    // 解析命令行参数
    ParseCommandLine(lpCmdLine);
    
    // 初始化日志
    if (!InitializeLogging()) {
        MessageBoxW(nullptr, L"Failed to initialize logging", L"Error", MB_OK | MB_ICONERROR);
        return false;
    }
    
    LOG_INFO("DbSync v" + std::string(VERSION) + " starting...");
    LOG_INFO("Command line: " + Utils::WideToUtf8(lpCmdLine ? lpCmdLine : L""));
    
    // 初始化配置
    if (!InitializeConfiguration()) {
        LOG_ERROR("Failed to initialize configuration");
        return false;
    }
    
    // 初始化数据库和同步管理器
    if (!InitializeDatabase()) {
        LOG_ERROR("Failed to initialize database");
        return false;
    }
    
    // 初始化网络
    if (!InitializeNetwork()) {
        LOG_ERROR("Failed to initialize network");
        return false;
    }
    
    // 初始化UI
    if (!InitializeUI(nCmdShow)) {
        LOG_ERROR("Failed to initialize UI");
        return false;
    }
    
    // 设置控制台控制处理程序
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    
    // 如果配置了开机自启，设置注册表
    if (ConfigManager::GetInstance().GetSyncConfig().startWithWindows) {
        SetupAutoStart();
    }
    
    running_ = true;
    LOG_INFO("DbSync initialized successfully");
    
    // 如果配置了自动启动同步，开始同步
    if (ConfigManager::GetInstance().GetSyncConfig().autoStart) {
        LOG_INFO("Auto-starting synchronization...");
        syncManager_->StartSync();
    }
    
    return true;
}

int DbSyncApp::Run() {
    if (!running_) {
        return 1;
    }
    
    // 运行消息循环
    return mainWindow_.Run();
}

void DbSyncApp::Shutdown() {
    if (!running_) {
        return;
    }
    
    LOG_INFO("Shutting down DbSync...");
    
    // 停止同步
    if (syncManager_) {
        syncManager_->Shutdown();
        syncManager_.reset();
    }
    
    // 关闭UI
    mainWindow_.Destroy();
    
    // 保存配置
    ConfigManager::GetInstance().SaveConfig("config/dbsync.conf");
    
    running_ = false;
    LOG_INFO("DbSync shutdown complete");
    
    // 关闭日志
    // Logger::GetInstance().Shutdown();
}

bool DbSyncApp::InitializeLogging() {
    // 创建日志目录
    CreateDirectoryW(L"logs", nullptr);
    
    // 初始化日志系统
    Logger::GetInstance().Initialize("logs/dbsync.log");
    Logger::GetInstance().SetLogLevel(LogLevel::DEBUG);
    
    return true;
}

bool DbSyncApp::InitializeConfiguration() {
    // 创建配置目录
    CreateDirectoryW(L"config", nullptr);
    
    // 加载配置
    std::string configPath = "config/dbsync.conf";
    if (!ConfigManager::GetInstance().LoadConfig(configPath)) {
        // 如果配置文件不存在，创建默认配置
        ConfigManager::GetInstance().SetDefaultConfig();
        ConfigManager::GetInstance().SaveConfig(configPath);
        LOG_INFO("Created default configuration file");
    }
    
    return true;
}

bool DbSyncApp::InitializeDatabase() {
    // 创建同步管理器
    syncManager_ = std::make_unique<SyncManager>();
    
    // 初始化同步管理器
    if (!syncManager_->Initialize()) {
        LOG_ERROR("Failed to initialize sync manager");
        return false;
    }
    
    // 设置回调
    syncManager_->SetSyncStatusCallback([this](const std::string& status, int progress) {
        mainWindow_.UpdateStatus(status);
        mainWindow_.UpdateProgress(progress);
    });
    
    syncManager_->SetErrorCallback([this](const std::string& error) {
        LOG_ERROR("Sync error: " + error);
        mainWindow_.ShowError(error);
    });
    
    return true;
}

bool DbSyncApp::InitializeNetwork() {
    // 网络已在SyncManager中初始化
    return true;
}

bool DbSyncApp::InitializeUI(int nCmdShow) {
    // 初始化通用控件
    INITCOMMONCONTROLSEX iccex;
    iccex.dwSize = sizeof(iccex);
    iccex.dwICC = ICC_PROGRESS_CLASS | ICC_BAR_CLASSES;
    InitCommonControlsEx(&iccex);
    
    // 创建主窗口
    int showCmd = minimizedStart_ ? SW_HIDE : nCmdShow;
    if (!mainWindow_.Create(hInstance_, showCmd)) {
        LOG_ERROR("Failed to create main window");
        return false;
    }
    
    // 设置同步管理器
    mainWindow_.SetSyncManager(syncManager_.get());
    
    // 如果是最小化启动，隐藏到托盘
    if (minimizedStart_) {
        mainWindow_.Minimize();
    }
    
    return true;
}

void DbSyncApp::ParseCommandLine(LPWSTR lpCmdLine) {
    if (!lpCmdLine) {
        return;
    }
    
    std::wstring cmdLine = lpCmdLine;
    
    // 检查参数
    if (cmdLine.find(L"-minimized") != std::wstring::npos ||
        cmdLine.find(L"/minimized") != std::wstring::npos) {
        minimizedStart_ = true;
    }
    
    if (cmdLine.find(L"-autostart") != std::wstring::npos ||
        cmdLine.find(L"/autostart") != std::wstring::npos) {
        autoStart_ = true;
    }
    
    if (cmdLine.find(L"-console") != std::wstring::npos ||
        cmdLine.find(L"/console") != std::wstring::npos) {
        showConsole_ = true;
    }
}

bool DbSyncApp::SetupAutoStart() {
    return SystemTray::SetAutoStart(true);
}

BOOL WINAPI DbSyncApp::ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || signal == CTRL_CLOSE_EVENT) {
        if (instance_) {
            instance_->Shutdown();
        }
        return TRUE;
    }
    return FALSE;
}

} // namespace dbsync
