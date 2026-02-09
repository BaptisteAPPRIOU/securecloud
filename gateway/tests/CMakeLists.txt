if (GTest_FOUND)
  add_executable(router_tests testRouter.cpp)
  target_link_libraries(router_tests PRIVATE gateway_lib GTest::gtest_main)
  add_test(NAME router_tests COMMAND router_tests)
  
  # Also add the UnitTests folder so unit tests are built automatically
  add_subdirectory(UnitTests)
endif()