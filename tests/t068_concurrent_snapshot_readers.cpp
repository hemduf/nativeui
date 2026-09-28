// T068 Batch 7b — concurrent old/new immutable snapshot reader coverage.
//
// One owning writer publishes a bounded, closed generation sequence while
// several reader threads repeatedly load the atomically published immutable
// snapshot. Readers must observe a monotonic, gap-free prefix of generations,
// keep early generations alive across every later publication, and still read
// their exact content after the writer stops and the publication endpoint is
// retired.
//
// Overlap is proven deterministically, not by timing: generations
// 1..kHandshakeGenerations are published in lockstep, and the writer waits
// until every reader has observed each of those generations before publishing
// the next. Scheduling can change how many extra generations a reader sees,
// but it cannot suppress any handshake generation or remove the documented
// minimum reader/publisher overlap.
//
// Sanitizer limitation: this repository configures ASan+UBSan
// (`NATIVEUI_ENABLE_SANITIZERS`), not TSan. ASan/UBSan cannot detect data
// races, so this suite does not claim race-freedom from sanitizer evidence; the
// race-freedom contract rests on the documented atomic shared_ptr
// publication/retirement and on these deterministic consistency assertions.

#include <nativeui/detail/semantic_native_publication.hpp>
#include <nativeui/detail/semantic_snapshot.hpp>

#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

void check_with_problem(bool condition,
                        const std::string& problem,
                        const char* expression,
                        int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression +
                                 (problem.empty() ? std::string{}
                                                  : " (" + problem + ")"));
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

constexpr std::uint64_t kRootId = 42;
constexpr std::size_t kReaderCount = 6;
constexpr std::uint64_t kTotalGenerations = 512;
// Minimum number of distinct generations every reader provably observes while
// the writer is still publishing. Kept below 64 so one 64-bit mask proves
// gap-free observation per reader.
constexpr std::uint64_t kHandshakeGenerations = 48;
constexpr std::uint64_t kCompleteHandshakeMask =
    (std::uint64_t{1} << kHandshakeGenerations) - 1U;
constexpr auto kHandshakeTimeout = std::chrono::seconds(30);

static_assert(kHandshakeGenerations < std::uint64_t{64},
              "the observed-generation mask is one 64-bit word");
static_assert(kHandshakeGenerations < kTotalGenerations,
              "the handshake prefix must stay shorter than the full sequence");

[[nodiscard]] std::string generation_name(std::uint64_t generation) {
    return "semantic-generation-" + std::to_string(generation);
}

[[nodiscard]] double generation_value(std::uint64_t generation) noexcept {
    return static_cast<double>(generation) * 3.0 + 0.25;
}

[[nodiscard]] float generation_scale(std::uint64_t generation) noexcept {
    return 1.0f + static_cast<float>(generation) * 0.001f;
}

[[nodiscard]] ui::detail::SemanticNativeGeometry generation_geometry(
    std::uint64_t generation) noexcept {
    return ui::detail::SemanticNativeGeometry{
        generation_scale(generation),
        {static_cast<float>(generation), -static_cast<float>(generation)}};
}

[[nodiscard]] ui::SemanticTreeSnapshot make_generation_snapshot(
    std::uint64_t generation) {
    ui::SemanticTreeSnapshot tree;
    tree.generation = generation;
    tree.root = kRootId;

    ui::SemanticNodeSnapshot node;
    node.id = kRootId;
    node.info.role = ui::SemanticRole::Button;
    node.info.name = generation_name(generation);
    node.info.numeric_value = generation_value(generation);
    node.info.enabled = true;
    node.info.focusable = true;
    node.info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    node.bounds = {12.0f, 24.0f, 120.0f, 32.0f};
    tree.nodes.push_back(std::move(node));
    return tree;
}

