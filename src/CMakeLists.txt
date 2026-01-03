add_library(matching_engine
  Exchange.cpp
  OrderBook.cpp
  Order.cpp
  TcpServer.cpp
)

target_include_directories(matching_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(OrderMatchingEngine main.cpp)
target_link_libraries(OrderMatchingEngine PRIVATE matching_engine)

add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE matching_engine)



