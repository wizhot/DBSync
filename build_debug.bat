@echo off
chcp 65001 >nul
echo ========================================
echo   DbSync 调试版编译脚本
echo ========================================
echo.

:: 检查 CMake
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未找到 CMake，请先安装 CMake
    echo 下载地址: https://cmake.org/download/
    pause
    exit /b 1
)

:: 创建构建目录
if not exist build mkdir build
cd build

:: 生成 Visual Studio 项目
echo [1/3] 正在生成项目...
cmake .. -G "Visual Studio 17 2022" -A x64
if %errorlevel% neq 0 (
    echo [1/3] 尝试使用 Visual Studio 16 2019...
    cmake .. -G "Visual Studio 16 2019" -A x64
)
if %errorlevel% neq 0 (
    echo [错误] 项目生成失败
    cd ..
    pause
    exit /b 1
)

:: 编译（Debug 配置）
echo [2/3] 正在编译（Debug 模式）...
cmake --build . --config Debug
if %errorlevel% neq 0 (
    echo [错误] 编译失败
    cd ..
    pause
    exit /b 1
)

:: 复制输出文件
echo [3/3] 正在复制文件...
if not exist ..\output_debug mkdir ..\output_debug
copy /Y Debug\*.exe ..\output_debug\ 2>nul
copy /Y Debug\*.dll ..\output_debug\ 2>nul
copy /Y Debug\*.pdb ..\output_debug\ 2>nul
if exist ..\config xcopy /Y /E ..\config ..\output_debug\config\

cd ..
echo.
echo ========================================
echo   Debug 编译完成！
echo   输出目录: output_debug\
echo ========================================
pause
