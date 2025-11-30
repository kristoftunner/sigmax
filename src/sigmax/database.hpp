#include <vector>
#include <functional>
#include <filesystem>
#include <expected>

#include "order_type.hpp"

namespace sigmax
{

enum class DBErrorType:std::uint8_t
{
  SUCCESS,
  UPDATE_FAILED
};
class DataBase
{
public:
  DBErrorType UpdateDb(const Order&& order);
  DBErrorType SaveDbToFile(const std::filesystem::path& filePath);
  std::expected<const std::vector<Order>&&, DBErrorType> GetOrders(const std::string& instrumentId);
  std::expected<const std::vector<Order>&&, DBErrorType> GetOrders(const orderId);
  std::expected<const std::vector<Order>&&, DBErrorType> GetOrders(const std::string& instrumentId, const Timestamp start, const Timestamp end);


private:
  std::vector<std::function<void()>> m_algoCallbackFns;
};
} // namespace sigmax
