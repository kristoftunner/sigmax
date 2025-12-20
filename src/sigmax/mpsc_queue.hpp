#pragma once

#include <atomic>
#include <cstdint>
#include <expected>
#include <vector>

#include <boost/beast/core/span.hpp>

#include "log.hpp"

namespace sigmax {

// TODO: pimp up the queue with a custom allocator type
template<typename T, int C> class MpscQueue
{
public:
    enum class QueueError : std::uint8_t { QUEUE_IS_EMPTY, QUEUE_IS_FULL };
    explicit MpscQueue(const int size) : m_data(size) {}
    /// \brief pushing back a single element
    void PushBack(const T &element)
    {
        const int newHead = (m_head + 1) % static_cast<int>(m_data.size());
        if (m_tail == newHead) {
            // new tail is one after the head -> we lost the oldest data in the queue
            m_tail = (m_tail + 1) % static_cast<int>(m_data.size());
            Logger::Warn("Head reached tail, dropping a queue element");
        }
        m_head = newHead;
        m_data[newHead] = element;
    }
    /// \brief pushing back multiple elements to the queue
    void PushBack(const std::vector<T> elements) {}
    /// \brief Pops out all the elements from the queue using a single read
    std::expected<QueueError, T> Pop()
    {
        if (m_head != m_tail) {
            const T elem = m_data[m_tail];
            m_tail = (m_tail + 1) % static_cast<int>(m_data.size());
            return std::move(elem);
        } else {
            return QueueError::QUEUE_IS_EMPTY;
        }
    }
    std::vector<T> Flush()
    {
        // the head is bigger than tail -> contiguous vector copy
        // head is smaller than tail -> ringbuffer turned over the contiguous vector
        // head equals to tail -> no pop
        if (m_head == m_tail) {
            return {};
        } else if (m_head > m_tail) {
            std::vector<T> ret(m_data.begin() + m_tail, m_data.begin() + m_head);
            m_head = static_cast<int>(m_tail);
            return std::move(ret);
        } else if (m_head < m_tail) {
            // assuming that the data is from tail -> end of the vector -> beginning -> head
            std::vector<T> ret(m_data.begin() + m_tail, m_data.end());
            ret.append_range(m_data.begin(), m_data.begin() + m_head);
            m_head = static_cast<int>(m_tail);
            return std::move(ret);
        }
    }

private:
    std::array<T, C> m_data;
    std::atomic<int> m_head{ 0 }, m_tail{ 0 };
};
}// namespace sigmax
