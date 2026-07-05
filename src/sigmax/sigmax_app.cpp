#include <cstdlib>
#include <iostream>
#include <thread>

#include "binance_api.hpp"
#include "sigmax_exit_codes.hpp"
#include "log.hpp"

using namespace sigmax;

int main()
{
    Logger::Init();
    const std::vector<std::string> instruments{ "bnbusdc", "tlmusdc", "ethusdc" };
    BinanceApi api(instruments);
    if (api.Connect() != BinanceApi::ApiReturn::SUCCESS) { std::_Exit(static_cast<int>(SigmaxExitCodes::BINANCE_API_ERROR)); }

    std::thread binanceApiTh([&]() {
        while (true) { api.Read(); }
    });

    return 0;
}
