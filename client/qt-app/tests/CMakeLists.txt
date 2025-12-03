cmake_minimum_required(VERSION 3.16)

find_package(Qt6 COMPONENTS Test Widgets REQUIRED)

# Test: LandingPage
add_executable(test_landing
    test_landing.cpp
    ../src/landingpage.cpp
    ../src/landingpage.ui
)
target_link_libraries(test_landing PRIVATE Qt6::Test Qt6::Widgets)
add_test(NAME test_landing COMMAND test_landing)

# Test: LoginPage
add_executable(test_login
    test_login.cpp
    ../src/loginpage.cpp
    ../src/loginpage.ui
)
target_link_libraries(test_login PRIVATE Qt6::Test Qt6::Widgets)
add_test(NAME test_login COMMAND test_login)

# Test: ThemeManager
add_executable(test_thememanager
    test_thememanager.cpp
    ../src/ThemeManager.cpp
)
target_link_libraries(test_thememanager PRIVATE Qt6::Test Qt6::Widgets)
add_test(NAME test_thememanager COMMAND test_thememanager)
