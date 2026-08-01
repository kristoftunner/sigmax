#pragma once

#include <array>
#include <atomic>
#include <boost/asio/detail/impl/socket_ops.ipp>
#include <cstdint>
#include <expected>

#include <fmt/base.h>
#include <tracy/Tracy.hpp>

#include "log.hpp"

namespace sigmax {

enum class QueueRet : std::uint8_t { SUCCESS, QUEUE_IS_EMPTY, QUEUE_IS_FULL, INPUT_ERROR, INTERNAL_ERROR };
// TODO:
// - pimp up the queue with a custom allocator type
// - not use std::array.at(), use instead plain array indexing

/// \brief A multi-producer single-consumer queue
/// \details The queue is implemented as a ringbuffer
/// and atomic head/tail pointers, it is thread safe, unfortunately there is no size function
template<std::size_t C> class MpscQueue
{
public:
    explicit MpscQueue(const std::size_t element_size) : buffer_mask_(C), element_size_(element_size), data_(element_size * C)
    {
        for (std::size_t i{ 0 }; i < C; i++) { sequences_[i].sequence.store(i, std::memory_order_relaxed); }

        LOG_INFO("head is lock free: {}", head_.is_lock_free());
    }

    /// \brief pushing back a single element
    QueueRet PushBack(const std::span<const std::byte> data)
    {
        if (data.size() != element_size_) {
            LOG_ERROR("Wrong input data size: {}, expected: {}", data.size(), element_size_);
            return QueueRet::INPUT_ERROR;
        }
        ZoneScopedN("MpscQueue::Push");
        auto pos = head_.load(std::memory_order_acquire);
        while (true) {
            const auto seq = sequences_.at(pos % buffer_mask_).sequence.load(std::memory_order_acquire);
            const std::int64_t diff = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos);
            if (diff == 0L) {
                if (head_.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) { break; }
            } else if (diff < 0L) {
                LOG_DEBUG("queue is full");
                return QueueRet::QUEUE_IS_FULL;
            } else {
                pos = head_.load(std::memory_order_acquire);
            }
        }
        

        const std::size_t offset = (pos % buffer_mask_) * element_size_;
        if(data_.insert(data_.cbegin() + offset, data.begin(), data.end()) == data_.end())
        {
            LOG_ERROR("Failed to insert data with size:{}", data.size());
            return QueueRet::INTERNAL_ERROR;
        }
        sequences_.at(pos % buffer_mask_).store(pos + 1, std::memory_order_release);
        push_count_.fetch_add(1, std::memory_order_relaxed);
        return QueueRet::SUCCESS;
    }

    /// \brief Pops out all the elements from the queue using a single read
    std::expected<std::vector<std::byte>, QueueRet> Pop()
    {
        ZoneScopedN("MpscQueue::Pop");
        auto pos = tail_.load(std::memory_order_acquire);
        while (true) {
            const auto seq = sequences_[pos % buffer_mask_].load(std::memory_order_acquire);
            const std::int64_t diff = static_cast<std::int64_t>(seq) - static_cast<std::int64_t>(pos + 1);
            if (diff == 0L) {
                if (tail_.compare_exchange_weak(pos, pos + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) { break; }
            } else if (diff < 0L) {
                return std::unexpected(QueueRet::QUEUE_IS_EMPTY);
            } else {
                pos = tail_.load(std::memory_order_acquire);
            }
        }

        const auto cit{ data_.begin() + pos % buffer_mask_ * element_size_ };
        std::vector<std::byte> data(cit, cit + element_size_);
        sequences_[pos % buffer_mask_].store(pos + buffer_mask_,
            std::memory_order_release);// TODO: this might be a bug, the sequence should be incremented by the number of elements pushed
        pop_count_.fetch_add(1, std::memory_order_relaxed);
        return data;
    }

    /// \brief Get the number of elements pushed to the queue, this is best-effort, not guaranteed to be accurate
    [[nodiscard]] std::size_t GetPushCount() const { return push_count_.load(std::memory_order_relaxed); }

    /// \brief Get the number of elements popped from the queue, this is best-effort, not guaranteed to be accurate
    [[nodiscard]] std::size_t GetPopCount() const { return pop_count_.load(std::memory_order_relaxed); }

private:
    const std::size_t buffer_mask_;
    const std::size_t element_size_;
    std::vector<std::byte> data_;
    std::array<std::atomic<std::size_t>, C> sequences_;
    std::atomic<std::size_t> head_, tail_;
    std::atomic<std::size_t> push_count_, pop_count_;
};
}// namespace sigmax
