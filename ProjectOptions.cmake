include(cmake/SystemLink.cmake)
include(cmake/LibFuzzer.cmake)
include(CMakeDependentOption)
include(CheckCXXCompilerFlag)

include(CheckCXXSourceCompiles)

macro(sigmax_supports_sanitizers)
    if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*")

        message(STATUS "Sanity checking UndefinedBehaviorSanitizer, it should be supported on this platform")
        set(TEST_PROGRAM "int main() { return 0; }")

        # Check if UndefinedBehaviorSanitizer works at link time
        set(CMAKE_REQUIRED_FLAGS "-fsanitize=undefined")
        set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=undefined")
        check_cxx_source_compiles("${TEST_PROGRAM}" HAS_UBSAN_LINK_SUPPORT)

        if(HAS_UBSAN_LINK_SUPPORT)
            message(STATUS "UndefinedBehaviorSanitizer is supported at both compile and link time.")
            set(SUPPORTS_UBSAN ON)
        else()
            message(WARNING "UndefinedBehaviorSanitizer is NOT supported at link time.")
            set(SUPPORTS_UBSAN OFF)
        endif()
    else()
        set(SUPPORTS_UBSAN OFF)
    endif()

    message(STATUS "Sanity checking AddressSanitizer, it should be supported on this platform")
    set(TEST_PROGRAM "int main() { return 0; }")

    # Check if AddressSanitizer works at link time
    set(CMAKE_REQUIRED_FLAGS "-fsanitize=address")
    set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
    check_cxx_source_compiles("${TEST_PROGRAM}" HAS_ASAN_LINK_SUPPORT)

    if(HAS_ASAN_LINK_SUPPORT)
        message(STATUS "AddressSanitizer is supported at both compile and link time.")
        set(SUPPORTS_ASAN ON)
    else()
        message(WARNING "AddressSanitizer is NOT supported at link time.")
        set(SUPPORTS_ASAN OFF)
    endif()
endmacro()

