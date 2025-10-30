cmake_minimum_required(VERSION 3.16)
project(MSF_Login LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.2 COMPONENTS Widgets REQUIRED)
# Qt5 : find_package(Qt5 COMPONENTS Widgets REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/ThemeManager.h src/ThemeManager.cpp
    src/MainWindow.h src/MainWindow.cpp src/MainWindow.ui
    assets/resources.qrc
)

target_link_libraries(${PROJECT_NAME} Qt6::Widgets) # ou Qt5::Widgets
