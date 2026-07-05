#pragma once

#include <string>

namespace sigmax {

enum class SigmaxReturnCodes {
    // clang-format off
    SUCCESS                     = 0,
    MARKET_API_FAILURE          = 10,
    MARKET_API_CONNECTION_ERROR = 11,
    // clang-format on
};

std::string to_string(SigmaxReturnCodes code);

}// namespace sigmax
