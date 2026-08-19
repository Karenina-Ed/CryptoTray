cmake_minimum_required(VERSION 3.21)

project(CryptoTray
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    WebSockets
)

qt_standard_project_setup()

qt_add_executable(CryptoTray
    src/main.cpp
    src/market/market_data_service.cpp
    src/market/market_data_service.h
    src/market/market_message_parser.cpp
    src/market/market_message_parser.h
    src/market/ticker.h
    src/ui/tray_manager.cpp
    src/ui/tray_manager.h
    src/ui/market_detail_card.cpp
    src/ui/market_detail_card.h
    src/ui/taskbar_ticker_widget.cpp
    src/ui/taskbar_ticker_widget.h
)

target_include_directories(CryptoTray PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(CryptoTray PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebSockets
)

target_compile_options(CryptoTray PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

set_target_properties(CryptoTray PROPERTIES
    # Debug 和 Release 都使用 Windows GUI 子系统，启动时不创建黑色控制台窗口。
    WIN32_EXECUTABLE TRUE
)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