/// Content is derived from the snapshot's own generation tag. A torn, mixed or
/// recycled publication therefore cannot satisfy this check, and a reader that
/// computes a different expected value from the tag also proves the reader is
/// reading the exact immutable generation it retained.
[[nodiscard]] bool snapshot_matches_own_tag(const ui::SemanticTreeSnapshot& snapshot,
                                            std::string& problem) {
    const auto generation = snapshot.generation;
    if (generation == 0) {
        problem = "snapshot carries untagged generation 0";
        return false;
    }
    if (snapshot.root != kRootId) {
        problem = "generation " + std::to_string(generation) +
                  " has root " + std::to_string(snapshot.root);
        return false;
    }
    if (snapshot.nodes.size() != 1) {
        problem = "generation " + std::to_string(generation) + " has " +
                  std::to_string(snapshot.nodes.size()) + " nodes";
        return false;
    }

    const auto& node = snapshot.nodes.front();
    if (node.id != kRootId) {
        problem = "generation " + std::to_string(generation) + " has node id " +
                  std::to_string(node.id);
        return false;
    }
    if (node.info.role != ui::SemanticRole::Button) {
        problem = "generation " + std::to_string(generation) + " lost its role";
        return false;
    }
    if (node.info.name != generation_name(generation)) {
        problem = "generation " + std::to_string(generation) + " carries name \"" +
                  node.info.name + "\"";
        return false;
    }
    if (node.info.numeric_value != generation_value(generation)) {
        problem = "generation " + std::to_string(generation) +
                  " carries a foreign numeric value";
        return false;
    }
    if (node.bounds.x != 12.0f || node.bounds.y != 24.0f ||
        node.bounds.w != 120.0f || node.bounds.h != 32.0f) {
        problem = "generation " + std::to_string(generation) +
                  " carries foreign bounds";
        return false;
    }
    if (!node.info.enabled || !node.info.focusable) {
        problem = "generation " + std::to_string(generation) +
                  " lost its exposed state";
        return false;
    }
    return true;
}

/// One native publication atomically pairs a native generation, the retained
/// semantic generation and copied geometry; all three must agree with the
/// publication's own tag.
[[nodiscard]] bool publication_matches_own_tag(
    const ui::detail::SemanticNativePublicationSnapshot& publication,
    std::string& problem) {
    if (!publication.semantic_snapshot) {
        problem = "native publication has no semantic snapshot";
        return false;
    }
    if (!snapshot_matches_own_tag(*publication.semantic_snapshot, problem)) {
        return false;
    }
    const auto generation = publication.generation;
    if (generation == 0) {
        problem = "native publication carries untagged generation 0";
        return false;
    }
    if (publication.semantic_generation() != generation) {
        problem = "native generation " + std::to_string(generation) +
                  " carries semantic generation " +
                  std::to_string(publication.semantic_generation());
        return false;
    }
    if (!(publication.geometry == generation_geometry(generation))) {
        problem = "native generation " + std::to_string(generation) +
                  " carries foreign geometry";
        return false;
    }
    return true;
}

void check_snapshot(const ui::SemanticTreeSnapshot& snapshot,
                    const char* expression,
                    int line) {
    std::string problem;
    if (!snapshot_matches_own_tag(snapshot, problem)) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression + " (" + problem +
                                 ")");
    }
}

void check_publication(
    const ui::detail::SemanticNativePublicationSnapshot& publication,
    const char* expression,
    int line) {
    std::string problem;
    if (!publication_matches_own_tag(publication, problem)) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression + " (" + problem +
                                 ")");
    }
}

struct LogicalReaderResult {
    // `observed` is the writer handshake signal; the remaining fields are
    // reader-owned and only read by the main thread after join.
    alignas(64) std::atomic<std::uint64_t> observed{0};
    alignas(64) std::atomic<bool> failed{false};
    std::uint64_t observations{0};
    std::uint64_t distinct_generations{0};
    std::uint64_t highest_generation{0};
    std::uint64_t previous_generation{0};
    std::uint64_t observed_mask{0};
    std::uint64_t content_mismatches{0};
    std::uint64_t monotonic_violations{0};
    std::uint64_t null_loads{0};
    std::shared_ptr<const ui::SemanticTreeSnapshot> first_retained;
    std::shared_ptr<const ui::SemanticTreeSnapshot> second_retained;
    std::string first_problem;
    std::exception_ptr failure;
};

struct NativeReaderResult {
    alignas(64) std::atomic<std::uint64_t> observed{0};
    alignas(64) std::atomic<bool> failed{false};
    std::uint64_t observations{0};
    std::uint64_t distinct_generations{0};
    std::uint64_t highest_generation{0};
    std::uint64_t previous_generation{0};
    std::uint64_t observed_mask{0};
    std::uint64_t content_mismatches{0};
    std::uint64_t monotonic_violations{0};
    std::uint64_t null_loads{0};
    std::shared_ptr<const ui::detail::SemanticNativePublicationSnapshot> first_retained;
    std::shared_ptr<const ui::detail::SemanticNativePublicationSnapshot> second_retained;
    std::string first_problem;
    std::exception_ptr failure;
};

