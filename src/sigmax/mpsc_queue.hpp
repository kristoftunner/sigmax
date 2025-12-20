#pragma once

#include <atomic>
#include <vector>

#include <boost/beast/core/span.hpp>

#include "log.hpp"

namespace sigmax {

// TODO: pimp up the queue with a custom allocator type
template<typename T> class MpscQueue
{
public:
    explicit MpscQueue(const int size) : m_data(size) {}
    /// \brief pushing back a single element
    void PushBack(const T &element) {
        const int newHead = (m_head + 1) % static_cast<int>(m_data.size());
        if (m_tail == newHead) {
            m_tail += 1;
            Logger::Warn("Head reached tail, dropping a queue element");
        }
        m_head = newHead;
        m_data[newHead] = element;

  }
    /// \brief pushing back multiple elements to the queue
    void PushBack(const std::vector<T> elements)
    {
    }
    /// \brief Pops out all the elements from the queue using a single read
    boost::beast::span<T> Pop() {}

private:
    std::vector<T> m_data;
    std::atomic<int> m_head{ 0 }, m_tail{ 0 };
};
}// namespace sigmax
