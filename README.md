# CryptoTray

CryptoTray 是一款轻量级 Windows 系统托盘行情应用，使用 C++17、Qt 6 Widgets 和 CMake 构建。

## 功能

- 提供直接嵌入 Windows 任务栏的紧凑单行行情组件。
- 通过 Binance USD-M WebSocket 实时获取 BTCUSDT 和 ETHUSDT U 本位永续合约的 UTC 日线行情。
- 将紧凑的单行行情窗口嵌入 Windows 任务栏，横向显示 BTC、ETH 价格和 UTC 涨跌幅。
- 使用 OKX 风格的透明背景、高对比文字、清晰数字字体和克制的涨跌配色。
- 任务栏组件的完整矩形均可单击；左键单击后显示 OKX 风格详情卡片，包含永续合约价格、UTC 自然日涨跌幅、开盘价、最高价、最低价、成交量和更新时间，点击卡片外部自动关闭。
- Debug 和 Release 构建都使用 Windows GUI 子系统，启动时不显示控制台窗口。
- 显示连接状态，并在断线后使用指数退避策略自动重连。
- Explorer 重启或任务栏挂载失败时自动重试。
- 提供行情消息解析单元测试，以及可选的真实网络冒烟测试。

## 构建

在 Visual Studio 开发者命令提示符中执行：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt\6.3.0\msvc2019_64
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`CryptoTrayMarketSmoke` 目标需要访问 Binance，因此不会自动注册到 CTest。需要验证真实网络连接时，请手动运行该目标。

`CryptoTrayTaskbarSmoke` 会临时嵌入真实任务栏，并在 BTC、ETH 行情都显示成功后退出。它用于手动验证完整 GUI、任务栏挂载和网络链路，同样不会注册到 CTest。
