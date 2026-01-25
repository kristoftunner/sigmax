add_library(sigmax STATIC)
add_library(sigmax::sigmax ALIAS sigmax)
target_link_libraries(sigmax PUBLIC sigmax_options sigmax_warnings spdlog::spdlog fmt::fmt nlohmann_json::nlohmann_json)

# Optionally link Tracy profiler
target_link_libraries(sigmax PUBLIC Tracy::TracyClient)
target_compile_definitions(sigmax PUBLIC $<$<CONFIG:Debug>:TRACY_ENABLE>)

target_include_directories(sigmax ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sigmax>)
target_compile_options(sigmax PUBLIC -g -O0)

set_target_properties(
    sigmax
    PROPERTIES VERSION ${PROJECT_VERSION}
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN YES)

add_subdirectory(sigmax)
