#include "include/core/SkExecutor.h"
#include "include/gpu/graphite/ContextOptions.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <mutex>

namespace {

constexpr int kMaximumExperimentWorkers = 4;

[[nodiscard]] int bounded_worker_count(int requested) noexcept {
    return std::clamp(requested, 1, kMaximumExperimentWorkers);
}

class GraphiteExecutorExperiment {
public:
    static GraphiteExecutorExperiment enabled(int requested_workers) {
        const int workers = bounded_worker_count(requested_workers);
        return GraphiteExecutorExperiment{
            workers,
            SkExecutor::MakeFIFOThreadPool(workers, false)};
    }

    static GraphiteExecutorExperiment unavailable() noexcept {
        return GraphiteExecutorExperiment{0, {}};
    }

    GraphiteExecutorExperiment(GraphiteExecutorExperiment&&) noexcept = default;
    GraphiteExecutorExperiment& operator=(GraphiteExecutorExperiment&&) noexcept = default;

    GraphiteExecutorExperiment(const GraphiteExecutorExperiment&) = delete;
    GraphiteExecutorExperiment& operator=(const GraphiteExecutorExperiment&) = delete;

    void configure(skgpu::graphite::ContextOptions& options) const noexcept {
        options.fExecutor = executor_.get();
    }

    [[nodiscard]] SkExecutor* executor() const noexcept {
        return executor_.get();
    }

    [[nodiscard]] int worker_count() const noexcept {
        return worker_count_;
    }

private:
    GraphiteExecutorExperiment(int worker_count,
                               std::unique_ptr<SkExecutor> executor) noexcept
        : worker_count_(worker_count),
          executor_(std::move(executor)) {}

    int worker_count_{};
    std::unique_ptr<SkExecutor> executor_;
};

class Countdown {
public:
    explicit Countdown(std::size_t count) noexcept : remaining_(count) {}

    void arrive() noexcept {
        std::lock_guard lock{mutex_};
        if (remaining_ > 0) {
            --remaining_;
        }
        condition_.notify_all();
    }

    [[nodiscard]] bool wait() noexcept {
        std::unique_lock lock{mutex_};
        return condition_.wait_for(
            lock,
            std::chrono::seconds{5},
            [this] { return remaining_ == 0; });
    }

private:
    std::mutex mutex_;
    std::condition_variable condition_;
    std::size_t remaining_{};
};

[[nodiscard]] bool schedule_and_wait(SkExecutor* executor,
                                     std::size_t task_count) {
    if (!executor) {
        return false;
    }

    Countdown countdown{task_count};
    for (std::size_t index = 0; index < task_count; ++index) {
        executor->add([&countdown] { countdown.arrive(); });
    }
    return countdown.wait();
}

bool require(bool condition, const char* message) {
    if (condition) {
        return true;
    }
    std::fprintf(stderr, "%s\n", message);
    return false;
}

} // namespace

int main() {
    bool ok = true;

    skgpu::graphite::ContextOptions defaults;
    ok &= require(defaults.fExecutor == nullptr,
                  "default Graphite options must remain serial");

    auto bounded = GraphiteExecutorExperiment::enabled(64);
    ok &= require(bounded.worker_count() == kMaximumExperimentWorkers,
                  "executor worker count must remain bounded");
    ok &= require(bounded.executor() != nullptr,
                  "executor creation must provide the experimental worker pool");

    skgpu::graphite::ContextOptions enabled_options;
    bounded.configure(enabled_options);
    ok &= require(enabled_options.fExecutor == bounded.executor(),
                  "Graphite options must reference the instance-owned executor");
    ok &= require(schedule_and_wait(bounded.executor(), 16),
                  "instance-owned executor must execute queued work");

    auto survivor = GraphiteExecutorExperiment::enabled(2);
    SkExecutor* survivor_executor = survivor.executor();
    ok &= require(survivor_executor != nullptr,
                  "survivor executor creation must succeed");

    {
        auto disposable = GraphiteExecutorExperiment::enabled(1);
        ok &= require(disposable.executor() != nullptr,
                      "disposable executor creation must succeed");
        ok &= require(disposable.executor() != survivor_executor,
                      "independent experiments must not share executor ownership");
        ok &= require(schedule_and_wait(disposable.executor(), 4),
                      "disposable executor must drain queued work before teardown");
    }

    ok &= require(schedule_and_wait(survivor.executor(), 8),
                  "destroying one executor must not affect another instance");

    auto unavailable = GraphiteExecutorExperiment::unavailable();
    skgpu::graphite::ContextOptions fallback_options;
    unavailable.configure(fallback_options);
    ok &= require(fallback_options.fExecutor == nullptr,
                  "unavailable executor must preserve Graphite serial fallback");

    for (int iteration = 0; iteration < 16; ++iteration) {
        auto experiment = GraphiteExecutorExperiment::enabled(1 + (iteration % 4));
        ok &= require(experiment.executor() != nullptr,
                      "repeated executor creation must succeed");
        ok &= require(schedule_and_wait(experiment.executor(), 2),
                      "repeated executor teardown must drain completed work");
    }

    return ok ? 0 : 1;
}
