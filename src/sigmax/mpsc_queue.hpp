#pragma once

#include <vector>
#include <atomic>

#include <boost/beast/core/span.hpp>
namespace sigmax {

// TODO: pimp up the queue with a custom allocator type
template<typename T>
class MpscQueue
{
public:
  MpscQueue(const int size);
  /// \brief pushing back a single element
  void PushBack(const T& element);
  /// \brief pushing back multiple elements to the queue
  void PushBack(const std::vector<T> elements);
  /// \brief Pops out all the elements from the queue using a single read
  std::span<T> Pop();
  
private:
  std::vector<T> m_data;
  int size{0};
  std::atomic<int> head{0}, tail{0};

};
}
