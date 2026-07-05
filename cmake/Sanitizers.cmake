# Applies the selected sanitizer mode to project_name (an INTERFACE target).
#
# MODE is a single mutually-exclusive choice, since ASan, TSan and MSan each
# maintain their own shadow memory and cannot be combined:
#   OFF     - no sanitizer
#   address - AddressSanitizer + UndefinedBehaviorSanitizer (leak detection included)
#   thread  - ThreadSanitizer
#   memory  - MemorySanitizer (Clang only)
function(sigmax_enable_sanitizers project_name MODE)
    if(MODE STREQUAL "OFF" OR MODE STREQUAL "")
        return()
    endif()

    if(MODE STREQUAL "address")
        set(FLAGS "address,undefined")
    elseif(MODE STREQUAL "thread")
        set(FLAGS "thread")
    elseif(MODE STREQUAL "memory")
        if(NOT CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
            message(WARNING "MemorySanitizer is only supported by Clang; sigmax_SANITIZER=memory ignored")
            return()
        endif()
        message(
            WARNING "MemorySanitizer requires all code (including libc++) to be MSan-instrumented, otherwise it reports false positives"
        )
        set(FLAGS "memory")
    else()
        message(FATAL_ERROR "Invalid sigmax_SANITIZER='${MODE}'. Valid values: OFF, address, thread, memory")
    endif()

    target_compile_options(${project_name} INTERFACE -fsanitize=${FLAGS})
    target_link_options(${project_name} INTERFACE -fsanitize=${FLAGS})
endfunction()
