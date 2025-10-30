cmake_minimum_required(VERSION 3.24)
project(securecloud_gateway LANGUAGES CXX)

# ---- C++ toolchain ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Aide CMake à trouver les paquets MSYS2 quand VS Code n'est pas lancé depuis le terminal MSYS2
if (MINGW AND NOT CMAKE_PREFIX_PATH)
  if (EXISTS "C:/msys64/mingw64")
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/mingw64")
  endif()
endif()

# ---- jwt-cpp (INTERFACE, sans construire les examples) ----
include(FetchContent)
FetchContent_Declare(jwt-cpp
  GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
  GIT_TAG        v0.7.0
  GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(jwt-cpp)
if (NOT jwt-cpp_POPULATED)
  FetchContent_Populate(jwt-cpp)
  add_library(jwt-cpp INTERFACE)
  target_include_directories(jwt-cpp INTERFACE ${jwt-cpp_SOURCE_DIR}/include)
  add_library(jwt-cpp::jwt-cpp ALIAS jwt-cpp)
endif()

# ---- Paquets MSYS2 ----
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)  # on forcera header-only ci-dessous

# ---- Librairie du gateway ----
add_library(gateway_lib
  src/server.cpp
  src/router.cpp
  src/jwtFilter.cpp
  src/upstreamProxy.cpp
  src/authzFilter.cpp
  src/tokenIntrospector.cpp
  src/authCache.cpp
  src/metrics.cpp
  src/auditSink.cpp
)

target_include_directories(gateway_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---- spdlog: header-only pour éviter les collisions avec la DLL ----
if (TARGET spdlog::spdlog_header_only)
  target_link_libraries(gateway_lib PRIVATE spdlog::spdlog_header_only)
  target_compile_definitions(gateway_lib PRIVATE SPDLOG_FMT_EXTERNAL)
else()
  # Fallback si la cible header_only n'existe pas : n'utilise PAS la lib, force header-only par macro
  target_compile_definitions(gateway_lib PRIVATE SPDLOG_HEADER_ONLY SPDLOG_FMT_EXTERNAL)
endif()

target_link_libraries(gateway_lib
  PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    jwt-cpp::jwt-cpp
    fmt::fmt
)

if (WIN32)
  target_link_libraries(gateway_lib PRIVATE ws2_32)
endif()

# ---- Binaire ----
add_executable(gateway src/main.cpp)
target_link_libraries(gateway PRIVATE gateway_lib)

# ---- Tests (optionnel) ----
include(CTest)
if (BUILD_TESTING)
  find_package(GTest CONFIG QUIET)
  if (GTest_FOUND)
    add_executable(router_tests tests/test_router.cpp)
    target_link_libraries(router_tests PRIVATE gateway_lib GTest::gtest_main)
    add_test(NAME router_tests COMMAND router_tests)
  endif()
endif()

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID})")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Prefix paths: ${CMAKE_PREFIX_PATH}")
