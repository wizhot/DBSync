# DbSync 安装指南

## 快速安装步骤

### 1. 在两台电脑上安装 Firebird 数据库

#### 下载 Firebird
- 访问: https://firebirdsql.org/en/server-packages/
- 下载 Firebird 4.0 Windows 安装包

#### 安装步骤
1. 运行安装程序
2. 选择 "SuperServer" 模式
3. 设置 SYSDBA 密码（默认: masterkey）
4. 完成安装

#### 验证安装
```cmd
# 检查服务是否运行
sc query FirebirdServerDefaultInstance

# 使用 isql 连接
"C:\Program Files\Firebird\Firebird_4_0\isql" -user SYSDBA -password masterkey localhost:3050
```

### 2. 创建数据库

#### 电脑 A
```sql
CREATE DATABASE 'C:\Databases\LOCAL_DB.FDB'
USER 'SYSDBA' PASSWORD 'masterkey'
PAGE_SIZE 8192
DEFAULT CHARACTER SET UTF8;

-- 创建示例表
CREATE TABLE CUSTOMERS (
    ID INTEGER PRIMARY KEY,
    NAME VARCHAR(100) NOT NULL,
    EMAIL VARCHAR(100),
    PHONE VARCHAR(20),
    ADDRESS VARCHAR(200),
    CREATED_AT TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UPDATED_AT TIMESTAMP
);

-- 创建生成器用于自增ID
CREATE GENERATOR GEN_CUSTOMERS_ID;
SET GENERATOR GEN_CUSTOMERS_ID TO 0;

-- 创建触发器实现自增
CREATE TRIGGER BI_CUSTOMERS FOR CUSTOMERS
ACTIVE BEFORE INSERT POSITION 0
AS
BEGIN
    IF (NEW.ID IS NULL) THEN
        NEW.ID = GEN_ID(GEN_CUSTOMERS_ID, 1);
    NEW.UPDATED_AT = CURRENT_TIMESTAMP;
END;

-- 创建更新触发器
CREATE TRIGGER BU_CUSTOMERS FOR CUSTOMERS
ACTIVE BEFORE UPDATE POSITION 0
AS
BEGIN
    NEW.UPDATED_AT = CURRENT_TIMESTAMP;
END;
```

#### 电脑 B
```sql
CREATE DATABASE 'C:\Databases\REMOTE_DB.FDB'
USER 'SYSDBA' PASSWORD 'masterkey'
PAGE_SIZE 8192
DEFAULT CHARACTER SET UTF8;

-- 创建相同的表结构
CREATE TABLE CUSTOMERS (
    ID INTEGER PRIMARY KEY,
    NAME VARCHAR(100) NOT NULL,
    EMAIL VARCHAR(100),
    PHONE VARCHAR(20),
    ADDRESS VARCHAR(200),
    CREATED_AT TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UPDATED_AT TIMESTAMP
);

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

### 3. 配置防火墙

#### 在电脑 A 和电脑 B 上都执行：

**方法 1: 使用命令行**
```cmd
# 以管理员身份运行 CMD

# 开放 Firebird 端口
netsh advfirewall firewall add rule name="Firebird Server" dir=in action=allow protocol=tcp localport=3050

# 开放 DbSync 端口
netsh advfirewall firewall add rule name="DbSync Sync" dir=in action=allow protocol=tcp localport=15555

# 验证规则
netsh advfirewall firewall show rule name="Firebird Server"
netsh advfirewall firewall show rule name="DbSync Sync"
```

**方法 2: 使用 Windows 防火墙界面**
1. 打开 "Windows Defender 防火墙"
2. 点击 "高级设置"
3. 右键 "入站规则" → "新建规则"
4. 选择 "端口" → "TCP"
5. 输入端口: 3050, 15555
6. 选择 "允许连接"
7. 应用范围: 域、专用、公用
8. 名称: Firebird Server / DbSync Sync

### 4. 部署 DbSync 程序

#### 准备程序文件

**电脑 A:**
```
C:\DbSync\
├── DbSync.exe
├── config\
│   └── dbsync.conf
├── logs\
└── dbsync_tracker.db
```

**电脑 B:**
```
C:\DbSync\
├── DbSync.exe
├── config\
│   └── dbsync.conf
├── logs\
└── dbsync_tracker.db
```

#### 配置电脑 A

编辑 `C:\DbSync\config\dbsync.conf`:

```ini
[node]
id = NODE_A_001

[local_database]
host = localhost
port = 3050
database = C:\Databases\LOCAL_DB.FDB
username = SYSDBA
password = masterkey
charset = UTF8

[remote_database]
host = 192.168.1.101
port = 3050
database = C:\Databases\REMOTE_DB.FDB
username = SYSDBA
password = masterkey
charset = UTF8

