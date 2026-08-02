# Boost is built from source into ext/ by scripts/build_boost.sh. The Ubuntu
# 22.04 system package is 1.74, which predates Boost.JSON (added in 1.75), so
# the version floor below also guards against silently picking it up.
if(NOT DEFINED Boost_ROOT AND EXISTS "${PROJECT_SOURCE_DIR}/ext/boost")
    set(Boost_ROOT "${PROJECT_SOURCE_DIR}/ext/boost")
endif()
# Boost.System is header-only as of 1.91 and ships no component of its own;
# Boost::headers covers it, along with Beast and Asio.
find_package(Boost 1.91 REQUIRED CONFIG COMPONENTS json)

add_library(sigmax STATIC)
add_library(sigmax::sigmax ALIAS sigmax)
target_link_libraries(
    sigmax
    PUBLIC sigmax_options
           sigmax_warnings
           spdlog::spdlog
           fmt::fmt
           nlohmann_json::nlohmann_json)

# Optionally link Tracy profiler
target_link_libraries(sigmax PUBLIC Tracy::TracyClient Boost::headers Boost::json)

# OpenSSL for Boost.Beast SSL/TLS (public: binance_api.hpp pulls in openssl headers)
target_link_libraries(sigmax PUBLIC OpenSSL::SSL OpenSSL::Crypto)
target_compile_definitions(sigmax PUBLIC $<$<CONFIG:Debug>:TRACY_ENABLE>)

target_include_directories(sigmax ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sigmax>)
target_compile_options(sigmax PUBLIC -g -O0)

set_target_properties(
    sigmax
    PROPERTIES VERSION ${PROJECT_VERSION}
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN YES)

add_subdirectory(sigmax)
