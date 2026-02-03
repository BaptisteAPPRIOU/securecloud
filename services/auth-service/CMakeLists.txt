cmake_minimum_required(VERSION 3.22)

# ---- CMake Policies ----
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(auth-service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==========================================
# Dépendances
# ==========================================
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(fmt CONFIG REQUIRED)

# jwt-cpp en header-only
if(NOT TARGET jwt-cpp::jwt-cpp)
    include(FetchContent)
    FetchContent_Declare(jwt_cpp
        URL https://github.com/Thalhammer/jwt-cpp/archive/refs/tags/v0.7.1.tar.gz
        SOURCE_SUBDIR _no_cmake_  # pas de CMakeLists.txt interne
    )
    FetchContent_MakeAvailable(jwt_cpp)

    add_library(jwt-cpp INTERFACE)
    target_include_directories(jwt-cpp SYSTEM INTERFACE ${jwt_cpp_SOURCE_DIR}/include)
    add_library(jwt-cpp::jwt-cpp ALIAS jwt-cpp)
endif()

# ==========================================
# Sources "core" (tout sauf main.cpp)
# ==========================================
set(AUTH_SERVICE_CORE_SOURCES
    # domain
    src/domain/Jwk.cpp
    src/domain/RefreshToken.cpp
    src/domain/RevokedToken.cpp
    src/domain/Role.cpp
    src/domain/User.cpp
    src/domain/UserRole.cpp

    # repositories
    src/repository/UserRepository.cpp
    src/repository/KeyRepository.cpp
    src/repository/RoleRepository.cpp
    src/repository/TokenRepository.cpp

    # services
    src/service/AuthServiceCore.cpp
    src/service/CredentialVerifier.cpp
    src/service/JwtService.cpp
    src/service/AuditPublisher.cpp
    src/service/CryptoService.cpp 
    src/service/MfaService.cpp
    src/service/PermissionService.cpp
    src/service/PasswordResetService.cpp 
    src/service/ProfileService.cpp     

    # web / controllers
    src/web/AuthController.cpp
    src/web/PasswordResetController.cpp   
)

# ==========================================
# Library core (réutilisable par l'exe et les tests)
# ==========================================
add_library(auth-service-core STATIC ${AUTH_SERVICE_CORE_SOURCES})

target_include_directories(auth-service-core
    PUBLIC                      # tests + exe voient les headers
        ${Boost_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIR}
        ${PostgreSQL_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(auth-service-core
    PUBLIC
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        nlohmann_json::nlohmann_json
        ${PostgreSQL_LIBRARIES}
        pqxx
        ws2_32                # Winsock pour Windows / Boost.Asio
        jwt-cpp::jwt-cpp
        fmt::fmt
)

# ==========================================
# Binaire principal
# ==========================================
add_executable(auth-service
    src/main.cpp
)

target_link_libraries(auth-service
    PRIVATE auth-service-core
)

# ==========================================
# Tests unitaires (GoogleTest) – pilotés par BUILD_TESTING (root)
# ==========================================
if(BUILD_TESTING)
    include(FetchContent)

    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(googletest)

    enable_testing()  # sans danger même si déjà appelé au root

    add_executable(auth-service-tests
        tests/AuthControllerTests.cpp
    )

    target_link_libraries(auth-service-tests
        PRIVATE
            auth-service-core
            GTest::gtest
            GTest::gtest_main
    )

    add_test(NAME AuthServiceTests COMMAND auth-service-tests)
endif()