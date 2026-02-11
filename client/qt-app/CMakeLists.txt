cmake_minimum_required(VERSION 3.16)
project(MSF_Login LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 COMPONENTS Widgets Network REQUIRED)


add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ThemeManager.h src/ThemeManager.cpp
    src/MainWindow.h src/MainWindow.cpp
    assets/resources.qrc
    src/landingpage.h src/landingpage.cpp src/landingpage.ui
    src/loginpage.h src/loginpage.cpp src/loginpage.ui
    src/forgotpage.h src/forgotpage.cpp src/forgotpage.ui
    src/homepage.h src/homepage.cpp src/homepage.ui
)

target_link_libraries(${PROJECT_NAME} Qt6::Widgets Qt6::Network)

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
endif()

# --- Tests (optional) -------------------------------------------------
option(BUILD_TESTS "Build unit tests" ON)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
