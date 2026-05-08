#pragma once

#include "Common.h"

namespace dbsync {

// 系统托盘图标管理类
class SystemTray {
public:
    SystemTray();
    ~SystemTray();
    
    // 初始化
    bool Initialize(HINSTANCE hInstance, HWND hwnd);
    void Shutdown();
    
    // 显示/隐藏
    bool Show();
    bool Hide();
    bool IsVisible() const { return visible_; }
    
    // 更新图标和提示
    bool SetIcon(HICON hIcon);
    bool SetTooltip(const std::wstring& tooltip);
    bool ShowBalloon(const std::wstring& title, const std::wstring& message, 
                     DWORD icon = NIIF_INFO, UINT timeout = 5000);
    
    // 消息处理
    bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);
    
    // 设置回调
    using TrayCallback = std::function<void(UINT message)>;
    void SetCallback(TrayCallback callback) { callback_ = callback; }
    
    // 菜单操作
    bool ShowContextMenu();
    void SetMenu(HMENU hMenu) { hMenu_ = hMenu; }
    
    // 窗口操作
    void MinimizeToTray();
    void RestoreFromTray();
    
    // 开机自启管理
    static bool SetAutoStart(bool enable);
    static bool IsAutoStartEnabled();
    
private:
    bool CreateTrayIcon();
    bool DestroyTrayIcon();
    bool UpdateTrayIcon();
    
    static const UINT WM_TRAYICON = WM_USER + 1;
    static const UINT ID_TRAY_ICON = 1000;
    
    HINSTANCE hInstance_;
    HWND hwnd_;
    NOTIFYICONDATA nid_;
    HMENU hMenu_;
    bool visible_;
    bool initialized_;
    TrayCallback callback_;
};

// 主窗口类
class MainWindow {
public:
    MainWindow();
    ~MainWindow();
    
    // 创建和销毁
    bool Create(HINSTANCE hInstance, int nCmdShow);
    void Destroy();
    
    // 消息循环
    int Run();
    void Exit();
    
    // 窗口操作
    void Show();
    void Hide();
    void Minimize();
    void Restore();
    bool IsMinimized() const;
    
    // 设置同步管理器
    void SetSyncManager(class SyncManager* syncManager) { syncManager_ = syncManager; }
    
    // 更新UI
    void UpdateStatus(const std::string& status);
    void UpdateProgress(int progress);
    void ShowError(const std::string& error);
    
    // 获取窗口句柄
    HWND GetHandle() const { return hwnd_; }
    
private:
    // 窗口过程
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    
    // 控件创建
    bool CreateControls();
    void UpdateControlPositions();
    
    // 命令处理
    void OnCommand(int id);
    void OnTrayIconMessage(UINT message);
    void OnTimer(UINT_PTR timerId);
    
    // 菜单命令
    void OnMenuStartSync();
    void OnMenuStopSync();
    void OnMenuTriggerSync();
    void OnMenuSettings();
    void OnMenuViewLog();
    void OnMenuAbout();
    void OnMenuExit();
    
    HINSTANCE hInstance_;
    HWND hwnd_;
    HWND hwndStatus_;
    HWND hwndProgress_;
    HWND hwndLog_;
    HMENU hMenu_;
    SystemTray trayIcon_;
    class SyncManager* syncManager_;
    bool running_;
    bool minimizedToTray_;
    
    static const UINT_PTR TIMER_UPDATE_UI = 1;
    static const UINT UPDATE_INTERVAL_MS = 1000;
};

} // namespace dbsync
