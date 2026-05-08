# DbSync 嵌入式版本安装指南

## 概述

DbSync 现已支持 **Firebird Embedded（嵌入式）** 模式，无需安装 Firebird 服务器即可运行。这大大简化了部署流程，只需复制程序和 DLL 文件即可使用。

## 嵌入式模式 vs 服务器模式

| 特性 | 嵌入式模式 | 服务器模式 |
|------|-----------|-----------|
| 安装要求 | 无需安装服务器 | 需要安装 Firebird 服务器 |
| 部署方式 | 复制文件即可 | 需要安装程序 |
| 性能 | 单进程访问，性能更好 | 支持多进程并发访问 |
| 网络访问 | 不支持远程连接 | 支持远程连接 |
| 适用场景 | 单机应用、便携部署 | 多用户、网络应用 |

## 快速安装步骤

### 1. 准备 Firebird Embedded 文件

#### 下载 Firebird Embedded

1. 访问 Firebird 官网：https://firebirdsql.org/en/server-packages/
2. 下载 Firebird 4.0 Windows 版本（zip 压缩包）
3. 解压缩

#### 提取嵌入式文件

从解压的 Firebird 目录中提取以下文件：

**必需文件：**
```
fbembed.dll          # 核心嵌入式引擎
icuuc70.dll          # ICU 库
icuin70.dll          # ICU 库
icudt70.dll          # ICU 数据文件
libcrypto-1_1-x64.dll  # OpenSSL 加密库
libssl-1_1-x64.dll     # OpenSSL SSL库
```

**可选文件（根据需要）：**
```
firebird.conf        # 配置文件（可选）
firebird.log         # 日志文件（运行时自动创建）
intl/                # 国际化支持（可选）
udf/                 # 用户定义函数（可选）
```

### 2. 部署程序文件

#### 目录结构

```
C:\DbSync\
├── DbSync.exe              # 主程序
├── config\
│   └── dbsync.conf        # 配置文件
├── data\
│   ├── LOCAL_DB.FDB       # 本地数据库
│   └── REMOTE_DB.FDB      # 远程数据库副本
├── logs\                   # 日志目录（自动创建）
│
├── fbembed.dll            # Firebird 嵌入式引擎
├── icuuc70.dll            # ICU 库
├── icuin70.dll            # ICU 库
├── icudt70.dll            # ICU 数据
├── libcrypto-1_1-x64.dll  # OpenSSL
├── libssl-1_1-x64.dll     # OpenSSL
│
├── litesql.dll            # LiteSQL 库
└── jsoncpp.dll            # JsonCpp 库
```

### 3. 创建数据库

#### 使用 isql 工具创建

Firebird Embedded 不包含 isql 工具，需要使用完整版 Firebird 的 isql 或第三方工具（如 DBeaver、FlameRobin）。

**方法 1：使用完整版 Firebird 的 isql**
```cmd
"C:\Program Files\Firebird\Firebird_4_0\isql" -user SYSDBA -password masterkey

SQL> CREATE DATABASE 'C:\DbSync\data\LOCAL_DB.FDB' 
     USER 'SYSDBA' PASSWORD 'masterkey' 
     PAGE_SIZE 8192 
     DEFAULT CHARACTER SET UTF8;
SQL> COMMIT;
SQL> QUIT;
```

**方法 2：使用 DBeaver（推荐）**
1. 下载安装 DBeaver：https://dbeaver.io/
2. 创建新连接 → Firebird Embedded
3. 设置数据库路径：`C:\DbSync\data\LOCAL_DB.FDB`
4. 创建数据库并执行建表脚本

#### 创建示例表

```sql
-- 客户表
CREATE TABLE CUSTOMERS (
    ID INTEGER PRIMARY KEY,
    NAME VARCHAR(100) NOT NULL,
    EMAIL VARCHAR(100),
    PHONE VARCHAR(20),
    ADDRESS VARCHAR(200),
    CREATED_AT TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UPDATED_AT TIMESTAMP
);

-- 创建自增触发器
CREATE GENERATOR GEN_CUSTOMERS_ID;
SET GENERATOR GEN_CUSTOMERS_ID TO 0;

CREATE TRIGGER BI_CUSTOMERS FOR CUSTOMERS
ACTIVE BEFORE INSERT POSITION 0
AS
BEGIN
    IF (NEW.ID IS NULL) THEN
        NEW.ID = GEN_ID(GEN_CUSTOMERS_ID, 1);
    NEW.UPDATED_AT = CURRENT_TIMESTAMP;
END;

CREATE TRIGGER BU_CUSTOMERS FOR CUSTOMERS
ACTIVE BEFORE UPDATE POSITION 0
AS
BEGIN
    NEW.UPDATED_AT = CURRENT_TIMESTAMP;
END;
```

### 4. 配置 DbSync

编辑 `config\dbsync.conf`：

