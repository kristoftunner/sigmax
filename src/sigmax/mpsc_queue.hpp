#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <expected>
#include <iostream>

#include "log.hpp"

namespace sigmax {

enum class QueueState : std::uint8_t { SUCCESS, QUEUE_IS_EMPTY, QUEUE_IS_FULL };
// TODO:
// - pimp up the queue with a custom allocator type
// - not use std::array.at(), use instead plain array indexing

/// \brief A multi-producer single-consumer queue
/// \details The queue is implemented as a ringbuffer
/// and atomic head/tail pointers, it is thread safe, unfortunately there is no size function
template<typename T, std::size_t C> class MpscQueue
{
public:
    MpscQueue() : m_buffer_mask(C)
    {
        for (std::size_t i{ 0 }; i < C; i++) { m_data[i].sequence.store(i); }
        LOG_INFO("head is lock free: {}", m_head.is_lock_free());
    }

    /// \brief pushing back a single element
    QueueState PushBack(const T &element)
    {
        auto pos = m_head.load();
        while (true) {
            const auto seq = m_data.at(pos % m_buffer_mask).sequence.load();
            const std::int64_t diff = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos);
            if (diff == 0L) {
                if (m_head.compare_exchange_strong(pos, pos + 1)) { break; }
            } else if (diff < 0L) {
                LOG_DEBUG("queue is full");
                return QueueState::QUEUE_IS_FULL;
            } else {
                pos = m_head.load();
            }
        }

        m_data.at(pos % m_buffer_mask).data = element;
        m_data.at(pos % m_buffer_mask).sequence.store(pos + 1);
        m_pushCount.fetch_add(1, std::memory_order_release);
        return QueueState::SUCCESS;
    }
    /// \brief pushing back multiple elements to the queue
    void PushBack(const std::vector<T> elements) {}
    /// \brief Pops out all the elements from the queue using a single read
    std::expected<T, QueueState> Pop()
    {
        auto pos = m_tail.load();
        while (true) {
            const auto seq = m_data[pos % m_buffer_mask].sequence.load();
            const std::int64_t diff = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos + 1);
            if (diff == 0L) {
                if (m_tail.compare_exchange_strong(pos, pos + 1)) { break; }
            } else if (diff < 0L) {
                return QueueState::QUEUE_IS_EMPTY;
            } else {
                pos = m_tail.load();
            }
        }
        const auto data = m_data[pos % m_buffer_mask].data;
        m_data[pos % m_buffer_mask].sequence.store(pos + m_buffer_mask); // TODO: this might be a bug, the sequence should be incremented by the number of elements pushed
        m_popCount.fetch_add(1, std::memory_order_release);
        return data;
    }

    /// \brief Get the number of elements pushed to the queue, this is best-effort, not guaranteed to be accurate
    std::size_t GetPushCount() const { return m_pushCount.load(); }

    /// \brief Get the number of elements popped from the queue, this is best-effort, not guaranteed to be accurate
    std::size_t GetPopCount() const { return m_popCount.load(); }

    struct Cell
    {
        T data;
        std::atomic<std::size_t> sequence;
    };

private:
    const std::size_t m_buffer_mask;
    std::array<Cell, C> m_data{};
    std::atomic<std::size_t> m_head{ 0 }, m_tail{ 0 };
    std::atomic<std::size_t> m_pushCount{ 0 }, m_popCount{ 0 };
};
}// namespace sigmax
