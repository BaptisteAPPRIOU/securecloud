cmake_minimum_required(VERSION 3.22)

# ---- CMake Policies ----
# CMP0135: Set timestamps of extracted files to time of extraction
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
# CMP0167: Use Boost's CMake config instead of FindBoost
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(auth-service LANGUAGES CXX)

# Ubuntu's libpqxx package is built with a C++17 ABI baseline.
# Building this target as C++17 avoids source_location symbol mismatches.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# Configure MinGW64 paths
# ==========================================
if (MINGW)
    set(CMAKE_PREFIX_PATH "C:/msys64/ucrt64" ${CMAKE_PREFIX_PATH})
    set(OPENSSL_ROOT_DIR "C:/msys64/ucrt64")
endif()

# ==========================================
# Dépendances
# ==========================================
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(libpqxx CONFIG QUIET)

set(AUTH_PQXX_TARGET "")
if(TARGET libpqxx::pqxx)
    set(AUTH_PQXX_TARGET libpqxx::pqxx)
elseif(TARGET pqxx::pqxx)
    set(AUTH_PQXX_TARGET pqxx::pqxx)
else()
    set(_auth_pqxx_hints "")
    foreach(_pg_lib IN LISTS PostgreSQL_LIBRARIES)
        if(IS_ABSOLUTE "${_pg_lib}")
            get_filename_component(_pg_lib_dir "${_pg_lib}" DIRECTORY)
            list(APPEND _auth_pqxx_hints "${_pg_lib_dir}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _auth_pqxx_hints)

    find_library(AUTH_PQXX_LIBRARY
        NAMES pqxx
        HINTS ${_auth_pqxx_hints}
        NO_DEFAULT_PATH
    )
    if(NOT AUTH_PQXX_LIBRARY)
        find_library(AUTH_PQXX_LIBRARY NAMES pqxx REQUIRED)
    endif()
    set(AUTH_PQXX_TARGET "${AUTH_PQXX_LIBRARY}")
endif()

# ==========================================
# jwt-cpp en header-only
# Check if target already exists (when built from root CMakeLists)
# ==========================================
if(NOT TARGET jwt-cpp::jwt-cpp)
    include(FetchContent)
    FetchContent_Declare(jwt_cpp
        URL https://github.com/Thalhammer/jwt-cpp/archive/refs/tags/v0.7.1.tar.gz
        URL_HASH SHA256=e52f247d5e62fac5da6191170998271a70ce27f747f2ce8fde9b09f96a5375a4
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR _no_cmake_  # Skip jwt-cpp's CMakeLists.txt to avoid CMP0135 warning
    )
    
    FetchContent_MakeAvailable(jwt_cpp)
    
    # Create interface library pointing to the headers
    add_library(jwt-cpp INTERFACE)
    target_include_directories(jwt-cpp SYSTEM INTERFACE ${jwt_cpp_SOURCE_DIR}/include)
    add_library(jwt-cpp::jwt-cpp ALIAS jwt-cpp)
endif()

# ==========================================
# Binaire
# ==========================================
add_executable(auth-service
    src/main.cpp
)

target_include_directories(auth-service PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${PostgreSQL_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(auth-service PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ${AUTH_PQXX_TARGET}
    ${PostgreSQL_LIBRARIES}
    jwt-cpp::jwt-cpp
)

if (WIN32)
    target_link_libraries(auth-service PRIVATE ws2_32)
endif()
