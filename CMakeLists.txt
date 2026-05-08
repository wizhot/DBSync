cmake_minimum_required(VERSION 3.16)
project(DbSync VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置Windows Unicode
add_definitions(-DUNICODE -D_UNICODE)

# 选项：使用嵌入式Firebird（默认开启）
option(USE_FIREBIRD_EMBEDDED "Use Firebird Embedded instead of Server" ON)

# Firebird配置
if(USE_FIREBIRD_EMBEDDED)
    # 嵌入式模式：使用 fbembed.dll
    message(STATUS "Building with Firebird Embedded support")
    set(FIREBIRD_ROOT "" CACHE PATH "Firebird Embedded directory (optional, can be empty if DLLs are in app dir)")
    
    # 如果指定了Firebird根目录，使用它
    if(FIREBIRD_ROOT AND EXISTS ${FIREBIRD_ROOT})
        include_directories(${FIREBIRD_ROOT}/include)
        link_directories(${FIREBIRD_ROOT}/lib)
        message(STATUS "Firebird Embedded headers from: ${FIREBIRD_ROOT}/include")
    else()
        # 使用系统路径或third_party目录
        if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/firebird/include)
            include_directories(${CMAKE_SOURCE_DIR}/third_party/firebird/include)
            message(STATUS "Firebird headers from: third_party/firebird/include")
        else()
            # 尝试常见安装路径
            if(EXISTS "C:/Program Files/Firebird/Firebird_4_0/include")
                include_directories("C:/Program Files/Firebird/Firebird_4_0/include")
                message(STATUS "Firebird headers from: C:/Program Files/Firebird/Firebird_4_0/include")
            endif()
        endif()
    endif()
else()
    # 服务器模式：使用 fbclient
    message(STATUS "Building with Firebird Server support")
    set(FIREBIRD_ROOT "C:/Program Files/Firebird/Firebird_4_0" CACHE PATH "Firebird installation directory")
    include_directories(${FIREBIRD_ROOT}/include)
    link_directories(${FIREBIRD_ROOT}/lib)
endif()

# LiteSQL
include_directories(${CMAKE_SOURCE_DIR}/third_party/litesql/include)
link_directories(${CMAKE_SOURCE_DIR}/third_party/litesql/lib)

# JsonCpp
include_directories(${CMAKE_SOURCE_DIR}/third_party/jsoncpp/include)

# 源文件
set(SOURCES
    src/main.cpp
    src/DbSyncApp.cpp
    src/FirebirdManager.cpp
    src/SyncManager.cpp
    src/NetworkManager.cpp
    src/ConfigManager.cpp
    src/ChangeTracker.cpp
    src/ConflictResolver.cpp
    src/SystemTray.cpp
    src/Logger.cpp
)

set(HEADERS
    src/DbSyncApp.h
    src/FirebirdManager.h
    src/SyncManager.h
    src/NetworkManager.h
    src/ConfigManager.h
    src/ChangeTracker.h
    src/ConflictResolver.h
    src/SystemTray.h
    src/Logger.h
    src/Common.h
)

# 可执行文件
add_executable(DbSync WIN32 ${SOURCES} ${HEADERS} resources/resource.rc)

# 链接库
if(USE_FIREBIRD_EMBEDDED)
    # 嵌入式模式：链接 fbembed.lib（如果存在）或 fbclient_ms.lib
    # 注意：嵌入式模式下，运行时需要 fbembed.dll 在程序目录
    if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/firebird/lib/fbembed.lib)
        target_link_libraries(DbSync fbembed.lib)
    else()
        # 使用 fbclient_ms.lib，运行时用 fbembed.dll 替换
        target_link_libraries(DbSync fbclient_ms.lib)
    endif()
else()
    # 服务器模式
    target_link_libraries(DbSync fbclient_ms.lib)
endif()

# 链接其他库
target_link_libraries(DbSync
    litesql.lib
    litesql-util.lib
    jsoncpp.lib
    ws2_32.lib
    shell32.lib
    user32.lib
    gdi32.lib
    comctl32.lib
    advapi32.lib
)

# 定义编译选项
if(USE_FIREBIRD_EMBEDDED)
    target_compile_definitions(DbSync PRIVATE FIREBIRD_EMBEDDED=1)
endif()

# 安装目标
install(TARGETS DbSync DESTINATION bin)
install(FILES config/dbsync.conf DESTINATION config)

# 打印构建信息
message(STATUS "")
message(STATUS "=== DbSync Build Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Firebird Embedded: ${USE_FIREBIRD_EMBEDDED}")
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "==================================")
message(STATUS "")
