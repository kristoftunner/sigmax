#include "mpsc_queue.hpp"


namespace sigmax {
MpscQueue::MpscQueue(const int size)
:m_data(size)
{}
  void MpscQueue::PushBack(const T& element)
{
    const int newHead = (head + 1) % m_data.size();
    if(tail == newHead)
  {
    m_tail += 1;
  }
  m_head = newhead;
  m_data[newHead] = element;
}
  void MpscQueue::PushBack(const std::vector<T> elements){}
  std::span<T> MpscQueue::Pop(){}

}
