# CryptoTray development notes

- Implement the project one phase at a time; do not add later-phase features early.
- Keep the application lightweight and event-driven, using C++17 and Qt 6 only.
- Prefer focused changes, Qt parent-child ownership, and simple maintainable code.
- This is a learning project. Add concise Chinese comments around class responsibilities, important control flow, ownership, and non-obvious decisions; avoid noisy line-by-line translation comments.
- After every change, configure/build the project, inspect warnings and errors, and review the diff.
- Phase 1 contains only the application skeleton, system tray icon, and tray menu. It does not contain Binance networking or a price popup.