```ini
[node]
id = NODE_A_001

# 本地数据库（嵌入式模式）
[local_database]
database = C:/DbSync/data/LOCAL_DB.FDB
username = SYSDBA
password = masterkey
charset = UTF8
embedded = true

# 远程数据库（嵌入式模式）
# 注意：远程数据库路径是远程电脑上的路径
# 实际同步通过网络传输，不直接访问远程文件
[remote_database]
host = 192.168.1.101
database = C:/DbSync/data/REMOTE_DB.FDB
username = SYSDBA
password = masterkey
charset = UTF8
embedded = true

[network]
local_ip = 0.0.0.0
local_port = 15555
remote_ip = 192.168.1.101
remote_port = 15555

[sync]
auto_start = true
minimize_to_tray = true
sync_interval_ms = 1000
conflict_resolution_strategy = timestamp
sync_all_tables = true
```

### 5. 防火墙配置

嵌入式模式下，仍需开放网络端口用于同步通信：

```cmd
# 以管理员身份运行
netsh advfirewall firewall add rule name="DbSync Sync" dir=in action=allow protocol=tcp localport=15555
```

### 6. 启动程序

```cmd
cd C:\DbSync
DbSync.exe
```

## 两台电脑配置示例

### 电脑 A (192.168.1.100)

```ini
[node]
id = NODE_A_001

[local_database]
database = C:/DbSync/data/LOCAL_DB.FDB
embedded = true

[remote_database]
host = 192.168.1.101
database = C:/DbSync/data/REMOTE_DB.FDB
embedded = true

[network]
local_port = 15555
remote_ip = 192.168.1.101
remote_port = 15555
```

### 电脑 B (192.168.1.101)

```ini
[node]
id = NODE_B_001

[local_database]
database = C:/DbSync/data/LOCAL_DB.FDB
embedded = true

[remote_database]
host = 192.168.1.100
database = C:/DbSync/data/REMOTE_DB.FDB
embedded = true

[network]
local_port = 15555
remote_ip = 192.168.1.100
remote_port = 15555
```

## 编译说明

### 使用 CMake 编译

```bash
# 创建构建目录
mkdir build
cd build

# 配置（默认启用嵌入式模式）
cmake .. -G "Visual Studio 16 2019" -A x64

# 或明确指定嵌入式模式
cmake .. -G "Visual Studio 16 2019" -A x64 -DUSE_FIREBIRD_EMBEDDED=ON

# 编译
cmake --build . --config Release
```

### 编译选项

| 选项 | 说明 |
|------|------|
| `USE_FIREBIRD_EMBEDDED=ON` | 启用嵌入式模式（默认） |
| `USE_FIREBIRD_EMBEDDED=OFF` | 使用服务器模式 |

## 常见问题

### Q1: 找不到 fbembed.dll

**错误信息：**
```
无法启动此程序，因为计算机中丢失 fbembed.dll
```

**解决方法：**
确保 `fbembed.dll` 和相关 DLL 文件在程序目录中。

### Q2: 数据库文件锁定

**错误信息：**
```
database file is locked
```

**解决方法：**
嵌入式模式只允许单进程访问。确保没有其他程序打开数据库文件。

### Q3: 无法创建数据库

**解决方法：**
使用完整版 Firebird 的 isql 工具或第三方数据库管理工具创建数据库。

### Q4: 字符编码问题

**解决方法：**
确保配置文件中设置正确的字符集：
```ini
charset = UTF8
```

### Q5: ICU 库版本不匹配

**错误信息：**
```
icuuc70.dll not found
```

**解决方法：**
确保 ICU 库版本与 Firebird 版本匹配。Firebird 4.0 使用 ICU 70。

## 性能优化

### 1. 数据库文件位置

将数据库文件放在 SSD 上可显著提升性能。

### 2. 页面大小

创建数据库时使用较大的页面大小：
```sql
CREATE DATABASE '...' PAGE_SIZE 16384;
```

### 3. 缓存设置

在 `firebird.conf` 中设置缓存（可选）：
```
DefaultDbCachePages = 10000
```

## 备份与恢复

### 备份数据库

嵌入式模式下，只需复制 `.FDB` 文件即可备份：

```cmd
# 停止 DbSync
# 复制数据库文件
copy C:\DbSync\data\LOCAL_DB.FDB C:\Backup\LOCAL_DB_%date%.FDB
```

### 恢复数据库

```cmd
# 停止 DbSync
# 恢复数据库文件
copy C:\Backup\LOCAL_DB_2024-01-01.FDB C:\DbSync\data\LOCAL_DB.FDB
# 启动 DbSync
```

## 便携部署

嵌入式模式支持便携部署，可以将整个程序放在 U 盘中运行：

```
U:\
├── DbSync\
│   ├── DbSync.exe
│   ├── config\
│   ├── data\
│   └── *.dll
```

## 技术支持

如有问题，请提供：
1. 操作系统版本
2. 错误日志（logs/dbsync.log）
3. 配置文件内容
4. 具体错误信息截图
