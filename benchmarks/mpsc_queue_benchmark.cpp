#include "mpsc_queue_benchmark.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
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
    using QueueType = MpscQueue<BookEvent, QueueSize::value>;
    using Counters = std::pair<std::size_t, std::size_t>;// {attempts, successes}

    // constantly writing and reading from the queue
    auto writer = [](QueueType &queue, std::shared_future<void> &ready, const std::atomic<bool> &stop) -> Counters {
        BookEvent event{ .event_ts = 0,
            .symbol = Symbol::BNBBTC,
            .first_update_id = 0,
            .final_update_id = 0,
            .bids = { BidsAsks{ .quantity = 100, .price = 100 } },
            .asks = { BidsAsks{ .quantity = 100, .price = 101 } } };
        ready.wait();
        std::size_t pushCount{ 0 };
        std::size_t successfulPushes{ 0 };
        while (!stop.load(std::memory_order_relaxed)) {
            event.event_ts++;
            event.first_update_id++;
            event.final_update_id = event.first_update_id;
            pushCount++;
            if (queue.PushBack(event) == QueueRet::SUCCESS) { successfulPushes++; }
        }
        return { pushCount, successfulPushes };
    };
    auto reader = [](QueueType &queue, std::shared_future<void> &ready, const std::atomic<bool> &stop) -> Counters {
        ready.wait();
        std::size_t popCount{ 0 };
        std::size_t successfulPops{ 0 };
        while (!stop.load(std::memory_order_relaxed)) {
            auto ret = queue.Pop();
            popCount++;
            if (ret.has_value()) { successfulPops++; }
        }
        return { popCount, successfulPops };
    };

    nlohmann::json singleBenchmarkResult;
    singleBenchmarkResult["producerCount"] = producerCount;
    singleBenchmarkResult["queueCapacity"] = QueueSize::value;
    singleBenchmarkResult["queueSize"] = QueueSize::value * sizeof(BookEvent);
    QueueType queue;
    std::promise<void> go;
    std::shared_future<void> ready(go.get_future().share());
    std::vector<std::future<Counters>> writerFutures;
    std::atomic<bool> stop{ false };
    for (int i{ 0 }; i < producerCount; i++) {
        writerFutures.emplace_back(std::async(std::launch::async, writer, std::ref(queue), std::ref(ready), std::cref(stop)));
    }
    auto readerFut = std::async(std::launch::async, reader, std::ref(queue), std::ref(ready), std::cref(stop));
    go.set_value();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    stop.store(true, std::memory_order_relaxed);
    auto [popCount, successfulPops] = readerFut.get();
    std::size_t pushCount{ 0 };
    std::size_t successfulPushes{ 0 };
    for (auto &writerFuture : writerFutures) {
        auto [attempts, successes] = writerFuture.get();
        pushCount += attempts;
        successfulPushes += successes;
    }

    singleBenchmarkResult["totalPushes"] = pushCount;
    singleBenchmarkResult["successfulPushes"] = successfulPushes;
    singleBenchmarkResult["totalPops"] = popCount;
    singleBenchmarkResult["successfulPops"] = successfulPops;
    singleBenchmarkResult["queuePushCount"] = queue.GetPushCount();
    singleBenchmarkResult["queuePopCount"] = queue.GetPopCount();
    LOG_INFO("Benchmark config: producers: {}, queue size: {}", producerCount, QueueSize::value);
    LOG_INFO("Total pushes: {}, successful pushes: {}", pushCount, successfulPushes);
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
