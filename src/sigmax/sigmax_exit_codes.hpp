#pragma once

namespace sigmax {
/// @brief Unix convention 0 - success, 1-127 for internal errors, 128-255 for signals
/// @details When adding new exit codes, DO NOT CHANGE THE EXISTING ONES
enum class SigmaxExitCodes : int {
    SUCCESS = 0,

    BINANCE_API_ERROR = 10,
};
}// namespace sigmax
