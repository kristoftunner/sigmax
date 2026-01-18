cmake_minimum_required(VERSION 3.21)

# This template attempts to be "fetch_content"-able
# so that it works well with tools like CPM or other
# manual dependency management

# Only set the cxx_standard if it is not set by someone else
if(NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
endif()

# strongly encouraged to enable this globally to avoid conflicts between
# -Wpedantic being enabled and -std=c++20 and -std=gnu++20 for example
# when compiling with PCH enabled
set(CMAKE_CXX_EXTENSIONS OFF)

# Set the project name and language
project(
    sigmax
    VERSION 0.0.1
    DESCRIPTION ""
    HOMEPAGE_URL "https://github.com/kristoftunner/sigmax"
    LANGUAGES CXX C)

include(cmake/PreventInSourceBuilds.cmake)
include(ProjectOptions.cmake)

sigmax_setup_options()

sigmax_global_options()
include(Dependencies.cmake)
sigmax_setup_dependencies()

sigmax_local_options()

# don't know if this should be set globally from here or not...
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(GIT_SHA
    "Unknown"
    CACHE STRING "SHA this build was generated from")
string(
    SUBSTRING "${GIT_SHA}"
              0
              8
              GIT_SHORT_SHA)

target_compile_features(sigmax_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_library(sigmax::sigmax_options ALIAS sigmax_options)
add_library(sigmax::sigmax_warnings ALIAS sigmax_warnings)

# configure files based on CMake configuration options
add_subdirectory(configured_files)

# Adding the src:
add_subdirectory(src)

# Don't even look at tests if we're not top level
message(STATUS "PROJECT_IS_TOP_LEVEL: ${PROJECT_IS_TOP_LEVEL}")
if(NOT PROJECT_IS_TOP_LEVEL)
    return()
endif()

# Adding the tests:
include(CTest)
enable_testing()

if(BUILD_TESTING)
    message(AUTHOR_WARNING "Building Tests. Be sure to check out test/constexpr_tests.cpp for constexpr testing")
    add_subdirectory(test)
    add_subdirectory(benchmarks)
endif()

if(sigmax_BUILD_FUZZ_TESTS)
    message(AUTHOR_WARNING "Building Fuzz Tests, using fuzzing sanitizer https://www.llvm.org/docs/LibFuzzer.html")
    if(NOT sigmax_ENABLE_SANITIZER_ADDRESS AND NOT sigmax_ENABLE_SANITIZER_THREAD)
        message(WARNING "You need asan or tsan enabled for meaningful fuzz testing")
    endif()
    add_subdirectory(fuzz_test)

endif()

if(CMAKE_SKIP_INSTALL_RULES)
    return()
endif()

include(cmake/PackageProject.cmake)

# Add other targets that you want installed here, by default we just package the one executable
# we know we want to ship
sigmax_package_project(
    TARGETS sigmax_options sigmax_warnings
)

# Experience shows that explicit package naming can help make it easier to sort
# out potential ABI related issues before they start, while helping you
# track a build to a specific GIT SHA
set(CPACK_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${GIT_SHORT_SHA}-${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
)

include(CPack)
