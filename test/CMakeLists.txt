cmake_minimum_required(VERSION 3.15...3.23)

#project(CmakeConfigPackageTests LANGUAGES CXX)

# ---- Test as standalone project the exported config package ----

#find_package(sigmax CONFIG REQUIRED) # for project_options, ...

#if(NOT TARGET sigmax_options)
#    message(FATAL_ERROR "Required config package not found!")
#    return() # be strictly paranoid for Template Janitor github action! CK
#endif()
add_executable(unit_tests mpsc_test.cpp)

target_link_libraries(unit_tests PRIVATE sigmax::sigmax GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(unit_tests)
