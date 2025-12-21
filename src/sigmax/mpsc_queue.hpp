#pragma once

#include <atomic>
#include <cstdint>
#include <expected>
#include <vector>

#include "log.hpp"

namespace sigmax {

enum class QueueError : std::uint8_t { QUEUE_IS_EMPTY, QUEUE_IS_FULL };
// TODO: pimp up the queue with a custom allocator type
template<typename T, std::size_t C> class MpscQueue
{
public:
    MpscQueue() = default;
    std::size_t Size()
    {
        return m_size;
    }
    /// \brief pushing back a single element
    void PushBack(const T &element)
    {
        const std::size_t newHead = (m_head + 1) % m_data.size();
        m_data[m_head] = element;
        if ((m_size != 0) && (m_tail == m_head)) {
            // new tail is one after the head -> we lost the oldest data in the queue
            m_tail = (m_tail + 1) % m_data.size();
            Logger::Warn("[MPSC] - Head reached tail, queue is full");
        }
        m_head = newHead;
        if(m_size == m_data.size())
        {
            Logger::Warn("[MPSC] - Element is lost, queue is full");
        }
        else {
        m_size++;
        }
    }
    /// \brief pushing back multiple elements to the queue
    void PushBack(const std::vector<T> elements) {}
    /// \brief Pops out all the elements from the queue using a single read
    std::expected<T, QueueError> Pop()
    {
        if (m_size > 0) {
            const T elem = m_data[m_tail];
            m_tail = (m_tail + 1) % m_data.size();
            m_size--;
            return elem;
        } else {
            return std::unexpected(QueueError::QUEUE_IS_EMPTY);
        }
    }
    std::vector<T> Flush()
    {
        // the head is bigger than tail -> contiguous vector copy
        // head is smaller than tail -> ringbuffer turned over the contiguous vector
        std::vector<T> ret(m_size);
        if(((m_head == m_tail) && (m_head != 0)) || (m_head < m_tail)) {
            // assuming that the data is from tail -> end of the vector -> beginning -> head
            const std::size_t tailToEnd = m_data.size() - m_tail;
            std::memcpy(ret.data(), m_data.data() + m_tail, tailToEnd * sizeof(T));
            std::memcpy(ret.data() + tailToEnd, m_data.data(), m_head * sizeof(T));
        }
        else if((m_head == m_tail) && (m_head == 0)) {
            std::memcpy(ret.data(), m_data.data(), m_size * sizeof(T));
        }
        else if (m_head > m_tail) {
            std::memcpy(ret.data(), m_data.data() + m_tail, (m_head - m_tail) * sizeof(T));
        }

        m_head = 0;
        m_tail = 0;
        m_size = 0;
        return ret;
    }

private:
    std::array<T, C> m_data{ };
    std::atomic<std::size_t> m_head{ 0 }, m_tail{ 0 }, m_size{ 0 };
};
}// namespace sigmax
