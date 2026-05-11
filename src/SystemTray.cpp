#include "SystemTray.h"
#include "SyncManager.h"
#include "Logger.h"
#include "ConfigManager.h"
#include <shellapi.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace dbsync {

// SystemTray 实现

SystemTray::SystemTray()
    : hInstance_(nullptr)
    , hwnd_(nullptr)
    , hMenu_(nullptr)
    , visible_(false)
    , initialized_(false) {
    memset(&nid_, 0, sizeof(nid_));
}

SystemTray::~SystemTray() {
    Shutdown();
}

bool SystemTray::Initialize(HINSTANCE hInstance, HWND hwnd) {
    if (initialized_) {
        return true;
    }
    
    hInstance_ = hInstance;
    hwnd_ = hwnd;
    
    // 初始化NOTIFYICONDATA
    nid_.cbSize = sizeof(NOTIFYICONDATA);
    nid_.hWnd = hwnd_;
    nid_.uID = ID_TRAY_ICON;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    
    // 加载默认图标
    nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcscpy_s(nid_.szTip, L"DbSync - Database Synchronization");
    
    initialized_ = true;
    return true;
}

void SystemTray::Shutdown() {
    Hide();
    initialized_ = false;
}

bool SystemTray::Show() {
    if (!initialized_ || visible_) {
        return true;
    }
    
    if (!Shell_NotifyIcon(NIM_ADD, &nid_)) {
        LOG_ERROR("Failed to add tray icon");
        return false;
    }
    
    visible_ = true;
    LOG_INFO("Tray icon shown");
    return true;
}

bool SystemTray::Hide() {
    if (!initialized_ || !visible_) {
        return true;
    }
    
    if (!Shell_NotifyIcon(NIM_DELETE, &nid_)) {
        LOG_WARNING("Failed to delete tray icon");
    }
    
    visible_ = false;
    LOG_INFO("Tray icon hidden");
    return true;
}

bool SystemTray::SetIcon(HICON hIcon) {
    if (!initialized_) {
        return false;
    }
    
    nid_.hIcon = hIcon;
    nid_.uFlags |= NIF_ICON;
    
    if (visible_) {
        return Shell_NotifyIcon(NIM_MODIFY, &nid_);
    }
    
    return true;
}

bool SystemTray::SetTooltip(const std::wstring& tooltip) {
    if (!initialized_) {
        return false;
    }
    
    wcsncpy_s(nid_.szTip, tooltip.c_str(), _TRUNCATE);
    nid_.uFlags |= NIF_TIP;
    
    if (visible_) {
        return Shell_NotifyIcon(NIM_MODIFY, &nid_);
    }
    
    return true;
}

bool SystemTray::ShowBalloon(const std::wstring& title, const std::wstring& message, 
                              DWORD icon, UINT timeout) {
    if (!initialized_ || !visible_) {
        return false;
    }
    
    NOTIFYICONDATA nid = nid_;
    nid.uFlags = NIF_INFO;
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, message.c_str(), _TRUNCATE);
    nid.dwInfoFlags = icon;
    nid.uTimeout = timeout;
    
    return Shell_NotifyIcon(NIM_MODIFY, &nid);
}

bool SystemTray::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_TRAYICON && wParam == ID_TRAY_ICON) {
        UINT trayMessage = LOWORD(lParam);
        
        switch (trayMessage) {
            case WM_LBUTTONDBLCLK:
                // 双击恢复窗口
                if (callback_) {
                    callback_(WM_LBUTTONDBLCLK);
                }
                break;
                
            case WM_RBUTTONUP:
                // 右键显示菜单
                if (callback_) {
                    callback_(WM_RBUTTONUP);
                }
                break;
                
            case WM_LBUTTONUP:
                // 单击
                if (callback_) {
                    callback_(WM_LBUTTONUP);
                }
                break;
        }
        
        return true;
    }
    
    return false;
}

bool SystemTray::ShowContextMenu() {
    if (!hMenu_) {
        return false;
    }
    
    POINT pt;
    GetCursorPos(&pt);
    
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(hMenu_, TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RIGHTBUTTON,
                   pt.x, pt.y, 0, hwnd_, nullptr);
    PostMessage(hwnd_, WM_NULL, 0, 0);
    
    return true;
}

