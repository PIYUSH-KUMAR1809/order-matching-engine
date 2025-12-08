add_executable(unit_tests test_orderbook.cpp)

target_link_libraries(unit_tests
    PRIVATE
    matching_engine
    GTest::gtest_main
)

target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(unit_tests)
