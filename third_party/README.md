# 第三方依赖库

## 目录结构

```
third_party/
├── sqlite3/
│   ├── include/
│   │   ├── sqlite3.h
│   │   └── sqlite3ext.h
│   └── lib/
│       └── sqlite3.lib (或 sqlite3.obj)
├── firebird/
│   ├── include/
│   │   ├── ibase.h
│   │   ├── iberror.h
│   │   └── ...
│   └── lib/
│       └── fbembed.lib (或 fbclient_ms.lib)
└── jsoncpp/
    ├── include/
    │   └── json/
    │       ├── json.h
    │       ├── value.h
    │       └── ...
    └── lib/
        └── jsoncpp.lib
```

## 下载地址

### SQLite3
- 官网: https://www.sqlite.org/download.html
- 下载 sqlite-amalgamation-XXXXXX.zip
- 提取 sqlite3.h 和 sqlite3ext.h 到 include/
- 编译 sqlite3.c 为 sqlite3.lib

### Firebird Embedded
- 官网: https://firebirdsql.org/en/server-packages/
- 下载 Firebird 4.0+ Windows x64 ZIP
- 提取 include/ 到 firebird/include/
- 复制 fbembed.dll 到项目输出目录

### JsonCpp
- GitHub: https://github.com/open-source-parsers/jsoncpp
- 下载 Release 版本
- 提取 include/json/ 到 jsoncpp/include/json/
- 编译或使用预编译的 jsoncpp.lib

## 快速准备

运行 `fetch_deps.bat` 自动创建目录结构，然后手动下载各库文件到对应目录。