void SystemTray::MinimizeToTray() {
    ShowWindow(hwnd_, SW_HIDE);
    Show();
}

void SystemTray::RestoreFromTray() {
    Hide();
    ShowWindow(hwnd_, SW_RESTORE);
    SetForegroundWindow(hwnd_);
}

bool SystemTray::SetAutoStart(bool enable) {
    HKEY hKey;
    const wchar_t* keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* valueName = L"DbSync";
    
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_WRITE, &hKey);
    if (result != ERROR_SUCCESS) {
        LOG_ERROR("Failed to open registry key for auto-start");
        return false;
    }
    
    if (enable) {
        // 获取程序路径
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        
        // 添加 -minimized 参数
        std::wstring cmdLine = L"\"";
        cmdLine += exePath;
        cmdLine += L"\" -minimized";
        
        result = RegSetValueExW(hKey, valueName, 0, REG_SZ,
                                (BYTE*)cmdLine.c_str(),
                                (cmdLine.length() + 1) * sizeof(wchar_t));
    } else {
        result = RegDeleteValueW(hKey, valueName);
    }
    
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) {
        LOG_ERROR("Failed to set auto-start registry value");
        return false;
    }
    
    LOG_INFO(std::string("Auto-start ") + (enable ? "enabled" : "disabled"));
    return true;
}

bool SystemTray::IsAutoStartEnabled() {
    HKEY hKey;
    const wchar_t* keyPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    const wchar_t* valueName = L"DbSync";
    
    LONG result = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_READ, &hKey);
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    wchar_t value[MAX_PATH];
    DWORD valueSize = sizeof(value);
    DWORD valueType;
    
    result = RegQueryValueExW(hKey, valueName, nullptr, &valueType, (BYTE*)value, &valueSize);
    RegCloseKey(hKey);
    
    return (result == ERROR_SUCCESS);
}

// MainWindow 实现

MainWindow::MainWindow()
    : hInstance_(nullptr)
    , hwnd_(nullptr)
    , hwndStatus_(nullptr)
    , hwndProgress_(nullptr)
    , hwndLog_(nullptr)
    , hMenu_(nullptr)
    , syncManager_(nullptr)
    , running_(false)
    , minimizedToTray_(false) {
}

MainWindow::~MainWindow() {
    Destroy();
}

bool MainWindow::Create(HINSTANCE hInstance, int nCmdShow) {
    hInstance_ = hInstance;
    
    // 注册窗口类
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = WindowProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = nullptr;
    wcex.lpszClassName = L"DbSyncMainWindow";
    wcex.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);
    
    if (!RegisterClassExW(&wcex)) {
        LOG_ERROR("Failed to register window class");
        return false;
    }
    
    // 创建主窗口
    hwnd_ = CreateWindowExW(
        0,
        L"DbSyncMainWindow",
        L"DbSync - Database Synchronization",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        nullptr,
        nullptr,
        hInstance,
        this
    );
    
    if (!hwnd_) {
        LOG_ERROR("Failed to create main window");
        return false;
    }
    
    // 创建控件
    if (!CreateControls()) {
        LOG_ERROR("Failed to create controls");
        return false;
    }
    
    // 创建菜单
    hMenu_ = CreatePopupMenu();
    AppendMenuW(hMenu_, MF_STRING, 1, L"Start Sync");
    AppendMenuW(hMenu_, MF_STRING, 2, L"Stop Sync");
    AppendMenuW(hMenu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu_, MF_STRING, 3, L"Trigger Sync Now");
    AppendMenuW(hMenu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu_, MF_STRING, 4, L"Settings...");
    AppendMenuW(hMenu_, MF_STRING, 5, L"View Log");
    AppendMenuW(hMenu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu_, MF_STRING, 6, L"About");
    AppendMenuW(hMenu_, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu_, MF_STRING, 7, L"Exit");
    
    trayIcon_.SetMenu(hMenu_);
    trayIcon_.SetCallback([this](UINT msg) { OnTrayIconMessage(msg); });
    
    // 初始化系统托盘
    if (!trayIcon_.Initialize(hInstance, hwnd_)) {
        LOG_WARNING("Failed to initialize system tray");
    }
    
    // 显示窗口
    ShowWindow(hwnd_, nCmdShow);
    UpdateWindow(hwnd_);
    
    // 设置定时器更新UI
    SetTimer(hwnd_, TIMER_UPDATE_UI, UPDATE_INTERVAL_MS, nullptr);
    
    running_ = true;
    LOG_INFO("Main window created");
    return true;
}

