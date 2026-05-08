#include "DbSyncApp.h"
#include <windows.h>

using namespace dbsync;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                    LPWSTR lpCmdLine, int nCmdShow) {
    // 防止多个实例运行
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"DbSync_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // 程序已在运行，激活已有窗口
        HWND hwnd = FindWindowW(L"DbSyncMainWindow", nullptr);
        if (hwnd) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        CloseHandle(hMutex);
        return 0;
    }
    
    // 创建应用程序实例
    DbSyncApp app;
    
    // 初始化应用程序
    if (!app.Initialize(hInstance, nCmdShow, lpCmdLine)) {
        CloseHandle(hMutex);
        return 1;
    }
    
    // 运行应用程序
    int result = app.Run();
    
    // 关闭应用程序
    app.Shutdown();
    
    // 释放互斥量
    CloseHandle(hMutex);
    
    return result;
}
