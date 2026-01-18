#pragma once

#include "log.hpp"

namespace sigmax {
class MpscQueueBenchmark
{
public:
    MpscQueueBenchmark();
    ~MpscQueueBenchmark();

    bool run();

private:
    void MPSCBenchmark();
};


}// namespace sigmax