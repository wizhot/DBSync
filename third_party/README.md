# Third Party Libraries

This directory contains third-party libraries required to build DbSync.

## Required Libraries

### 1. Firebird Embedded

**Download:** https://firebirdsql.org/en/server-packages/

**Directory Structure:**
```
firebird/
├── include/
│   ├── ibase.h
│   ├── ib_util.h
│   └── ...
└── lib/
    └── fbclient_ms.lib
```

**Build Instructions:**
1. Download Firebird 4.0 Windows ZIP package
2. Extract header files from `include/` directory
3. Extract `fbclient_ms.lib` from `lib/` directory
4. For embedded mode, also extract DLL files:
   - `fbembed.dll`
   - `icuuc70.dll`, `icuin70.dll`, `icudt70.dll`
   - `libcrypto-1_1-x64.dll`, `libssl-1_1-x64.dll`

**Note:** For embedded mode, you need to copy the DLL files to the application directory.

### 2. LiteSQL

**Download:** https://github.com/litesql/litesql

**Directory Structure:**
```
litesql/
├── include/
│   └── litesql/
│       ├── litesql.hpp
│       └── ...
└── lib/
    ├── litesql.lib
    └── litesql-util.lib
```

**Build Instructions:**
1. Clone LiteSQL repository
2. Build using CMake
3. Copy headers to `include/litesql/`
4. Copy libraries to `lib/`

### 3. JsonCpp

**Download:** https://github.com/open-source-parsers/jsoncpp

**Directory Structure:**
```
jsoncpp/
├── include/
│   └── json/
│       ├── json.h
│       └── ...
└── lib/
    └── jsoncpp.lib
```

**Build Instructions:**
1. Clone JsonCpp repository
2. Build using CMake or Visual Studio
3. Copy headers to `include/json/`
4. Copy library to `lib/`

## Firebird Embedded DLL Files

For embedded mode, the following DLL files are required at runtime:

| File | Description |
|------|-------------|
| `fbembed.dll` | Firebird embedded engine |
| `icuuc70.dll` | ICU common library |
| `icuin70.dll` | ICU internationalization |
| `icudt70.dll` | ICU data file |
| `libcrypto-1_1-x64.dll` | OpenSSL crypto |
| `libssl-1_1-x64.dll` | OpenSSL SSL |

**Note:** ICU version must match Firebird version:
- Firebird 4.0.x → ICU 70
- Firebird 5.0.x → ICU 73

## Pre-built Binaries

If you have pre-built binaries, place them in the appropriate subdirectories following the structure above.

## License Notes

- **Firebird**: IPL (InterBase Public License) and IDPL (Initial Developer's Public License)
- **LiteSQL**: MIT License
- **JsonCpp**: MIT License

Please review the licenses of each library before distribution.

## Quick Setup Script

```batch
@echo off
REM Setup third-party libraries for DbSync

REM Create directories
mkdir firebird\include firebird\lib
mkdir litesql\include litesql\lib
mkdir jsoncpp\include jsoncpp\lib

echo Please copy the library files to the appropriate directories:
echo - Firebird headers to firebird\include\
echo - Firebird lib to firebird\lib\
echo - LiteSQL headers to litesql\include\
echo - LiteSQL libs to litesql\lib\
echo - JsonCpp headers to jsoncpp\include\
echo - JsonCpp lib to jsoncpp\lib\
```
