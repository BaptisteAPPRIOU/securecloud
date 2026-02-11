if (GTest_FOUND)
  add_executable(router_tests testRouter.cpp)
  target_link_libraries(router_tests PRIVATE gateway_lib GTest::gtest_main)
  add_test(NAME router_tests COMMAND router_tests)

  add_executable(auth_session_e2e_tests test_auth_session_e2e.cpp)
  target_link_libraries(auth_session_e2e_tests PRIVATE gateway_lib GTest::gtest_main)
  add_test(NAME auth_session_e2e_tests COMMAND auth_session_e2e_tests)
  set_tests_properties(auth_session_e2e_tests PROPERTIES LABELS "e2e;integration;auth")
  
  # Also add the UnitTests folder so unit tests are built automatically
  add_subdirectory(UnitTests)
endif()
