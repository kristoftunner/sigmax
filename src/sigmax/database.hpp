#pragma once
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

#include "mpsc_queue.hpp"
#include "order_type.hpp"

namespace sigmax {

enum class DBErrorType : std::uint8_t { SUCCESS, UPDATE_FAILED, FILE_ACESS_ERROR, INSTRUMENT_NOT_FOUND, TIMESTAMP_RANGE_NOT_FOUND };
class DataBase
{
public:
    // locking DB write function
    DBErrorType UpdateDb(const Order &&order);
    DBErrorType SaveDbToFile(const std::filesystem::path &filePath);
    // one-copy DB read functions
    std::expected<const std::vector<Order>, DBErrorType> GetOrders(const InstrumentId &instrumentId);
    std::expected<const std::vector<Order>, DBErrorType>
        GetOrders(const InstrumentId &instrumentId, const Timestamp start, const Timestamp end);

    DBErrorType AppendCallbackFn(std::function<void(const Order &order)>);

private:
    static constexpr int kInputQueueSize{ 1024 };
    std::vector<std::function<void(const Order &order)>> m_algoCallbackFns;
    // TODO: improve the perf using an isntrument id enum and vectors instead of maps
    std::map<InstrumentId, std::vector<Order>> m_orders;
    std::map<InstrumentId, std::mutex> m_instrumentLocks;
    MpscQueue<Order, kInputQueueSize> m_queue;
};
}// namespace sigmax
