#include "mpsc_queue_benchmark.hpp"
#include <chrono>
#include <future>
#include <thread>

#include "mpsc_queue.hpp"
#include "order_type.hpp"

namespace sigmax {
MpscQueueBenchmark::MpscQueueBenchmark() { Logger::Init(); }

MpscQueueBenchmark::~MpscQueueBenchmark() {}

void MpscQueueBenchmark::MPSCBenchmark()
{
    static constexpr int kNumberOfThreads = 8;
    static constexpr int kQueueSize = 1024;
    using QueueType = MpscQueue<Order, kQueueSize>;

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

    std::promise<void> go;
    std::shared_future<void> ready(go.get_future().share());

    std::vector<std::future<void>> writerFutures;

    bool stop = false;
    QueueType queue;
    for (int i{ 0 }; i < kNumberOfThreads; i++) {
        writerFutures.emplace_back(std::async(std::launch::async, writer, std::ref(queue), std::ref(ready), std::ref(stop)));
    }

    auto readerFut = std::async(std::launch::async, reader, std::ref(queue), std::ref(ready), std::ref(stop));
    go.set_value();

    std::this_thread::sleep_for(std::chrono::seconds(1));
    stop = true;
    auto [popCount, successfulPops] = readerFut.get();
    LOG_INFO("Pops: {}, successful pops: {}", popCount, successfulPops);
}
bool MpscQueueBenchmark::run()
{
    MPSCBenchmark();
    return true;
}

}// namespace sigmax

int main()
{
    using namespace sigmax;
    MpscQueueBenchmark benchmark;
    auto result = benchmark.run();
    if (!result) {
        LOG_ERROR("Benchmark failed");
        return 1;
    }
    return 0;
}
