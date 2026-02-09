cmake_minimum_required(VERSION 3.24)

# ---- CMake Policies ----
# CMP0135: Set timestamps of extracted files to time of extraction
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
# CMP0167: Use Boost's CMake config instead of FindBoost
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
endif()

project(securecloud_gateway LANGUAGES CXX)

# ---- C++ toolchain ----
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
  # GCC 15 on MSYS2 workaround: C++ stdlib headers reference C11 functions
  # (at_quick_exit, quick_exit, timespec_get) that aren't exposed by default
  if (MINGW)
    add_compile_definitions(_UCRT __STDC_WANT_LIB_EXT1__=1)
  endif()
endif()

# Aide CMake à trouver les paquets MSYS2 quand VS Code n'est pas lancé depuis le terminal MSYS2
if (MINGW AND NOT CMAKE_PREFIX_PATH)
  if (EXISTS "C:/msys64/ucrt64")
    list(APPEND CMAKE_PREFIX_PATH "C:/msys64/ucrt64")
  endif()
endif()

# ---- jwt-cpp (Header-only library) ----
# Using v0.7.1 - fetch as header-only without processing its CMakeLists.txt
# SOURCE_SUBDIR set to non-existent path prevents add_subdirectory on the fetched content
include(FetchContent)
FetchContent_Declare(jwt-cpp
  URL https://github.com/Thalhammer/jwt-cpp/archive/refs/tags/v0.7.1.tar.gz
  URL_HASH SHA256=e52f247d5e62fac5da6191170998271a70ce27f747f2ce8fde9b09f96a5375a4
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  SOURCE_SUBDIR _no_cmake_  # Non-existent subdir prevents processing jwt-cpp's CMakeLists.txt
)

FetchContent_MakeAvailable(jwt-cpp)

# Create interface library pointing to the headers
if(NOT TARGET jwt-cpp::jwt-cpp)
  add_library(jwt-cpp INTERFACE)
  target_include_directories(jwt-cpp SYSTEM INTERFACE ${jwt-cpp_SOURCE_DIR}/include)
  add_library(jwt-cpp::jwt-cpp ALIAS jwt-cpp)
endif()

# Ensure the target exists (jwt-cpp creates jwt-cpp::jwt-cpp)
if (NOT TARGET jwt-cpp::jwt-cpp)
  add_library(jwt-cpp INTERFACE)
  target_include_directories(jwt-cpp SYSTEM INTERFACE ${jwt-cpp_SOURCE_DIR}/include)
  add_library(jwt-cpp::jwt-cpp ALIAS jwt-cpp)
endif()

# ---- Paquets MSYS2 ----
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)  # on forcera header-only ci-dessous
find_package(Boost REQUIRED COMPONENTS system)  # For Boost.Beast and Boost.Asio

# ---- Librairie du gateway ----
add_library(gateway_lib
  src/config.cpp
  src/server.cpp
  src/router.cpp
  src/jwtFilter.cpp
  src/upstreamProxy.cpp
  src/authzFilter.cpp
  src/tokenIntrospector.cpp
  src/authCache.cpp
  src/metrics.cpp
  src/auditSink.cpp
  src/tlsContext.cpp
  src/httpClient.cpp
  src/udsClient.cpp
  src/websocketHandler.cpp
  src/restApiEndpoints.cpp
  src/rateLimiter.cpp
  src/rateLimitMiddleware.cpp
  src/prometheusExporter.cpp
  src/gatewayMetrics.cpp
  src/requestContext.cpp
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
    Boost::system
)

if (WIN32)
  target_link_libraries(gateway_lib PRIVATE ws2_32 wsock32)
endif()

# ---- Binaire ----
add_executable(gateway src/main.cpp)
target_link_libraries(gateway PRIVATE gateway_lib)

# ---- googletest (if not already available) ----
find_package(GTest CONFIG QUIET)
if (NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
  )
  FetchContent_MakeAvailable(googletest)
endif()

# ---- Tests (optional) ----
include(CTest)
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()

message(STATUS "Compiler: ${CMAKE_CXX_COMPILER} (${CMAKE_CXX_COMPILER_ID})")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Prefix paths: ${CMAKE_PREFIX_PATH}")
