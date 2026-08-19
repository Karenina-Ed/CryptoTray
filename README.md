# CryptoTray

CryptoTray is a lightweight Windows system-tray application built with C++17, Qt 6 Widgets, and CMake.

## Current scope

Phase 1 provides the application skeleton and a system tray icon with **Show Market** and **Exit** menu entries. Market data and the price popup intentionally belong to later phases.

## Build

From a Visual Studio developer command prompt:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.3.0\msvc2019_64
cmake --build build --config Debug
```

