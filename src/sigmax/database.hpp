#pragma once
#include <expected>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

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
    std::expected<const std::vector<Order>, DBErrorType> GetOrders(const std::string &instrumentId);
    std::expected<const std::vector<Order>, DBErrorType> GetOrders(const OrderId orderId);
    std::expected<const std::vector<Order>, DBErrorType>
        GetOrders(const std::string &instrumentId, const Timestamp start, const Timestamp end);

    DBErrorType AppendCallbackFn(std::function<void(const Order &order)>);

private:
    std::vector<std::function<void(const Order &order)>> m_algoCallbackFns;
    std::map<std::string, std::vector<Order>> m_orders;
    std::mutex m_dbLock;
};
}// namespace sigmax