enum class HandshakeOutcome {
    Complete,
    TimedOut,
    ReaderFailed,
};

/// Deterministic overlap primitive: the writer blocks until every reader has
/// loaded and reported the just-published generation. Because the writer
/// cannot advance while a reader still reports an older generation, no reader
/// can skip a handshake generation, regardless of scheduling or core count.
template <typename ReaderResults>
[[nodiscard]] HandshakeOutcome wait_for_all_readers(const ReaderResults& results,
                                                    std::uint64_t generation) {
    const auto deadline = std::chrono::steady_clock::now() + kHandshakeTimeout;
    while (true) {
        bool complete = true;
        for (const auto& result : results) {
            if (result.failed.load(std::memory_order_acquire)) {
                return HandshakeOutcome::ReaderFailed;
            }
            if (result.observed.load(std::memory_order_acquire) < generation) {
                complete = false;
            }
        }
        if (complete) {
            return HandshakeOutcome::Complete;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return HandshakeOutcome::TimedOut;
        }
        // The watchdog above exists only so a broken reader cannot hang CTest;
        // normal progress is synchronized exclusively through the atomics.
        std::this_thread::yield();
    }
}

void record_logical_observation(
    LogicalReaderResult& result,
    const std::shared_ptr<const ui::SemanticTreeSnapshot>& snapshot) {
    ++result.observations;
    const auto generation = snapshot->generation;

    std::string problem;
    if (!snapshot_matches_own_tag(*snapshot, problem)) {
        if (result.content_mismatches == 0) {
            result.first_problem = std::move(problem);
        }
        ++result.content_mismatches;
    }

    if (generation < result.highest_generation) {
        ++result.monotonic_violations;
    } else {
        result.highest_generation = generation;
    }
    if (generation != result.previous_generation) {
        ++result.distinct_generations;
        result.previous_generation = generation;
    }
    if (generation >= 1 && generation <= kHandshakeGenerations) {
        result.observed_mask |= std::uint64_t{1} << (generation - 1);
    }

    if (!result.first_retained) {
        result.first_retained = snapshot;
    }
    if (generation == 2 && !result.second_retained) {
        result.second_retained = snapshot;
    }

    result.observed.store(generation, std::memory_order_release);
}

void record_native_observation(
    NativeReaderResult& result,
    const std::shared_ptr<const ui::detail::SemanticNativePublicationSnapshot>& publication) {
    ++result.observations;
    const auto generation = publication->generation;

    std::string problem;
    if (!publication_matches_own_tag(*publication, problem)) {
        if (result.content_mismatches == 0) {
            result.first_problem = std::move(problem);
        }
        ++result.content_mismatches;
    }

    if (generation < result.highest_generation) {
        ++result.monotonic_violations;
    } else {
        result.highest_generation = generation;
    }
    if (generation != result.previous_generation) {
        ++result.distinct_generations;
        result.previous_generation = generation;
    }
    if (generation >= 1 && generation <= kHandshakeGenerations) {
        result.observed_mask |= std::uint64_t{1} << (generation - 1);
    }

    if (!result.first_retained) {
        result.first_retained = publication;
    }
    if (generation == 2 && !result.second_retained) {
        result.second_retained = publication;
    }

    result.observed.store(generation, std::memory_order_release);
}

void logical_reader(LogicalReaderResult& result,
                    const ui::detail::SemanticSnapshotPublisher& publisher,
                    std::barrier<>& gate,
                    const std::atomic<bool>& stop,
                    const std::atomic<bool>& writer_failed) {
    try {
        gate.arrive_and_wait();

        std::shared_ptr<const ui::SemanticTreeSnapshot> first;
        while (!first) {
            const auto candidate = publisher.current();
            // A fresh publisher exposes its generation-0 default snapshot;
            // only a real publication is a readable generation.
            if (candidate && candidate->generation >= 1) {
                first = candidate;
                break;
            }
            if (writer_failed.load(std::memory_order_acquire)) {
                return;
            }
            std::this_thread::yield();
        }
        // Generation 1 is already published and the writer waits for every
        // reader before publishing generation 2, so this is deterministically
        // the first generation any reader can retain.
        record_logical_observation(result, first);

        while (!stop.load(std::memory_order_acquire)) {
            const auto current = publisher.current();
            if (current) {
                record_logical_observation(result, current);
            } else {
                ++result.null_loads;
            }
            std::this_thread::yield();
        }

        // The writer stopped publishing but has not retired the endpoint yet;
        // one final load pins the documented terminal generation.
        const auto terminal = publisher.current();
        if (terminal) {
            record_logical_observation(result, terminal);
        } else {
            ++result.null_loads;
        }
    } catch (...) {
        result.failure = std::current_exception();
        result.failed.store(true, std::memory_order_release);
    }
}

