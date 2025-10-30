find_package(pqxx CONFIG REQUIRED)   # via vcpkg, ou FetchContent si tu préfères

add_executable(auth-service
    src/main.cpp
)
target_link_libraries(auth-service PRIVATE pqxx::pqxx)
