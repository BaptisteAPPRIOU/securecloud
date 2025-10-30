cmake_minimum_required(VERSION 3.24)
project(securecloud_gateway CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)


include(CTest)


find_package(OpenSSL REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
# Prometheus-Cpp est optionnel pour le kit minimal; commente si absent
find_package(Prometheus-Cpp CONFIG QUIET)


add_library(gateway_lib
src/http_server.cpp
src/router.cpp
src/jwt_filter.cpp
src/upstream_proxy.cpp
src/authz_filter.cpp
src/token_introspector.cpp
src/auth_cache.cpp
src/metrics.cpp
src/audit_sink.cpp
)


target_include_directories(gateway_lib PUBLIC include)


target_link_libraries(gateway_lib
PRIVATE OpenSSL::SSL OpenSSL::Crypto
spdlog::spdlog nlohmann_json::nlohmann_json
jwt-cpp::jwt-cpp fmt::fmt
)
if (TARGET Prometheus-Cpp::core)
target_link_libraries(gateway_lib PRIVATE Prometheus-Cpp::core)
endif()


add_executable(gateway src/main.cpp)


# Pour Windows: éviter l'erreur sur WinMain
if (WIN32)
target_link_options(gateway PRIVATE /SUBSYSTEM:CONSOLE)
endif()


target_link_libraries(gateway PRIVATE gateway_lib)


if (BUILD_TESTING)
add_subdirectory(tests)
endif()