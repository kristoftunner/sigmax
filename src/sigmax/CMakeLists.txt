add_executable(sigmax_app sigmax_app.cpp)
target_link_libraries(sigmax_app PRIVATE sigmax)

# gathering source files
target_sources(
    sigmax
    PRIVATE 
    database.cpp
    mpsc_queue.cpp
    log.cpp
    binance_api.hpp
    binance_api.cpp
    order_type.hpp
    database.hpp
    instruments.hpp
    mpsc_queue.hpp
    log.hpp
  )

