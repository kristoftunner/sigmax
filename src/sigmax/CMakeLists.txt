# gathering source files
target_sources(sigmax PRIVATE 
  database.cpp
  mpsc_queue.cpp
)

# gathering headers
target_sources(sigmax PUBLIC
  order_type.hpp
  database.hpp
  instruments.hpp
  mpsc_queue.hpp
)
