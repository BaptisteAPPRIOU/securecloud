# Unit tests for gateway
find_package(GTest CONFIG REQUIRED)

add_executable(unit_tests
  test_config.cpp
  test_full_config.cpp
  test_tls.cpp
  test_auth_cache.cpp
  test_server_stub.cpp
  test_rate_limiter.cpp
)

target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(unit_tests PRIVATE gateway_lib GTest::gtest_main)
add_test(NAME unit_tests COMMAND unit_tests)
