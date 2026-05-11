cmake_minimum_required(VERSION 3.16)
project(DbSync VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Unicode 支持
add_definitions(-DUNICODE -D_UNICODE)

# 源文件
set(SOURCES
    src/main.cpp
    src/DbSyncApp.cpp
    src/FirebirdManager.cpp
    src/SqliteManager.cpp
    src/SyncManager.cpp
    src/NetworkManager.cpp
    src/ConfigManager.cpp
    src/ChangeTracker.cpp
    src/ConflictResolver.cpp
    src/MappingManager.cpp
    src/SystemTray.cpp
    src/Logger.cpp
)

set(HEADERS
    src/DbSyncApp.h
    src/FirebirdManager.h
    src/SqliteManager.h
    src/SyncManager.h
    src/NetworkManager.h
    src/ConfigManager.h
    src/ChangeTracker.h
    src/ConflictResolver.h
    src/MappingManager.h
    src/SystemTray.h
    src/Logger.h
    src/Common.h
)

# 头文件路径
include_directories(${CMAKE_SOURCE_DIR}/src)

# 第三方库路径
if(EXISTS /workspace/cross_deps)
    include_directories(/workspace/cross_deps/sqlite3/include)
    include_directories(/workspace/cross_deps/jsoncpp/include)
    include_directories(/workspace/cross_deps/firebird/include)
    link_directories(/workspace/cross_deps/sqlite3/lib)
    link_directories(/workspace/cross_deps/jsoncpp/lib)
    link_directories(/workspace/cross_deps/firebird/lib)
endif()

# 创建可执行文件
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(${PROJECT_NAME}
    sqlite3
    jsoncpp
    fbclient
    ws2_32
    shell32
    user32
    gdi32
    comctl32
    advapi32
    shlwapi
    winmm
    imm32
    ole32
    oleaut32
    uuid
)

# 安装规则
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY config DESTINATION .)

# 打印信息
message(STATUS "")
message(STATUS "=== DbSync Build Configuration ===")
message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
message(STATUS "CXX Compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "====================================")
message(STATUS "")