macro(sigmax_setup_options)
    option(sigmax_ENABLE_HARDENING "Enable hardening" ON)
    option(sigmax_ENABLE_COVERAGE "Enable coverage reporting" OFF)
    cmake_dependent_option(
        sigmax_ENABLE_GLOBAL_HARDENING
        "Attempt to push hardening options to built dependencies"
        ON
        sigmax_ENABLE_HARDENING
        OFF)

    sigmax_supports_sanitizers()

    message("PROJECT_IS_TOP_LEVEL is: ${PROJECT_IS_TOP_LEVEL}")
    message("sigmax_PACKAGING_MAINTAINER_MODE is: ${sigmax_PACKAGING_MAINTAINER_MODE}")
    if(NOT PROJECT_IS_TOP_LEVEL
       OR sigmax_PACKAGING_MAINTAINER_MODE
       OR CMAKE_BUILD_TYPE MATCHES "Release")
        option(sigmax_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
        set(sigmax_SANITIZER_DEFAULT "OFF")
        option(sigmax_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
        option(sigmax_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
        option(sigmax_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
        option(sigmax_ENABLE_CACHE "Enable ccache" OFF)
    else()
        option(sigmax_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
        if(SUPPORTS_ASAN)
            set(sigmax_SANITIZER_DEFAULT "address")
        else()
            set(sigmax_SANITIZER_DEFAULT "OFF")
        endif()
        option(sigmax_ENABLE_UNITY_BUILD "Enable unity builds" OFF)
        option(sigmax_ENABLE_CLANG_TIDY "Enable clang-tidy" OFF)
        option(sigmax_ENABLE_CPPCHECK "Enable cpp-check analysis" ON)
        option(sigmax_ENABLE_CACHE "Enable ccache" ON)
    endif()

    # A single mutually-exclusive sanitizer choice (ASan, TSan and MSan cannot be combined)
    set(sigmax_SANITIZER
        "${sigmax_SANITIZER_DEFAULT}"
        CACHE STRING "Sanitizer to enable: OFF, address, thread, memory")
    set_property(
        CACHE sigmax_SANITIZER
        PROPERTY STRINGS
                 OFF
                 address
                 thread
                 memory)

    if(NOT PROJECT_IS_TOP_LEVEL)
        mark_as_advanced(
            sigmax_WARNINGS_AS_ERRORS
            sigmax_SANITIZER
            sigmax_ENABLE_UNITY_BUILD
            sigmax_ENABLE_CLANG_TIDY
            sigmax_ENABLE_CPPCHECK
            sigmax_ENABLE_COVERAGE
            sigmax_ENABLE_CACHE)
    endif()

    sigmax_check_libfuzzer_support(LIBFUZZER_SUPPORTED)
    if(LIBFUZZER_SUPPORTED AND NOT sigmax_SANITIZER STREQUAL "OFF")
        set(DEFAULT_FUZZER ON)
    else()
        set(DEFAULT_FUZZER OFF)
    endif()

    option(sigmax_BUILD_FUZZ_TESTS "Enable fuzz testing executable" ${DEFAULT_FUZZER})

    # print all selected options:
    message("--------------------------------")
    message("-- Selected options:")
    message("-- sigmax_WARNINGS_AS_ERRORS: ${sigmax_WARNINGS_AS_ERRORS}")
    message("-- sigmax_SANITIZER: ${sigmax_SANITIZER}")
    message("-- sigmax_ENABLE_UNITY_BUILD: ${sigmax_ENABLE_UNITY_BUILD}")
    message("-- sigmax_ENABLE_CLANG_TIDY: ${sigmax_ENABLE_CLANG_TIDY}")
    message("-- sigmax_ENABLE_CPPCHECK: ${sigmax_ENABLE_CPPCHECK}")
    message("-- sigmax_ENABLE_COVERAGE: ${sigmax_ENABLE_COVERAGE}")
    message("-- sigmax_ENABLE_CACHE: ${sigmax_ENABLE_CACHE}")

endmacro()

macro(sigmax_global_options)
    sigmax_supports_sanitizers()

    if(sigmax_ENABLE_HARDENING AND sigmax_ENABLE_GLOBAL_HARDENING)
        include(cmake/Hardening.cmake)
        if(NOT SUPPORTS_UBSAN OR NOT sigmax_SANITIZER STREQUAL "OFF")
            set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
        else()
            set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
        endif()
        message("${sigmax_ENABLE_HARDENING} ${ENABLE_UBSAN_MINIMAL_RUNTIME} ${sigmax_SANITIZER}")
        sigmax_enable_hardening(sigmax_options ON ${ENABLE_UBSAN_MINIMAL_RUNTIME})
    endif()
endmacro()

macro(sigmax_local_options)
    if(PROJECT_IS_TOP_LEVEL)
        include(cmake/StandardProjectSettings.cmake)
    endif()

    add_library(sigmax_warnings INTERFACE)
    add_library(sigmax_options INTERFACE)

    include(cmake/CompilerWarnings.cmake)
    sigmax_set_project_warnings(
        sigmax_warnings
        ${sigmax_WARNINGS_AS_ERRORS}
        ""
        ""
        "")

    include(cmake/Sanitizers.cmake)
    sigmax_enable_sanitizers(sigmax_options ${sigmax_SANITIZER})

    set_target_properties(sigmax_options PROPERTIES UNITY_BUILD ${sigmax_ENABLE_UNITY_BUILD})

    if(sigmax_ENABLE_CACHE)
        include(cmake/Cache.cmake)
        sigmax_enable_cache()
    endif()

    include(cmake/StaticAnalyzers.cmake)
    if(sigmax_ENABLE_CLANG_TIDY)
        sigmax_enable_clang_tidy(sigmax_options ${sigmax_WARNINGS_AS_ERRORS})
    endif()

    if(sigmax_ENABLE_CPPCHECK)
        sigmax_enable_cppcheck(${sigmax_WARNINGS_AS_ERRORS} "" # override cppcheck options
        )
    endif()

    if(sigmax_ENABLE_COVERAGE)
        include(cmake/Tests.cmake)
        sigmax_enable_coverage(sigmax_options)
    endif()

    if(sigmax_WARNINGS_AS_ERRORS)
        check_cxx_compiler_flag("-Wl,--fatal-warnings" LINKER_FATAL_WARNINGS)
        if(LINKER_FATAL_WARNINGS)
            # This is not working consistently, so disabling for now
            # target_link_options(sigmax_options INTERFACE -Wl,--fatal-warnings)
        endif()
    endif()

    if(sigmax_ENABLE_HARDENING AND NOT sigmax_ENABLE_GLOBAL_HARDENING)
        include(cmake/Hardening.cmake)
        if(NOT SUPPORTS_UBSAN OR NOT sigmax_SANITIZER STREQUAL "OFF")
            set(ENABLE_UBSAN_MINIMAL_RUNTIME FALSE)
        else()
            set(ENABLE_UBSAN_MINIMAL_RUNTIME TRUE)
        endif()
        sigmax_enable_hardening(sigmax_options OFF ${ENABLE_UBSAN_MINIMAL_RUNTIME})
    endif()

endmacro()
