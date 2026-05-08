# Firebird Embedded DLL 清单

## 必需文件

以下 DLL 文件必须与 DbSync.exe 放在同一目录下：

### Firebird Embedded 核心文件

| 文件名 | 大小（约） | 说明 |
|--------|-----------|------|
| `fbembed.dll` | ~15 MB | Firebird 嵌入式引擎核心 |

### ICU 国际化库（Firebird 4.0）

| 文件名 | 大小（约） | 说明 |
|--------|-----------|------|
| `icuuc70.dll` | ~1.5 MB | ICU 通用库 |
| `icuin70.dll` | ~300 KB | ICU 国际化库 |
| `icudt70.dll` | ~30 MB | ICU 数据文件 |

### OpenSSL 库

| 文件名 | 大小（约） | 说明 |
|--------|-----------|------|
| `libcrypto-1_1-x64.dll` | ~3 MB | OpenSSL 加密库 |
| `libssl-1_1-x64.dll` | ~500 KB | OpenSSL SSL 库 |

## 可选文件

### 配置文件

| 文件名 | 说明 |
|--------|------|
| `firebird.conf` | Firebird 配置文件 |
| `firebird.msg` | 错误消息文件 |

### 国际化支持

| 目录 | 说明 |
|------|------|
| `intl/` | 字符集转换文件 |

### 用户定义函数

| 目录 | 说明 |
|------|------|
| `udf/` | UDF 函数库 |

## 第三方库文件

### LiteSQL

| 文件名 | 说明 |
|--------|------|
| `litesql.dll` | LiteSQL 数据库库 |

### JsonCpp

| 文件名 | 说明 |
|--------|------|
| `jsoncpp.dll` | JSON 解析库 |

## 完整文件列表

```
DbSync/
├── DbSync.exe              # 主程序
│
├── fbembed.dll             # Firebird 嵌入式引擎 ★必需
├── icuuc70.dll             # ICU 通用库 ★必需
├── icuin70.dll             # ICU 国际化库 ★必需
├── icudt70.dll             # ICU 数据 ★必需
├── libcrypto-1_1-x64.dll   # OpenSSL 加密 ★必需
├── libssl-1_1-x64.dll      # OpenSSL SSL ★必需
│
├── litesql.dll             # LiteSQL 库 ★必需
├── jsoncpp.dll             # JsonCpp 库 ★必需
│
├── firebird.conf           # 配置文件（可选）
├── firebird.msg            # 消息文件（可选）
│
├── intl/                   # 国际化支持（可选）
│   ├── fbintl.conf
│   └── fbintl.dll
│
├── udf/                    # UDF 函数（可选）
│   ├── ib_udf.dll
│   └── fbudf.dll
│
├── config/
│   └── dbsync.conf         # DbSync 配置
│
├── data/
│   ├── LOCAL_DB.FDB        # 本地数据库
│   └── REMOTE_DB.FDB       # 远程数据库副本
│
└── logs/
    └── dbsync.log          # 日志文件
```

## 文件来源

### 从 Firebird 4.0 安装包提取

1. 下载 Firebird 4.0 Windows ZIP 包
   - 官网：https://firebirdsql.org/en/server-packages/
   - 文件名示例：`Firebird-4.0.4.2981-0-x64.zip`

2. 解压后从以下目录提取文件：
   ```
   firebird/
   ├── fbembed.dll
   ├── icuuc70.dll
   ├── icuin70.dll
   ├── icudt70.dll
   ├── libcrypto-1_1-x64.dll
   ├── libssl-1_1-x64.dll
   ├── firebird.conf
   ├── firebird.msg
   ├── intl/
   │   ├── fbintl.conf
   │   └── fbintl.dll
   └── udf/
       ├── ib_udf.dll
       └── fbudf.dll
   ```

### 版本兼容性

| Firebird 版本 | ICU 版本 | OpenSSL 版本 |
|--------------|----------|--------------|
| Firebird 4.0.x | ICU 70 | OpenSSL 1.1.x |
| Firebird 5.0.x | ICU 73 | OpenSSL 3.x |

**注意：** DLL 版本必须与 Firebird 版本匹配！

## 部署检查清单

- [ ] DbSync.exe
- [ ] fbembed.dll
- [ ] icuuc70.dll
- [ ] icuin70.dll
- [ ] icudt70.dll
- [ ] libcrypto-1_1-x64.dll
- [ ] libssl-1_1-x64.dll
- [ ] litesql.dll
- [ ] jsoncpp.dll
- [ ] config/dbsync.conf
- [ ] data/ 目录（用于存放数据库文件）

## 故障排除

### DLL 缺失错误

如果出现类似错误：
```
无法启动此程序，因为计算机中丢失 xxx.dll
```

解决方法：
1. 确认所有必需 DLL 都在程序目录
2. 检查 DLL 架构（x64 vs x86）是否匹配
3. 检查 DLL 版本是否兼容

### 加载失败错误

如果出现：
```
无法加载 DLL 'fbembed.dll': 找不到指定的模块
```

解决方法：
1. 确认 fbembed.dll 存在
2. 确认所有依赖 DLL 都存在
3. 尝试使用 Dependency Walker 检查依赖关系

### ICU 版本错误

如果出现：
```
icuucXX.dll not found
```

解决方法：
确保 ICU DLL 版本与 Firebird 版本匹配：
- Firebird 4.0 → ICU 70 (icuuc70.dll)
- Firebird 5.0 → ICU 73 (icuuc73.dll)

## 下载链接

### Firebird Embedded

- 官方下载：https://firebirdsql.org/en/server-packages/
- 选择 Windows x64 ZIP 包

### Visual C++ Redistributable

如果程序无法启动，可能需要安装：
- VC++ 2015-2022 Redistributable (x64)
- 下载：https://aka.ms/vs/17/release/vc_redist.x64.exe
