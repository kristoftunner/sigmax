add_library(sigmax STATIC)
add_library(sigmax::sigmax ALIAS sigmax)
target_link_libraries(sigmax 
  PRIVATE 
  sigmax_options 
  sigmax_warnings 
  Boost::boost
  spdlog::spdlog
)

target_include_directories(sigmax ${WARNING_GUARD} PUBLIC 
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sigmax>
)

set_target_properties(
  sigmax
  PROPERTIES VERSION ${PROJECT_VERSION}
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN YES)

add_subdirectory(sigmax)
