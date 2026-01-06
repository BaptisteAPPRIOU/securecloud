cmake_minimum_required(VERSION 3.22)

# Evite le warning récent sur FindBoost supprimé
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(auth-service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# Configure MinGW64 paths
# ==========================================
if (MINGW)
    set(CMAKE_PREFIX_PATH "C:/msys64/mingw64" ${CMAKE_PREFIX_PATH})
    set(OPENSSL_ROOT_DIR "C:/msys64/mingw64")
endif()

# ==========================================
# Dépendances
# ==========================================
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)

# ==========================================
# jwt-cpp en header-only SANS add_subdirectory
# (évite la détection du compilateur C chez tes collègues)
# ==========================================
include(FetchContent)
FetchContent_Declare(jwt_cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.0
)
FetchContent_GetProperties(jwt_cpp)
if(NOT jwt_cpp_POPULATED)
    FetchContent_Populate(jwt_cpp)
endif()

add_library(jwt-cpp INTERFACE)
target_include_directories(jwt-cpp INTERFACE
    ${jwt_cpp_SOURCE_DIR}/include
)
add_library(jwt-cpp::jwt-cpp ALIAS jwt-cpp)

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
    ${PostgreSQL_LIBRARIES}
    pqxx
    ws2_32
    jwt-cpp::jwt-cpp
)