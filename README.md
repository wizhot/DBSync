# DbSync - 数据库实时同步工具

DbSync 是一个基于 Firebird 数据库和 LiteSQL 的 Windows 局域网数据库双向实时同步工具。

## 功能特性

- **双向实时同步**: 支持两台电脑之间的数据库双向同步（主主模式）
- **嵌入式支持**: 支持 Firebird Embedded，无需安装数据库服务器
- **自动变更追踪**: 使用数据库触发器自动捕获数据变更
- **冲突解决**: 支持多种冲突解决策略（时间戳优先、本地优先、远程优先、手动）
- **网络通信**: 基于 TCP Socket 的可靠数据传输
- **系统托盘**: 最小化到系统托盘，后台静默运行
- **开机自启**: 支持 Windows 开机自动启动
- **自动重连**: 网络断开后自动重连
- **便携部署**: 嵌入式模式支持 U 盘便携运行

## 系统要求

- Windows 7/8/10/11 (x64)
- Visual Studio 2019 或更高版本（仅编译时需要）
- CMake 3.16 或更高版本（仅编译时需要）

## 运行模式

### 嵌入式模式（推荐）

无需安装 Firebird 服务器，只需复制 DLL 文件即可运行。

**优点：**
- 部署简单，复制即用
- 性能更好（单进程访问）
- 支持便携部署

**缺点：**
- 不支持远程数据库连接
- 单进程独占访问

### 服务器模式

需要安装 Firebird 服务器。

**优点：**
- 支持多进程并发访问
- 支持远程数据库连接

**缺点：**
- 需要安装和配置服务器

## 快速开始（嵌入式模式）

### 1. 准备文件

从 [Firebird 官网](https://firebirdsql.org/en/server-packages/) 下载 Windows ZIP 包，提取以下文件：

```
DbSync/
├── DbSync.exe
├── fbembed.dll           # Firebird 嵌入式引擎
├── icuuc70.dll           # ICU 库
├── icuin70.dll           # ICU 库
├── icudt70.dll           # ICU 数据
├── libcrypto-1_1-x64.dll # OpenSSL
├── libssl-1_1-x64.dll    # OpenSSL
├── litesql.dll           # LiteSQL
├── jsoncpp.dll           # JsonCpp
├── config/
│   └── dbsync.conf
└── data/
    └── LOCAL_DB.FDB
```

### 2. 配置

编辑 `config/dbsync.conf`：

```ini
[local_database]
database = C:/DbSync/data/LOCAL_DB.FDB
embedded = true

[remote_database]
host = 192.168.1.101
database = C:/DbSync/data/REMOTE_DB.FDB
embedded = true

[network]
remote_ip = 192.168.1.101
```

### 3. 运行

```cmd
DbSync.exe
```

详细安装说明请参考：
- [嵌入式版本安装指南](INSTALL_EMBEDDED.md)
- [DLL 文件清单](DLL_MANIFEST.md)

## 编译步骤

### 1. 准备环境

```bash
# 创建第三方库目录
mkdir third_party
cd third_party
mkdir litesql
mkdir jsoncpp
mkdir firebird
```

### 2. 放置依赖库

```
third_party/
├── litesql/
│   ├── include/
│   │   └── litesql/
│   │       └── *.h
│   └── lib/
│       ├── litesql.lib
│       └── litesql-util.lib
├── jsoncpp/
│   ├── include/
│   │   └── json/
│   │       └── *.h
│   └── lib/
│       └── jsoncpp.lib
└── firebird/
    ├── include/
    │   └── ibase.h
    └── lib/
        └── fbclient_ms.lib
```

### 3. 编译

```bash
mkdir build
cd build

# 嵌入式模式（默认）
cmake .. -G "Visual Studio 16 2019" -A x64

# 服务器模式
cmake .. -G "Visual Studio 16 2019" -A x64 -DUSE_FIREBIRD_EMBEDDED=OFF

cmake --build . --config Release
```

## 配置文件说明

```ini
[node]
id = auto-generated-uuid          # 节点唯一标识

[local_database]
database = C:/DbSync/data/LOCAL_DB.FDB  # 本地数据库路径
username = SYSDBA
password = masterkey
charset = UTF8
embedded = true                   # 使用嵌入式模式

[remote_database]
host = 192.168.1.100              # 远程电脑IP
database = C:/DbSync/data/REMOTE_DB.FDB
embedded = true

[network]
local_port = 15555                # 本地监听端口
remote_ip = 192.168.1.100         # 远程电脑IP
remote_port = 15555

[sync]
auto_start = true                 # 启动时自动同步
minimize_to_tray = true           # 最小化到托盘
sync_interval_ms = 1000           # 同步间隔
conflict_resolution_strategy = timestamp  # 冲突解决策略
```

## 冲突解决策略

| 策略 | 说明 |
|------|------|
| `timestamp` | 时间戳较新的变更优先（默认） |
| `local` | 本地变更始终优先 |
| `remote` | 远程变更始终优先 |
| `manual` | 手动解决 |

## 文档

- [嵌入式版本安装指南](INSTALL_EMBEDDED.md) - 详细的嵌入式模式部署说明
- [DLL 文件清单](DLL_MANIFEST.md) - 所需 DLL 文件列表
- [服务器版本安装指南](INSTALL.md) - 服务器模式部署说明

## 常见问题

### Q: 程序无法启动，提示缺少 DLL？
A: 确保所有必需的 DLL 文件都在程序目录中。参考 [DLL_MANIFEST.md](DLL_MANIFEST.md)。

### Q: 如何创建数据库？
A: 使用 DBeaver、FlameRobin 等工具，或使用完整版 Firebird 的 isql 工具。

### Q: 两台电脑如何同步？
A: 两台电脑都运行 DbSync，通过网络端口通信。确保防火墙开放同步端口（默认 15555）。

### Q: 数据库被锁定怎么办？
A: 嵌入式模式只允许单进程访问。确保没有其他程序打开数据库文件。

## 技术架构

```
┌─────────────────────────────────────────────────────────────┐
│                        DbSync Application                    │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   SyncManager │  │ NetworkManager│  │  MainWindow   │      │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘      │
│         │                 │                 │              │
│  ┌──────▼───────┐  ┌──────▼───────┐  ┌──────▼───────┐      │
│  │FirebirdManager│  │ ChangeTracker │  │  SystemTray   │      │
│  │  (Embedded)   │  │   (LiteSQL)   │  │               │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
                    ┌──────────────────┐
                    │   TCP Network    │
                    │   (LAN Sync)     │
                    └──────────────────┘
```

## 许可证

MIT License

## 联系方式

如有问题或建议，请提交 Issue。
