cmake_minimum_required(VERSION 3.16)
project(cpp_http_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32 AND NOT DEFINED PostgreSQL_ROOT)
    foreach(_pg_root "C:/msys64/ucrt64" "C:/msys64/mingw64")
        if(EXISTS "${_pg_root}/include/libpq-fe.h")
            set(PostgreSQL_ROOT "${_pg_root}" CACHE PATH "PostgreSQL install prefix")
            break()
        endif()
    endforeach()
endif()

find_package(PostgreSQL REQUIRED)

add_library(http_core
    src/http_request.cpp
    src/http_response.cpp
    src/http_router.cpp
    src/http_prometheus.cpp
    src/json_utils.cpp
    src/note_store.cpp
    src/middleware.cpp
    src/rate_limiter.cpp
    src/server_config.cpp
    src/thread_pool.cpp
    src/http_logger.cpp
    src/shutdown.cpp
)
target_include_directories(http_core PUBLIC include ${PostgreSQL_INCLUDE_DIRS})
target_link_libraries(http_core PUBLIC ${PostgreSQL_LIBRARIES})

add_library(http_server_lib
    src/http_server.cpp
)
target_link_libraries(http_server_lib PUBLIC http_core)
target_include_directories(http_server_lib PUBLIC include)

if(WIN32)
    target_link_libraries(http_server_lib PUBLIC ws2_32)
endif()

add_executable(http_server src/main.cpp)
target_link_libraries(http_server PRIVATE http_server_lib)

option(BUILD_TESTING "Build unit and integration tests" ON)
if(BUILD_TESTING)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)

    add_executable(http_tests
        tests/test_main.cpp
        tests/test_http_request.cpp
        tests/test_http_response.cpp
        tests/test_http_router.cpp
        tests/test_http_prometheus.cpp
        tests/test_note_store.cpp
        tests/test_json_utils.cpp
        tests/test_rate_limiter.cpp
        tests/test_server_config.cpp
        tests/test_integration.cpp
        tests/test_database.cpp
        tests/http_client.cpp
    )
    target_link_libraries(http_tests PRIVATE http_core http_server_lib doctest::doctest)
    target_include_directories(http_tests PRIVATE include tests)

    add_test(NAME http_tests COMMAND http_tests)
endif()
