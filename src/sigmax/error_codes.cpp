#include "error_codes.hpp"

#include <map>
#include <string>

namespace sigmax {

std::string to_string(SigmaxReturnCodes code) {
    static const std::map<SigmaxReturnCodes, std::string> names = {
        { SigmaxReturnCodes::SUCCESS,                     "SUCCESS"                     },
        { SigmaxReturnCodes::MARKET_API_FAILURE,          "MARKET_API_FAILURE"          },
        { SigmaxReturnCodes::MARKET_API_CONNECTION_ERROR, "MARKET_API_CONNECTION_ERROR" },
    };
    const auto it = names.find(code);
    return it != names.end() ? it->second : "UNKNOWN";
}

}  // namespace sigmax
