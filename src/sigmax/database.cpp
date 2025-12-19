#include "database.hpp"

#include <algorithm>
#include <filesystem>
#include <mutex>

#include "instruments.hpp"
#include "order_type.hpp"

namespace sigmax {
DBErrorType DataBase::UpdateDb(const Order &&order)
{
    const std::lock_guard lockGuard(m_dbLock);
    // insert the order into the orders
    if (m_orders.contains(order.instrumentId)) {
        m_orders[order.instrumentId].emplace_back(order);

    } else {
        m_orders[order.instrumentId] = { order };
        m_instrumentLocks[order.instrumentId] = std::mutex();
    }
    // sort the orders by timestamp
    std::sort(m_orders[order.instrumentId].begin(), m_orders[order.instrumentId].end(), [](const Order &lhs, const Order &rhs) {
        return lhs.ts < rhs.ts;
    });

    return DBErrorType::SUCCESS;
}
DBErrorType DataBase::SaveDbToFile(const std::filesystem::path &filePath)
{
    if (std::filesystem::exists(filePath)) {
        return DBErrorType::SUCCESS;
    } else {
        return DBErrorType::FILE_ACESS_ERROR;
    }
}

std::expected<const std::vector<Order>, DBErrorType> DataBase::GetOrders(const InstrumentId &instrumentId)
{
    if (!m_orders.contains(instrumentId)) {
        return std::unexpected(DBErrorType::INSTRUMENT_NOT_FOUND);
    } else {
        return m_orders[instrumentId];
    }
}

std::expected<const std::vector<Order>, DBErrorType>
    DataBase::GetOrders(const InstrumentId &instrumentId, const Timestamp start, const Timestamp end)
{
    if (!m_orders.contains(instrumentId)) {
        return std::unexpected(DBErrorType::INSTRUMENT_NOT_FOUND);
    } else {
        std::vector<Order> ret;
        const std::vector<Order> &orders = m_orders[instrumentId];
        const auto startOrder = std::lower_bound(
            orders.begin(), orders.end(), start, [=](const Order &order, const Timestamp timestamp) { return timestamp < order.ts; });
        const auto endOrder = std::lower_bound(
            orders.begin(), orders.end(), end, [=](const Order &order, const Timestamp timestamp) { return timestamp < order.ts; });
        if ((startOrder != orders.end()) && (endOrder != orders.end())) {
            return std::vector<Order>(startOrder, endOrder);
        } else {
            return std::unexpected(DBErrorType::TIMESTAMP_RANGE_NOT_FOUND);
        }
    }
}

}// namespace sigmax
