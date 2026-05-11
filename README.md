# DbSync - SQLite + Firebird 异构数据库同步工具

## 项目简介

DbSync 是一个用于 **SQLite** 和 **Firebird** 异构数据库之间实时双向同步的桌面工具。
主要场景为将管家婆系统的 SQLite 数据库 (grasp.db) 与 Firebird 数据库 (SALES.FDB) 进行数据同步。

## 核心特性

- **异构数据库同步**: 支持 SQLite (grasp.db) 与 Firebird (SALES.FDB) 之间的双向数据同步
- **映射驱动同步**: 按映射配置文件 (config/mapping.json) 只同步有映射的表和字段
- **自动识别数据库类型**: 根据文件扩展名 (.db=SQLite, .fdb=Firebird) 或配置文件中的 db_type 字段自动识别
- **字段映射与值转换**: 支持字段名映射和值转换规则（如 deleted <-> is_active 的 0/1 反转）
- **变更追踪**: 基于触发器和日志表实现数据变更的自动检测
- **冲突解决**: 支持时间戳策略、源优先、目标优先等多种冲突解决策略
- **网络通信**: 支持多节点间的同步数据推送和通知
- **系统托盘**: 最小化到系统托盘，后台运行

## 工作目录

```
C:\Users\wizhot\Desktop\GWweb\DBSync
```

## 项目结构

```
DbSync/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目说明
├── config/
│   ├── dbsync.conf             # 主配置文件（INI 格式）
│   └── mapping.json            # 表字段映射配置（JSON 格式）
├── src/
│   ├── main.cpp                # 程序入口
│   ├── DbSyncApp.h/cpp         # 应用程序主类
│   ├── Common.h                # 公共数据结构和常量
│   ├── ConfigManager.h/cpp     # 配置管理器
│   ├── SqliteManager.h/cpp     # SQLite 数据库管理器
│   ├── FirebirdManager.h/cpp   # Firebird 数据库管理器
│   ├── MappingManager.h/cpp    # 映射管理器
│   ├── SyncManager.h/cpp       # 同步管理器
│   ├── ChangeTracker.h/cpp     # 变更追踪器
│   ├── ConflictResolver.h/cpp  # 冲突解决器
│   ├── NetworkManager.h/cpp    # 网络管理器
│   ├── SystemTray.h/cpp        # 系统托盘
│   └── Logger.h/cpp            # 日志管理器
├── data/
│   ├── grasp.db                # 管家婆 SQLite 数据库
│   └── SALES.FDB               # Firebird 销售数据库
└── third_party/
    ├── sqlite3/                # SQLite3 库
    ├── firebird/               # Firebird 客户端库
    └── jsoncpp/                # JSON 解析库
```

## 构建说明

### 依赖

- C++17 编译器 (MSVC 2019+ / GCC 9+)
- CMake 3.14+
- SQLite3 (支持加密扩展)
- Firebird Client (fbclient)
- jsoncpp

### 构建步骤

```bash
# 配置第三方库路径
cmake -B build -DSQLITE3_ROOT=C:/path/to/sqlite3 -DFIREBIRD_ROOT=C:/path/to/firebird

# 构建
cmake --build build --config Release
```

## 配置说明

### 主配置文件 (config/dbsync.conf)

```ini
[node]
id = auto-generated-uuid

[local_database]
database = data/grasp.db
db_type = sqlite
encryption_key =
charset = UTF8

[remote_database]
host = 192.168.1.100
database = data/SALES.FDB
db_type = firebird
username = SYSDBA
password = masterkey
charset = UTF8
embedded = true

[sync]
auto_start = true
sync_interval_ms = 1000
mapping_file = config/mapping.json
```

### 映射配置文件 (config/mapping.json)

映射文件定义了 SQLite 和 Firebird 之间的表和字段对应关系，只有配置了映射的表和字段才会被同步。

```json
{
  "table_mappings": [
    {
      "source_table": "Ptype",
      "target_table": "products",
      "source_db": "sqlite",
      "target_db": "firebird",
      "primary_key": { "source": "ptypeid", "target": "id" },
      "field_mappings": [
        { "source": "ptypeid", "target": "id" },
        { "source": "pfullname", "target": "name" }
      ],
      "value_transforms": [
        { "source": "deleted", "target": "is_active", "transform": "invert" }
      ]
    }
  ]
}
```

## 同步流程

1. **初始化**: 加载配置 -> 连接 SQLite 和 Firebird -> 加载映射规则 -> 设置变更追踪
2. **变更检测**: 通过触发器自动记录数据变更到日志表
3. **数据同步**: 定时扫描日志表，按映射规则转换数据，写入目标数据库
4. **冲突处理**: 检测到冲突时按配置的策略自动解决
5. **标记完成**: 同步成功后标记变更记录为已同步

## 数据库类型识别

系统通过以下方式自动识别数据库类型：

1. 配置文件中的 `db_type` 字段（优先级最高）
2. 数据库文件扩展名：`.db` = SQLite，`.fdb` = Firebird
3. `db_type` 设为 `"auto"` 时自动根据扩展名判断
