add_library(sigmax STATIC)
add_library(sigmax::sigmax ALIAS sigmax)
target_link_libraries(sigmax PUBLIC sigmax_options sigmax_warnings spdlog::spdlog fmt::fmt nlohmann_json::nlohmann_json)

# Optionally link Tracy profiler
target_link_libraries(sigmax PUBLIC Tracy::TracyClient)
if(sigmax_ENABLE_TRACY)
    target_compile_definitions(sigmax PUBLIC TRACY_ENABLE)
endif()

target_include_directories(sigmax ${WARNING_GUARD} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sigmax>)

set_target_properties(
    sigmax
    PROPERTIES VERSION ${PROJECT_VERSION}
               CXX_VISIBILITY_PRESET hidden
               VISIBILITY_INLINES_HIDDEN YES)

add_subdirectory(sigmax)
