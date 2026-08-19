find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(CryptoTrayMarketTests
    market_message_parser_test.cpp
    ../src/market/market_message_parser.cpp
    ../src/market/market_message_parser.h
    ../src/market/ticker.h
)

target_include_directories(CryptoTrayMarketTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(CryptoTrayMarketTests PRIVATE
    Qt6::Core
    Qt6::Test
)

target_compile_options(CryptoTrayMarketTests PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

add_test(NAME CryptoTrayMarketTests COMMAND CryptoTrayMarketTests)

# 该程序用于手动验证真实网络，不注册到 CTest，避免离线环境影响单元测试。
qt_add_executable(CryptoTrayMarketSmoke
    market_connection_smoke.cpp
    ../src/market/market_data_service.cpp
    ../src/market/market_data_service.h
    ../src/market/market_message_parser.cpp
    ../src/market/market_message_parser.h
    ../src/market/ticker.h
)

target_include_directories(CryptoTrayMarketSmoke PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(CryptoTrayMarketSmoke PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::WebSockets
)

target_compile_options(CryptoTrayMarketSmoke PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)