void native_reader(
    NativeReaderResult& result,
    const std::weak_ptr<const ui::detail::SemanticNativePublicationSource>& weak_source,
    std::barrier<>& gate,
    const std::atomic<bool>& stop,
    const std::atomic<bool>& writer_failed) {
    try {
        gate.arrive_and_wait();

        // A native proxy only ever holds the view's weak reader source; the
        // strong source stays owned by the publication state for the view.
        const auto source = weak_source.lock();
        if (!source) {
            result.first_problem = "weak reader source expired before the reader started";
            result.failed.store(true, std::memory_order_release);
            return;
        }

        std::shared_ptr<const ui::detail::SemanticNativePublicationSnapshot> first;
        while (!first) {
            const auto candidate = source->current();
            if (candidate && candidate->generation >= 1) {
                first = candidate;
                break;
            }
            if (writer_failed.load(std::memory_order_acquire)) {
                return;
            }
            std::this_thread::yield();
        }
        record_native_observation(result, first);

        while (!stop.load(std::memory_order_acquire)) {
            const auto current = source->current();
            if (current) {
                record_native_observation(result, current);
            } else {
                ++result.null_loads;
            }
            std::this_thread::yield();
        }

        const auto terminal = source->current();
        if (terminal) {
            record_native_observation(result, terminal);
        } else {
            ++result.null_loads;
        }
    } catch (...) {
        result.failure = std::current_exception();
        result.failed.store(true, std::memory_order_release);
    }
}

