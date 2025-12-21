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
        m_head = newHead;
        if (m_tail == newHead) {
            // new tail is one after the head -> we lost the oldest data in the queue
            m_tail = (m_tail + 1) % m_data.size();
            Logger::Warn("Head reached tail, queue is full");
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
        if (m_head != m_tail) {
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
        // head equals to tail -> no pop
        if (m_head == m_tail) {
            m_size = 0;
            return {};
        } else if (m_head > m_tail) {
            std::vector<T> ret(m_data.begin() + m_tail, m_data.begin() + m_head);
            m_head = static_cast<int>(m_tail);
            m_size = 0;
            return std::move(ret);
        } else if (m_head < m_tail) {
            // assuming that the data is from tail -> end of the vector -> beginning -> head
            std::vector<T> ret(m_size);
            const std::size_t tailToEnd = m_data.size() - m_tail;
            std::memcpy(ret.data(), m_data.data() + m_tail, tailToEnd * sizeof(T));
            std::memcpy(ret.data() + tailToEnd, m_data.data(), m_head * sizeof(T));
            m_head = static_cast<int>(m_tail);
            m_size = 0;
            return ret;
        }
    }

private:
    std::array<T, C> m_data{ };
    std::atomic<std::size_t> m_head{ 0 }, m_tail{ 0 }, m_size{ 0 };
};
}// namespace sigmax
