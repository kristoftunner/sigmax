# gathering source files
target_sources(sigmax PRIVATE 
  database.cpp
)

# gathering headers
target_sources(sigmax PUBLIC
  order_type.hpp
  database.hpp
)
