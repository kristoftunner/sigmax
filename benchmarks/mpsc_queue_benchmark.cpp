#include "mpsc_queue_benchmark.hpp"

#include <chrono>
#include <filesystem>
#include <future>
#include <thread>

#include <argparse/argparse.hpp>

#include "mpsc_queue.hpp"
#include "order_type.hpp"


namespace sigmax {
MpscQueueBenchmark::MpscQueueBenchmark(const std::filesystem::path &benchmarkResultsPath) : m_benchmarkResultsPath(benchmarkResultsPath)
{
    Logger::Init();
    m_cpuInfo.QueryCpuInfo();
}

template<typename QueueSize> bool MpscQueueBenchmark::RunBenchmark(const std::vector<int> &producerCount)
{
    using QueueType = MpscQueue<Order, QueueSize::value>;
    // constantly writing and reading from the queue
    auto writer = [](QueueType &queue, std::shared_future<void> &ready, bool &stop) {
        ready.wait();
        int i{ 0 };
        while (!stop) { queue.PushBack(Order{ i++, "AAPL", OrderSide::BUY, OrderState::NEW, 100, 100, 1000000000000000000 }); }
    };
    auto reader = [](QueueType &queue, std::shared_future<void> &ready, bool &stop) -> std::pair<int, int> {
        ready.wait();
        int counter{ 0 };
        int successfullPops{ 0 };
        while (!stop) {
            auto ret = queue.Pop();
            counter++;
            if (ret.has_value()) { successfullPops++; }
        }
        return { counter, successfullPops };
    };

    std::vector<nlohmann::json> benchmarkResults;
    for (const auto &count : producerCount) {
        nlohmann::json singleBenchmarkResult;
        singleBenchmarkResult["producerCount"] = count;
        singleBenchmarkResult["queueSize"] = QueueSize::value * sizeof(typename QueueType::value_type);
        QueueType queue;
        std::promise<void> go;
        std::shared_future<void> ready(go.get_future().share());
        std::vector<std::future<void>> writerFutures;
        bool stop = false;
        for (int i{ 0 }; i < count; i++) {
            writerFutures.emplace_back(std::async(std::launch::async, writer, std::ref(queue), std::ref(ready), std::ref(stop)));
        }
        auto readerFut = std::async(std::launch::async, reader, std::ref(queue), std::ref(ready), std::ref(stop));
        go.set_value();

        std::this_thread::sleep_for(std::chrono::seconds(1));
        stop = true;
        auto [popCount, successfulPops] = readerFut.get();
        singleBenchmarkResult["totalPops"] = popCount;
        singleBenchmarkResult["successfulPops"] = successfulPops;
        benchmarkResults.push_back(singleBenchmarkResult);
        LOG_INFO("Benchmark config: producers: {}, queue size: {}", count, QueueSize::value);
        LOG_INFO("Total pops: {}, successful pops: {}", popCount, successfulPops);
    }


    if (!SaveBenchmarkResults(benchmarkResults)) {
        LOG_ERROR("Failed to save benchmark results");
        return false;
    }
    return true;
}

bool MpscQueueBenchmark::SaveBenchmarkResults(const std::vector<nlohmann::json> &benchmarkResults) const
{
    nlohmann::json finalBenchmarkResults;
    finalBenchmarkResults["benchmarkResults"] = benchmarkResults;
    if (std::filesystem::exists(m_benchmarkResultsPath)) {
        std::ifstream in(m_benchmarkResultsPath);
        nlohmann::json existingBenchmarkResults;
        in >> existingBenchmarkResults;
        for (const auto &result : existingBenchmarkResults["benchmarkResults"]) {
            finalBenchmarkResults["benchmarkResults"].push_back(result);
        }
    }
    finalBenchmarkResults["cpuInfo"] = m_cpuInfo.ToJson();
    std::ofstream out(m_benchmarkResultsPath);
    if (!out) {
        LOG_ERROR("Failed to open file for saving benchmark results");
        return false;
    }
    out << finalBenchmarkResults.dump(4);
    out.close();
    LOG_INFO("Benchmark results saved to {}", m_benchmarkResultsPath.string());
    return true;
}


}// namespace sigmax

using namespace sigmax;

int main(int argc, char *argv[])
{
    argparse::ArgumentParser program("benchmark_test");
    program.add_argument("-q", "--queue-size")
        .help("Queue size, possible values: 32, 64, 128, 256, 512, 1024, 1024*2, 1024*4, 1024*8, 1024*10")
        .default_value(32)
        .scan<'i', int>();
    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &e) {
        LOG_ERROR("Error: {}", e.what());
        return 1;
    }

    std::vector<int> producerCount = { 1, 2, 4, 8, 16, 32, 64 };
    MpscQueueBenchmark benchmark(program.get<std::filesystem::path>("--results-path"));
    int queueSize = program.get<int>("--queue-size");

    bool result = false;
    switch (queueSize) {
    case 32:
        result = benchmark.RunBenchmark<std::integral_constant<int, 32>>(producerCount);
        break;
    case 64:
        result = benchmark.RunBenchmark<std::integral_constant<int, 64>>(producerCount);
        break;
    case 128:
        result = benchmark.RunBenchmark<std::integral_constant<int, 128>>(producerCount);
        break;
    case 256:
        result = benchmark.RunBenchmark<std::integral_constant<int, 256>>(producerCount);
        break;
    case 512:
        result = benchmark.RunBenchmark<std::integral_constant<int, 512>>(producerCount);
        break;
    case 1024:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024>>(producerCount);
        break;
    case 1024 * 2:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024 * 2>>(producerCount);
        break;
    case 1024 * 4:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024 * 4>>(producerCount);
        break;
    case 1024 * 8:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024 * 8>>(producerCount);
        break;
    case 1024 * 10:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024 * 10>>(producerCount);
        break;
    default:
        LOG_ERROR("Invalid queue size");
        return 1;
    }

    if (!result) {
        LOG_ERROR("Benchmark failed");
        return 1;
    }
    return 0;
}
