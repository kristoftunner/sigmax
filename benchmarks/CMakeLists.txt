cmake_minimum_required(VERSION 3.15...3.23)

project(BenchmarkTests LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

if(PROJECT_IS_TOP_LEVEL OR TEST_INSTALLED_VERSION)
    find_package(sigmax CONFIG REQUIRED) # for project_options, ...

    if(NOT TARGET sigmax_options)
        message(FATAL_ERROR "Required config package not found!")
        return() # be strictly paranoid for Template Janitor github action! CK
    endif()
    add_executable(benchmark_test mpsc_qeueu_benchmark.cpp)

    target_sources(benchmark_test PRIVATE mpsc_qeueu_benchmark.hpp)

    target_link_libraries(benchmark_test PRIVATE sigmax)
endif()