[network]
local_ip = 0.0.0.0
local_port = 15555
remote_ip = 192.168.1.101
remote_port = 15555
connection_timeout_ms = 5000
retry_interval_ms = 3000

[sync]
auto_start = true
minimize_to_tray = true
start_with_windows = true
sync_interval_ms = 1000
max_retries = 3
resolve_conflicts_automatically = true
conflict_resolution_strategy = timestamp
sync_all_tables = true
sync_tables = CUSTOMERS
```

#### 配置电脑 B

编辑 `C:\DbSync\config\dbsync.conf`:

```ini
[node]
id = NODE_B_001

[local_database]
host = localhost
port = 3050
database = C:\Databases\REMOTE_DB.FDB
username = SYSDBA
password = masterkey
charset = UTF8

[remote_database]
host = 192.168.1.100
port = 3050
database = C:\Databases\LOCAL_DB.FDB
username = SYSDBA
password = masterkey
charset = UTF8

[network]
local_ip = 0.0.0.0
local_port = 15555
remote_ip = 192.168.1.100
remote_port = 15555
connection_timeout_ms = 5000
retry_interval_ms = 3000

[sync]
auto_start = true
minimize_to_tray = true
start_with_windows = true
sync_interval_ms = 1000
max_retries = 3
resolve_conflicts_automatically = true
conflict_resolution_strategy = timestamp
sync_all_tables = true
sync_tables = CUSTOMERS
```

### 5. 启动程序

#### 电脑 A
```cmd
cd C:\DbSync
DbSync.exe
```

#### 电脑 B
```cmd
cd C:\DbSync
DbSync.exe
```

### 6. 验证同步

#### 在电脑 A 上插入数据：
```sql
INSERT INTO CUSTOMERS (NAME, EMAIL, PHONE) 
VALUES ('张三', 'zhangsan@example.com', '13800138001');

COMMIT;
```

#### 在电脑 B 上查询：
```sql
SELECT * FROM CUSTOMERS;
```

如果看到刚才插入的数据，说明同步成功！

## 常见问题

### Q1: 程序无法启动
**A:** 
1. 检查是否已安装 Visual C++ Redistributable
2. 检查配置文件路径是否正确
3. 查看 logs/dbsync.log 获取错误信息

### Q2: 无法连接到数据库
**A:**
1. 检查 Firebird 服务是否运行: `services.msc`
2. 检查数据库文件路径是否正确
3. 检查用户名密码是否正确
4. 尝试使用 isql 直接连接测试

### Q3: 网络连接失败
**A:**
1. 检查两台电脑是否能互相 ping 通
2. 检查防火墙设置
3. 检查 IP 地址是否正确
4. 检查端口是否被占用: `netstat -an | findstr 15555`

### Q4: 数据不同步
**A:**
1. 检查表是否有主键
2. 检查同步配置中的表名是否正确
3. 查看日志文件中的错误信息
4. 尝试手动触发同步

### Q5: 冲突频繁发生
**A:**
1. 调整冲突解决策略
2. 确保两台电脑时间同步
3. 减少同时修改相同记录的情况

## 时间同步设置

确保两台电脑时间一致：

```cmd
# 以管理员身份运行
# 配置 Windows 时间服务
w32tm /config /manualpeerlist:"time.windows.com" /syncfromflags:manual /reliable:yes /update

# 重启时间服务
net stop w32time
net start w32time

# 强制同步
w32tm /resync
```

## 性能优化

### 1. 调整同步间隔
如果数据变更不频繁，可以增加同步间隔：
```ini
[sync]
sync_interval_ms = 5000  # 改为5秒
```

### 2. 只同步需要的表
```ini
[sync]
sync_all_tables = false
sync_tables = CUSTOMERS,ORDERS,PRODUCTS
```

### 3. 数据库优化
```sql
-- 为同步表添加索引
CREATE INDEX IDX_CUSTOMERS_UPDATED ON CUSTOMERS(UPDATED_AT);

-- 定期清理同步日志
DELETE FROM DBSYNC_CHANGE_LOG WHERE TIMESTAMP < CURRENT_TIMESTAMP - 30;
```

## 卸载

### 1. 停止程序
- 右键系统托盘图标 → Exit
- 或任务管理器结束 DbSync.exe

### 2. 删除文件
```cmd
rmdir /s /q C:\DbSync
```

### 3. 删除防火墙规则
```cmd
netsh advfirewall firewall delete rule name="Firebird Server"
netsh advfirewall firewall delete rule name="DbSync Sync"
```

### 4. 删除开机启动（如果设置了）
```cmd
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v DbSync /f
```

## 技术支持

如有问题，请提供以下信息：
1. 操作系统版本
2. Firebird 版本
3. 错误日志 (logs/dbsync.log)
4. 配置文件 (config/dbsync.conf)
5. 网络环境描述
