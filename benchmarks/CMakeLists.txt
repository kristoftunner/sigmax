cmake_minimum_required(VERSION 3.15...3.23)

project(BenchmarkTests LANGUAGES CXX)

add_executable(benchmark_test mpsc_queue_benchmark.cpp)
target_compile_options(benchmark_test PRIVATE -g -O0)
target_sources(benchmark_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mpsc_queue_benchmark.hpp)
target_link_libraries(benchmark_test PRIVATE sigmax sigmax_options sigmax_warnings)
install(TARGETS benchmark_test DESTINATION bin)
