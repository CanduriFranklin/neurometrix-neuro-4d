cmake_minimum_required(VERSION 3.27)
project(neuro-4d VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /EHsc /bigobj)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Eigen3 CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(RdKafka CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)
find_package(DCMTK CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenMP)
find_package(prometheus-cpp CONFIG REQUIRED)

file(GLOB_RECURSE NEURO4D_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/domain/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/application/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/contracts/*.cpp"
)
file(GLOB_RECURSE NEURO4D_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/domain/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/application/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/contracts/*.hpp"
)

add_library(neuro4d_core STATIC ${NEURO4D_SOURCES} ${NEURO4D_HEADERS})
target_include_directories(neuro4d_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(neuro4d_core PUBLIC
    Eigen3::Eigen
    fmt::fmt
    spdlog::spdlog
    RdKafka::rdkafka
    PostgreSQL::PostgreSQL
    DCMTK::dcmdata
    OpenSSL::SSL
    OpenSSL::Crypto
    prometheus-cpp::core
    prometheus-cpp::pull
    Threads::Threads
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(neuro4d_core PUBLIC OpenMP::OpenMP_CXX)
endif()

add_executable(neuro-4d main.cpp)
target_link_libraries(neuro-4d PRIVATE neuro4d_core)

include(CTest)
if(BUILD_TESTING)
    find_package(GTest CONFIG REQUIRED)
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp")
    if(TEST_SOURCES)
        add_executable(neuro-4d-tests ${TEST_SOURCES})
        target_link_libraries(neuro-4d-tests PRIVATE neuro4d_core GTest::gtest GTest::gtest_main)
        include(GoogleTest)
        gtest_discover_tests(neuro-4d-tests)
    endif()
    file(GLOB PERF_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/performance/*.cpp")
    if(PERF_SOURCES)
        add_executable(benchmark_4d ${PERF_SOURCES})
        target_link_libraries(benchmark_4d PRIVATE neuro4d_core)
    endif()
endif()
