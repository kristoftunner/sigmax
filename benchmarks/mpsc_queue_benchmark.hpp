#pragma once

#include "log.hpp"
#include <vector>

namespace sigmax {
class MpscQueueBenchmark
{
public:
    struct BenchmarkConfig
    {
        int numberOfThreads;
        int queueSize;
    };
    MpscQueueBenchmark();
    ~MpscQueueBenchmark() = default;

    template<typename QueueSize>
    bool RunBenchmark(const std::vector<int> &producerCount);
};


}// namespace sigmax