#include "order_type.hpp"

#include <map>

#include "log.hpp"
#include "helpers.hpp"
#include "sigmax_exit_codes.hpp"

namespace sigmax
{

Symbol StrToSymbol(const std::string_view& symbol_str)
{
    static const std::map<Symbol, std::string> symbol_map{ { Symbol::BNBBTC, "BNBBTC" }, { Symbol::BNBUSDT, "BNBUSDT" } };

    for (const auto &key : symbol_map) {
        if (key.second == symbol_str) { return key.first; }
    }
  
  /// Failed to find requested symbol, something is got really wrong -> exiting
  LOG_FATAL("Wrong or not registered symbol: {}", symbol_str);
  std::_Exit(AsInt(SigmaxExitCodes::BINANCE_API_ERROR));
}

}
