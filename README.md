# CryptoTray

CryptoTray 是一款轻量级 Windows 系统托盘行情应用，使用 C++17、Qt 6 Widgets 和 CMake 构建。

## 功能

- 提供直接嵌入 Windows 任务栏的紧凑单行行情组件。
- 通过 Binance USD-M WebSocket 实时获取 BTCUSDT 和 ETHUSDT U 本位永续合约的 UTC 日线行情。
- 将紧凑的单行行情窗口嵌入 Windows 任务栏，横向显示 BTC、ETH 价格和 UTC 涨跌幅。
- 使用 OKX 风格的透明背景、高对比文字、清晰数字字体和克制的涨跌配色。
- 任务栏组件的完整矩形均可单击；左键单击后显示 OKX 风格详情卡片，包含永续合约价格、UTC 自然日涨跌幅、开盘价、最高价、最低价、成交量和更新时间，点击卡片外部自动关闭。
- 详情卡片采用紧凑的“市场 / 账户”底部分页，右上角可显示或隐藏总资产；现货、U 本位、币本位、期权及 Simple Earn 活期和定期权益统一折合为 USDT，并显示账户明细与非零仓位。
- 右键任务栏组件可配置或删除 Binance HMAC API 凭据，凭据由 Windows 凭据管理器持久保存。
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

## 账户持仓配置

右键任务栏行情组件，选择“配置 Binance API”，输入 API Key 和 HMAC Secret 后保存。凭据保存在当前用户的 Windows 凭据管理器中，不会写入普通配置文件、日志或 Git；后续启动无需重复配置。

也可以临时使用环境变量。仅当凭据管理器中没有有效凭据时，程序才会读取它们：

```powershell
$env:CRYPTOTRAY_BINANCE_API_KEY = "你的 API Key"
$env:CRYPTOTRAY_BINANCE_API_SECRET = "你的 HMAC Secret"
Start-Process .\build\CryptoTray.exe
```

请为 CryptoTray 单独创建仅具备账户读取权限的 API Key，不要启用交易或提现权限。若账户页提示期权接口无权限，请确认 Binance 账户已开通期权，并允许该密钥读取期权账户。右键菜单中的“删除 API 凭据”会清除持久凭据并停止账户同步；环境变量只在当前 PowerShell 会话及其启动的程序中有效。
