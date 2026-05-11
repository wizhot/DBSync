@echo off
chcp 65001 >nul
echo ========================================
echo   DbSync 依赖库下载脚本
echo ========================================
echo.

set DEPS_DIR=%~dp0third_party

:: 创建目录结构
echo [1/4] 创建目录结构...
if not exist "%DEPS_DIR%\sqlite3\include" mkdir "%DEPS_DIR%\sqlite3\include"
if not exist "%DEPS_DIR%\sqlite3\lib" mkdir "%DEPS_DIR%\sqlite3\lib"
if not exist "%DEPS_DIR%\firebird\include" mkdir "%DEPS_DIR%\firebird\include"
if not exist "%DEPS_DIR%\firebird\lib" mkdir "%DEPS_DIR%\firebird\lib"
if not exist "%DEPS_DIR%\jsoncpp\include" mkdir "%DEPS_DIR%\jsoncpp\include\json"
if not exist "%DEPS_DIR%\jsoncpp\lib" mkdir "%DEPS_DIR%\jsoncpp\lib"

:: 下载 SQLite3
echo [2/4] 下载 SQLite3 源码...
if not exist "%DEPS_DIR%\sqlite3\include\sqlite3.h" (
    echo 正在下载 sqlite3.h ...
    curl -L -o "%DEPS_DIR%\sqlite3\include\sqlite3.h" https://www.sqlite.org/2024/sqlite-amalgamation-3450000.zip
    echo SQLite3 头文件需要从源码包中提取
    echo 请手动下载: https://www.sqlite.org/download.html
    echo   1. 下载 sqlite-amalgamation-XXXXXX.zip
    echo   2. 解压后将 sqlite3.h sqlite3ext.h 复制到 third_party\sqlite3\include\
    echo   3. 将 sqlite3.c 编译为 sqlite3.lib 放入 third_party\sqlite3\lib\
) else (
    echo SQLite3 已存在，跳过
)

:: 下载 Firebird Embedded
echo [3/4] Firebird Embedded 文件...
if not exist "%DEPS_DIR%\firebird\include\ibase.h" (
    echo 正在准备 Firebird Embedded...
    echo 请手动下载 Firebird 4.0 或 5.0: https://firebirdsql.org/en/server-packages/
    echo   1. 下载 Windows x64 ZIP 包
    echo   2. 从中提取以下文件:
    echo      include\  → third_party\firebird\include\
    echo      fbembed.dll → 项目根目录
    echo      icu*.dll → 项目根目录
    echo      libcrypto-*.dll → 项目根目录
    echo      libssl-*.dll → 项目根目录
) else (
    echo Firebird 已存在，跳过
)

:: 下载 JsonCpp
echo [4/4] JsonCpp...
if not exist "%DEPS_DIR%\jsoncpp\include\json\json.h" (
    echo 请手动下载 JsonCpp: https://github.com/open-source-parsers/jsoncpp
    echo   1. 下载最新 Release
    echo   2. 将 include\json\ 复制到 third_party\jsoncpp\include\json\
    echo   3. 编译或使用预编译的 jsoncpp.lib 放入 third_party\jsoncpp\lib\
) else (
    echo JsonCpp 已存在，跳过
)

echo.
echo ========================================
echo   依赖库准备完成
echo ========================================
echo.
echo 下一步: 运行 build.bat 编译项目
echo.
pause
