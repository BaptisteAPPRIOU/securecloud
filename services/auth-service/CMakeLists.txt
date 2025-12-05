cmake_minimum_required(VERSION 3.22)
project(auth-service CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# Indice pour OpenSSL avec ta toolchain MinGW/MSYS2
# ==========================================
if (MINGW)
    # Chemin standard pour MSYS2 UCRT64
    set(OPENSSL_ROOT_DIR "C:/msys64/ucrt64")
endif()

# ==========================================
# Dépendances de base
# ==========================================
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# jwt-cpp via FetchContent (header-only)
include(FetchContent)
FetchContent_Declare(jwt-cpp
    GIT_REPOSITORY https://github.com/Thalhammer/jwt-cpp.git
    GIT_TAG v0.7.0
)
set(JWT_CPP_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(JWT_CPP_BUILD_TESTS OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(jwt-cpp)

# ==========================================
# Binaire auth-service
# ==========================================
add_executable(auth-service
    src/main.cpp
)

target_include_directories(auth-service PRIVATE
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# On linke les libs par leur nom : pqxx et pq
# (présentes dans C:/msys64/ucrt64/lib si les paquets MSYS2 sont installés)
target_link_libraries(auth-service PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    pqxx
    pq
    jwt-cpp::jwt-cpp
    ws2_32      # <== IMPORTANT pour Boost.Asio sur Windows
)

