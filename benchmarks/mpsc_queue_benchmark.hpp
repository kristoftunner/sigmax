#pragma once

#include <filesystem>
#include <vector>

#include <nlohmann/json.hpp>

#include "benchmark_utils.hpp"
#include "log.hpp"

namespace sigmax {
class MpscQueueBenchmark
{
public:
    struct BenchmarkConfig
    {
        int numberOfThreads;
        int queueSize;
    };
    MpscQueueBenchmark(const std::filesystem::path &benchmarkResultsPath);
    ~MpscQueueBenchmark() = default;

    template<typename QueueSize> bool RunBenchmark(const std::vector<int> &producerCount);

private:
    bool SaveBenchmarkResults(const std::vector<nlohmann::json> &benchmarkResults) const;

    std::filesystem::path m_benchmarkResultsPath;
    CpuInfo m_cpuInfo;
};


}// namespace sigmax