void concurrent_logical_readers_retain_old_generations() {
    ui::detail::SemanticSnapshotPublisher publisher;

    std::array<LogicalReaderResult, kReaderCount> results{};
    std::atomic<bool> stop{false};
    std::atomic<bool> writer_failed{false};
    std::barrier<> gate{static_cast<std::ptrdiff_t>(kReaderCount + 1)};

    std::vector<std::thread> readers;
    readers.reserve(kReaderCount);
    for (std::size_t index = 0; index < kReaderCount; ++index) {
        readers.emplace_back(
            [&results, &publisher, &gate, &stop, &writer_failed, index] {
                logical_reader(results[index], publisher, gate, stop, writer_failed);
            });
    }

    std::string writer_problem;
    std::exception_ptr writer_failure;
    std::shared_ptr<const ui::SemanticTreeSnapshot> writer_retained;
    gate.arrive_and_wait();
    try {
        for (std::uint64_t generation = 1; generation <= kTotalGenerations;
             ++generation) {
            const auto changes =
                publisher.publish(make_generation_snapshot(generation));
            if (changes.empty()) {
                writer_problem = "generation " + std::to_string(generation) +
                                 " was suppressed as an unchanged publication";
                break;
            }
            if (generation == 1) {
                writer_retained = publisher.current();
            }
            if (generation <= kHandshakeGenerations) {
                const auto outcome = wait_for_all_readers(results, generation);
                if (outcome == HandshakeOutcome::TimedOut) {
                    writer_problem =
                        "reader handshake timed out before generation " +
                        std::to_string(generation);
                    break;
                }
                if (outcome == HandshakeOutcome::ReaderFailed) {
                    writer_problem =
                        "a reader failed before observing generation " +
                        std::to_string(generation);
                    break;
                }
            }
        }
    } catch (...) {
        writer_failure = std::current_exception();
        writer_problem = "writer threw before completing the publication sequence";
    }
    if (!writer_problem.empty()) {
        writer_failed.store(true, std::memory_order_release);
    }
    stop.store(true, std::memory_order_release);
    for (auto& reader : readers) {
        reader.join();
    }

    for (const auto& result : results) {
        if (result.failure) {
            std::rethrow_exception(result.failure);
        }
    }
    if (writer_failure) {
        std::rethrow_exception(writer_failure);
    }
    if (!writer_problem.empty()) {
        throw std::runtime_error("writer failed: " + writer_problem);
    }

    // The writer stopped but has not retired the endpoint: the terminal
    // generation is still the documented new-load value.
    const auto terminal = publisher.current();
    T068_CHECK(terminal != nullptr);
    T068_CHECK(terminal->generation == kTotalGenerations);
    T068_CHECK(writer_retained != nullptr);
    T068_CHECK(writer_retained->generation == 1);
    T068_CHECK(writer_retained.get() != terminal.get());

    for (const auto& result : results) {
        check_with_problem(result.content_mismatches == 0, result.first_problem,
                           "result.content_mismatches == 0", __LINE__);
        T068_CHECK(result.monotonic_violations == 0);
        T068_CHECK(result.null_loads == 0);
        // Every reader observed each handshake generation while the writer was
        // still publishing: gap-free, monotonic overlap, not a timing guess.
        T068_CHECK(result.observed_mask == kCompleteHandshakeMask);
        T068_CHECK(result.observations >= kHandshakeGenerations);
        T068_CHECK(result.distinct_generations >= kHandshakeGenerations);
        T068_CHECK(result.highest_generation == kTotalGenerations);
        T068_CHECK(result.first_retained != nullptr);
        T068_CHECK(result.first_retained->generation == 1);
        T068_CHECK(result.second_retained != nullptr);
        T068_CHECK(result.second_retained->generation == 2);
        T068_CHECK(result.first_retained.get() != terminal.get());
        T068_CHECK(result.second_retained.get() != terminal.get());
        T068_CHECK(result.first_retained->nodes.front().info.name ==
                   generation_name(1));
        T068_CHECK(result.second_retained->nodes.front().info.name ==
                   generation_name(2));
    }

    check_snapshot(*terminal, "terminal generation matches its own tag", __LINE__);
    check_snapshot(*writer_retained, "writer-retained generation 1", __LINE__);

    publisher.shutdown();
    T068_CHECK(!publisher.current());

    // Retained generations stay exact across retirement, including generations
    // that outlived hundreds of later publications, while new loads are
    // permanently defunct.
    check_snapshot(*terminal, "retained terminal generation after shutdown", __LINE__);
    check_snapshot(*writer_retained, "writer-retained generation 1 after shutdown",
                   __LINE__);
    for (const auto& result : results) {
        check_snapshot(*result.first_retained,
                       "reader-retained generation 1 after shutdown", __LINE__);
        check_snapshot(*result.second_retained,
                       "reader-retained generation 2 after shutdown", __LINE__);
    }
}

