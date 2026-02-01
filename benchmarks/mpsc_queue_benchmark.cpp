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

template<typename QueueSize> bool MpscQueueBenchmark::RunBenchmark(int producerCount)
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

    nlohmann::json singleBenchmarkResult;
    singleBenchmarkResult["producerCount"] = producerCount;
    singleBenchmarkResult["queueSize"] = QueueSize::value * sizeof(typename QueueType::value_type);
    QueueType queue;
    std::promise<void> go;
    std::shared_future<void> ready(go.get_future().share());
    std::vector<std::future<void>> writerFutures;
    bool stop = false;
    for (int i{ 0 }; i < producerCount; i++) {
        writerFutures.emplace_back(std::async(std::launch::async, writer, std::ref(queue), std::ref(ready), std::ref(stop)));
    }
    auto readerFut = std::async(std::launch::async, reader, std::ref(queue), std::ref(ready), std::ref(stop));
    go.set_value();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop = true;
    auto [popCount, successfulPops] = readerFut.get();
    singleBenchmarkResult["totalPops"] = popCount;
    singleBenchmarkResult["successfulPops"] = successfulPops;
    LOG_INFO("Benchmark config: producers: {}, queue size: {}", producerCount, QueueSize::value);
    LOG_INFO("Total pops: {}, successful pops: {}", popCount, successfulPops);


    if (!SaveBenchmarkResults(singleBenchmarkResult)) {
        LOG_ERROR("Failed to save benchmark results");
        return false;
    }
    return true;
}

bool MpscQueueBenchmark::SaveBenchmarkResults(const nlohmann::json &benchmarkResult) const
{
    nlohmann::json finalBenchmarkResults;
    finalBenchmarkResults["benchmarkResults"] = benchmarkResult;
    if (std::filesystem::exists(m_benchmarkResultsPath)) {
        std::ifstream in(m_benchmarkResultsPath);
        nlohmann::json existingBenchmarkResults;
        in >> existingBenchmarkResults;
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
    program.add_argument("-p", "--producer-count")
        .help("Producer count, possible values: 1, 2, 4, 8, 16, 32, 64")
        .default_value(1)
        .scan<'i', int>()
        .required();
    program.add_argument("-r", "--results-path")
        .help("Path to write benchmark results JSON")
        .default_value(std::string("results/benchmark_results_q${queueSize}_p${producerCount}.json"))
        .required();
    program.add_epilog("Example: benchmark_test -q 32 -p 1 -r results/benchmark_results_q${queueSize}_p${producerCount}.json");
    program.add_description("Benchmark the MPSC queue");
    try {
        program.parse_args(argc, argv);
    } catch (const std::exception &e) {
        LOG_ERROR("Error: {}", e.what());
        return 1;
    }

    std::filesystem::path resultsPath = program.get<std::string>("--results-path");
    MpscQueueBenchmark benchmark(resultsPath);
    int queueSize = program.get<int>("--queue-size");
    int producerCountValue = program.get<int>("--producer-count");
    bool result = false;
    switch (queueSize) {
    case 32:
        result = benchmark.RunBenchmark<std::integral_constant<int, 32>>(producerCountValue);
        break;
    case 64:
        result = benchmark.RunBenchmark<std::integral_constant<int, 64>>(producerCountValue);
        break;
    case 128:
        result = benchmark.RunBenchmark<std::integral_constant<int, 128>>(producerCountValue);
        break;
    case 256:
        result = benchmark.RunBenchmark<std::integral_constant<int, 256>>(producerCountValue);
        break;
    case 512:
        result = benchmark.RunBenchmark<std::integral_constant<int, 512>>(producerCountValue);
        break;
    case 1024:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024>>(producerCountValue);
        break;
    case 1024 * 2:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024 * 2>>(producerCountValue);
        break;
    case 1024 * 4:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024 * 4>>(producerCountValue);
        break;
    case 1024 * 8:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024 * 8>>(producerCountValue);
        break;
    case 1024 * 10:
        result = benchmark.RunBenchmark<std::integral_constant<int, 1024 * 10>>(producerCountValue);
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