void MainWindow::Destroy() {
    if (hwnd_) {
        KillTimer(hwnd_, TIMER_UPDATE_UI);
        trayIcon_.Shutdown();
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    running_ = false;
}

int MainWindow::Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void MainWindow::Exit() {
    running_ = false;
    PostQuitMessage(0);
}

void MainWindow::Show() {
    ShowWindow(hwnd_, SW_SHOW);
}

void MainWindow::Hide() {
    ShowWindow(hwnd_, SW_HIDE);
}

void MainWindow::Minimize() {
    if (ConfigManager::GetInstance().GetSyncConfig().minimizeToTray) {
        trayIcon_.MinimizeToTray();
        minimizedToTray_ = true;
    } else {
        ShowWindow(hwnd_, SW_MINIMIZE);
    }
}

void MainWindow::Restore() {
    if (minimizedToTray_) {
        trayIcon_.RestoreFromTray();
        minimizedToTray_ = false;
    } else {
        ShowWindow(hwnd_, SW_RESTORE);
    }
    SetForegroundWindow(hwnd_);
}

bool MainWindow::IsMinimized() const {
    return IsIconic(hwnd_) || minimizedToTray_;
}

void MainWindow::UpdateStatus(const std::string& status) {
    if (hwndStatus_) {
        SetWindowTextW(hwndStatus_, Utils::Utf8ToWide(status).c_str());
    }
    
    // 更新托盘提示
    std::wstring tooltip = L"DbSync - " + Utils::Utf8ToWide(status);
    trayIcon_.SetTooltip(tooltip);
}

void MainWindow::UpdateProgress(int progress) {
    if (hwndProgress_) {
        SendMessage(hwndProgress_, PBM_SETPOS, progress, 0);
    }
}

void MainWindow::ShowError(const std::string& error) {
    // 添加到日志控件
    if (hwndLog_) {
        std::wstring wError = Utils::Utf8ToWide(error + "\r\n");
        int len = GetWindowTextLengthW(hwndLog_);
        SendMessageW(hwndLog_, EM_SETSEL, len, len);
        SendMessageW(hwndLog_, EM_REPLACESEL, 0, (LPARAM)wError.c_str());
    }
    
    // 显示气球提示
    trayIcon_.ShowBalloon(L"DbSync Error", Utils::Utf8ToWide(error), NIIF_ERROR);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    MainWindow* window = nullptr;
    
    if (message == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (window) {
        return window->HandleMessage(message, wParam, lParam);
    }
    
    return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    // 先检查是否是托盘消息
    if (trayIcon_.ProcessMessage(message, wParam, lParam)) {
        return 0;
    }
    
    switch (message) {
        case WM_CREATE:
            return 0;
            
        case WM_SIZE:
            UpdateControlPositions();
            return 0;
            
        case WM_COMMAND:
            OnCommand(LOWORD(wParam));
            return 0;
            
        case WM_TIMER:
            OnTimer(wParam);
            return 0;
            
        case WM_SYSCOMMAND:
            if (wParam == SC_MINIMIZE) {
                Minimize();
                return 0;
            }
            break;
            
        case WM_CLOSE:
            if (ConfigManager::GetInstance().GetSyncConfig().minimizeToTray) {
                Minimize();
                return 0;
            }
            break;
            
        case WM_DESTROY:
            Exit();
            return 0;
    }
    
    return DefWindowProc(hwnd_, message, wParam, lParam);
}

bool MainWindow::CreateControls() {
    // 创建状态标签
    hwndStatus_ = CreateWindowW(L"STATIC", L"Status: Idle",
                                WS_VISIBLE | WS_CHILD | SS_LEFT,
                                10, 10, 760, 20,
                                hwnd_, nullptr, hInstance_, nullptr);
    
    // 创建进度条
    hwndProgress_ = CreateWindowExW(0, PROGRESS_CLASSW, nullptr,
                                    WS_VISIBLE | WS_CHILD | PBS_SMOOTH,
                                    10, 40, 760, 20,
                                    hwnd_, nullptr, hInstance_, nullptr);
    SendMessage(hwndProgress_, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    
    // 创建日志编辑框
    hwndLog_ = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr,
                               WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL |
                               ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                               10, 70, 760, 480,
                               hwnd_, nullptr, hInstance_, nullptr);
    
    // 设置等宽字体
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    SendMessage(hwndLog_, WM_SETFONT, (WPARAM)hFont, TRUE);
    
    return hwndStatus_ && hwndProgress_ && hwndLog_;
}

void MainWindow::UpdateControlPositions() {
    RECT rect;
    GetClientRect(hwnd_, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;
    
    // 调整控件大小和位置
    SetWindowPos(hwndStatus_, nullptr, 10, 10, width - 20, 20, SWP_NOZORDER);
    SetWindowPos(hwndProgress_, nullptr, 10, 40, width - 20, 20, SWP_NOZORDER);
    SetWindowPos(hwndLog_, nullptr, 10, 70, width - 20, height - 80, SWP_NOZORDER);
}

void MainWindow::OnCommand(int id) {
    switch (id) {
        case 1: OnMenuStartSync(); break;
        case 2: OnMenuStopSync(); break;
        case 3: OnMenuTriggerSync(); break;
        case 4: OnMenuSettings(); break;
        case 5: OnMenuViewLog(); break;
        case 6: OnMenuAbout(); break;
        case 7: OnMenuExit(); break;
    }
}

void MainWindow::OnTrayIconMessage(UINT message) {
    switch (message) {
        case WM_LBUTTONDBLCLK:
            Restore();
            break;
        case WM_RBUTTONUP:
            trayIcon_.ShowContextMenu();
            break;
    }
}

void MainWindow::OnTimer(UINT_PTR timerId) {
    if (timerId == TIMER_UPDATE_UI && syncManager_) {
        // 更新状态
        std::string status = syncManager_->GetStatus();
        UpdateStatus(status);
        
        // 更新进度
        int pending = syncManager_->GetPendingChangesCount();
        if (pending > 0) {
            UpdateProgress(50);
        } else {
            UpdateProgress(100);
        }
    }
}

void MainWindow::OnMenuStartSync() {
    if (syncManager_) {
        if (syncManager_->StartSync()) {
            UpdateStatus("Synchronization started");
            trayIcon_.ShowBalloon(L"DbSync", L"Synchronization started", NIIF_INFO);
        } else {
            ShowError("Failed to start synchronization");
        }
    }
}

void MainWindow::OnMenuStopSync() {
    if (syncManager_) {
        syncManager_->StopSync();
        UpdateStatus("Synchronization stopped");
    }
}

void MainWindow::OnMenuTriggerSync() {
    if (syncManager_) {
        if (syncManager_->TriggerSync()) {
            trayIcon_.ShowBalloon(L"DbSync", L"Manual sync triggered", NIIF_INFO);
        }
    }
}

void MainWindow::OnMenuSettings() {
    // TODO: 打开设置对话框
    MessageBoxW(hwnd_, L"Settings dialog not implemented yet.", L"Settings", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::OnMenuViewLog() {
    // 打开日志文件
    std::wstring logPath = L"logs\\dbsync.log";
    ShellExecuteW(nullptr, L"open", logPath.c_str(), nullptr, nullptr, SW_SHOW);
}

void MainWindow::OnMenuAbout() {
    std::wstring aboutText = L"DbSync v" + Utils::Utf8ToWide(VERSION) + 
        L"\n\nDatabase Synchronization Tool\n\n"
        L"Supports Firebird database synchronization\n"
        L"between two Windows computers in LAN.";
    MessageBoxW(hwnd_, aboutText.c_str(), L"About DbSync", MB_OK | MB_ICONINFORMATION);
}

void MainWindow::OnMenuExit() {
    if (syncManager_) {
        syncManager_->StopSync();
    }
    Destroy();
}

} // namespace dbsync