void concurrent_native_readers_retain_publications_through_weak_sources() {
    auto state = std::make_unique<ui::detail::SemanticNativePublicationState>();
    const auto weak_source = state->reader_source();
    T068_CHECK(!weak_source.expired());

    std::array<NativeReaderResult, kReaderCount> results{};
    std::atomic<bool> stop{false};
    std::atomic<bool> writer_failed{false};
    std::barrier<> gate{static_cast<std::ptrdiff_t>(kReaderCount + 1)};

    std::vector<std::thread> readers;
    readers.reserve(kReaderCount);
    for (std::size_t index = 0; index < kReaderCount; ++index) {
        readers.emplace_back(
            [&results, &weak_source, &gate, &stop, &writer_failed, index] {
                native_reader(results[index], weak_source, gate, stop, writer_failed);
            });
    }

    std::string writer_problem;
    std::exception_ptr writer_failure;
    std::shared_ptr<const ui::detail::SemanticNativePublicationSnapshot>
        writer_retained;
    gate.arrive_and_wait();
    try {
        for (std::uint64_t generation = 1; generation <= kTotalGenerations;
             ++generation) {
            std::vector<ui::SemanticChange> changes{
                generation == 1 ? ui::SemanticChange::StructureChanged
                                : ui::SemanticChange::ValueChanged};
            const auto batch = state->publish(
                std::make_shared<const ui::SemanticTreeSnapshot>(
                    make_generation_snapshot(generation)),
                changes,
                generation_geometry(generation));
            if (!batch.has_value() || !batch->publication) {
                writer_problem = "native generation " + std::to_string(generation) +
                                 " was rejected by the publication state";
                break;
            }
            // The writer changes exposed semantic content and geometry on every
            // generation, so native and semantic generations advance in
            // lockstep.
            if (batch->generation() != generation ||
                batch->semantic_generation() != generation) {
                writer_problem = "native generation " + std::to_string(generation) +
                                 " reported generation " +
                                 std::to_string(batch->generation()) +
                                 " / semantic generation " +
                                 std::to_string(batch->semantic_generation());
                break;
            }
            if (generation == 1) {
                writer_retained = state->current();
            }
            if (generation <= kHandshakeGenerations) {
                const auto outcome = wait_for_all_readers(results, generation);
                if (outcome == HandshakeOutcome::TimedOut) {
                    writer_problem =
                        "reader handshake timed out before generation " +
                        std::to_string(generation);
                    break;
                }
                if (outcome == HandshakeOutcome::ReaderFailed) {
                    writer_problem =
                        "a reader failed before observing generation " +
                        std::to_string(generation);
                    break;
                }
            }
        }
    } catch (...) {
        writer_failure = std::current_exception();
        writer_problem = "writer threw before completing the publication sequence";
    }
    if (!writer_problem.empty()) {
        writer_failed.store(true, std::memory_order_release);
    }
    stop.store(true, std::memory_order_release);
    for (auto& reader : readers) {
        reader.join();
    }

    for (const auto& result : results) {
        if (result.failure) {
            std::rethrow_exception(result.failure);
        }
    }
    if (writer_failure) {
        std::rethrow_exception(writer_failure);
    }
    if (!writer_problem.empty()) {
        throw std::runtime_error("writer failed: " + writer_problem);
    }

    const auto terminal = state->current();
    T068_CHECK(terminal != nullptr);
    T068_CHECK(terminal->generation == kTotalGenerations);
    T068_CHECK(writer_retained != nullptr);
    T068_CHECK(writer_retained->generation == 1);
    T068_CHECK(writer_retained.get() != terminal.get());

    for (const auto& result : results) {
        check_with_problem(result.content_mismatches == 0, result.first_problem,
                           "result.content_mismatches == 0", __LINE__);
        T068_CHECK(result.monotonic_violations == 0);
        T068_CHECK(result.null_loads == 0);
        T068_CHECK(result.observed_mask == kCompleteHandshakeMask);
        T068_CHECK(result.observations >= kHandshakeGenerations);
        T068_CHECK(result.distinct_generations >= kHandshakeGenerations);
        T068_CHECK(result.highest_generation == kTotalGenerations);
        T068_CHECK(result.first_retained != nullptr);
        T068_CHECK(result.first_retained->generation == 1);
        T068_CHECK(result.second_retained != nullptr);
        T068_CHECK(result.second_retained->generation == 2);
        T068_CHECK(result.first_retained.get() != terminal.get());
        T068_CHECK(result.second_retained.get() != terminal.get());
    }

    check_publication(*terminal, "terminal native publication matches its tag",
                      __LINE__);
    check_publication(*writer_retained, "writer-retained native publication 1",
                      __LINE__);

    state->shutdown();
    T068_CHECK(!state->current());
    {
        // The view still owns the source, so a weak reader reference stays
        // lockable while every new read is defunct.
        const auto source = weak_source.lock();
        T068_CHECK(source != nullptr);
        T068_CHECK(!source->current());
    }

    check_publication(*terminal, "retained terminal publication after shutdown",
                      __LINE__);
    check_publication(*writer_retained,
                      "writer-retained publication 1 after shutdown", __LINE__);
    for (const auto& result : results) {
        check_publication(*result.first_retained,
                          "reader-retained publication 1 after shutdown", __LINE__);
        check_publication(*result.second_retained,
                          "reader-retained publication 2 after shutdown", __LINE__);
    }

    state.reset();
    T068_CHECK(weak_source.expired());
}

} // namespace

int main() {
    try {
        concurrent_logical_readers_retain_old_generations();
        concurrent_native_readers_retain_publications_through_weak_sources();
        std::cout << "PASS t068 concurrent snapshot readers\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 concurrent snapshot readers: " << error.what()
                  << '\n';
        return EXIT_FAILURE;
    }
}
