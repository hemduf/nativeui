// T440 research probe: NativeUI text workloads against Skia M153 strike-cache modes.
//
// This executable is research-only instrumentation for issue #440. It lives
// entirely under tests/, touches no production NativeUI code, and never exposes
// the upstream Skia build flag through public headers or installed targets.
//
// Variants recorded in the result document:
//   A - pinned skia-builder chrome/m153 archive (thread-local strike cache OFF)
//   B - a later skia-builder archive built with skia_enable_threadlocal_strikecache
//       enabled (skia-builder work is a separate follow-up task; the probe only
//       records the runtime selector default so a variant-B run cannot be
//       mislabelled)
//   C - pinned chrome/m153 archive (flag OFF) with the exported experimental
//       runtime selector forced to true before any text operation
//
// Modes: --self-test, --json <path>, --threads <list> (default 1,2,4,8),
//        --variant <A|B|C>, --skia-archive-sha256 <64 hex chars> (recorded as
//        provenance; mandatory for decision-grade runs, see the research doc).
// Result schema: t440-strikecache-v2.
//
// Measurement protocol reuses the frozen T051 constants and summarize_samples()
// semantics: 5 discarded warmup samples, 30 measured samples, median = average
// of sorted samples 14/15, p95 = sorted sample 28. T051's run_fixed_protocol()
// is single-threaded by construction, so this probe coordinates its own
// per-sample barriers while keeping the identical 5+30 sample shape.
//
// Concurrency model: one OS thread per UI+HeadlessRenderer instance. NativeUI
// retained UI is main-thread confined, so each worker exclusively owns its own
// instance and only touches it on its own thread. Worker threads persist for a
// whole (workload, thread-count) run so a thread-local strike cache is not
// artificially cold on every sample.
//
// Memory instrumentation notes:
//   - SkGraphics::GetFontCacheUsed()/GetFontCacheCountUsed() are sampled on each
//     rendering thread after its batch. With a process-global cache every thread
//     observes the same process-wide value; with thread-local caches the values
//     are disjoint per-thread values. The document records the raw per-thread
//     arrays, their largest thread, and a scope-aware total that is the only
//     field comparable across variants (process-global value vs per-thread sum).
//   - Process RSS/footprint deltas come from platform APIs. The T051 operator-new
//     interception is deliberately absent here: Skia allocates strike caches
//     through malloc, which operator new cannot see. Process-level deltas are
//     therefore the only reliable whole-process memory signal for strike caches.
//
// Privacy: the probe records build metadata (commit SHA, compiler, OS, arch) and
// measurements only. It never records user names, host names or file-system
// paths. Result documents are expected to stay outside the repository.

#include "benchmarks/t051_benchmark_harness.hpp"
#include "test_support.hpp"

#include <nativeui/nativeui.hpp>

#include "include/core/SkGraphics.h"
#include "include/core/SkMilestone.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <mach/mach.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

#ifndef NATIVEUI_BENCHMARK_COMMIT_SHA
#define NATIVEUI_BENCHMARK_COMMIT_SHA "unknown"
#endif
#ifndef NATIVEUI_BENCHMARK_COMPILER_ID
#define NATIVEUI_BENCHMARK_COMPILER_ID "unknown"
#endif
#ifndef NATIVEUI_BENCHMARK_COMPILER_VERSION
#define NATIVEUI_BENCHMARK_COMPILER_VERSION "unknown"
#endif
#ifndef NATIVEUI_BENCHMARK_COMPILER_MAJOR
#define NATIVEUI_BENCHMARK_COMPILER_MAJOR 0
#endif
#ifndef NATIVEUI_BENCHMARK_BUILD_TYPE
#define NATIVEUI_BENCHMARK_BUILD_TYPE "unknown"
#endif
#ifndef NATIVEUI_BENCHMARK_OS
#define NATIVEUI_BENCHMARK_OS "unknown"
#endif
#ifndef NATIVEUI_BENCHMARK_ARCH
#define NATIVEUI_BENCHMARK_ARCH "unknown"
#endif

// Skia M153 exports this experimental process-global selector. The pinned
// archive defines it as false; a future archive built with
// skia_enable_threadlocal_strikecache defines it as true. Only this research
// executable under tests/ may reference it.
extern bool gSkUseThreadLocalStrikeCaches_IAcknowledgeThisIsIncrediblyExperimental;

namespace {

using nativeui::bench::kWarmupSamples;
using nativeui::bench::kMeasuredSamples;
using nativeui::bench::SampleSummary;
using nativeui::bench::summarize_samples;

inline constexpr std::string_view kSchema = "t440-strikecache-v2";
inline constexpr std::string_view kTextLayoutPaint = "text_layout_paint";
inline constexpr std::string_view kFontSizeChurn = "font_size_churn";
inline constexpr float kViewportWidth = 1024.0f;
inline constexpr float kViewportHeight = 768.0f;
inline constexpr float kFixedFontSize = 11.0f;
// Alternate content for the isolation/staleness checks: a different strike
// descriptor must produce different pixels, so equal checksums cannot pass
// vacuously through content-addressed cache reuse.
inline constexpr float kAlternateFontSize = 17.0f;
inline constexpr int kMaximumThreadCount = 64;

// 24 deterministic distinct font sizes for the churn workload. The op count per
// sample equals the size count so every sample covers one complete cycle.
inline constexpr std::array<float, 24> kChurnFontSizes{
    11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 17.0f, 18.0f,
    19.0f, 20.0f, 21.0f, 22.0f, 23.0f, 24.0f, 25.0f, 26.0f,
    27.0f, 28.0f, 29.0f, 30.0f, 31.0f, 32.0f, 33.0f, 34.0f,
};

struct ProbeWorkload {
    std::string_view name;
    std::uint64_t operations_per_sample;
    std::size_t font_size_count;
};

inline constexpr std::array<ProbeWorkload, 2> kWorkloads{{
    {kTextLayoutPaint, 50, 1},
    {kFontSizeChurn, 24, kChurnFontSizes.size()},
}};

// ---------------------------------------------------------------------------
// Small JSON helpers (research report serialization only).
// ---------------------------------------------------------------------------

std::string json_quote(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back('"');
    for (const char ch : value) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(ch); break;
        }
    }
    out.push_back('"');
    return out;
}

std::string json_number(double value) {
    if (!std::isfinite(value)) {
        throw std::runtime_error("T440 probe refuses to serialize a non-finite measurement");
    }
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return out.str();
}

class JsonSyntaxValidator {
public:
    static bool is_valid(std::string_view json) {
        try {
            JsonSyntaxValidator validator(json);
            validator.skip_whitespace();
            validator.parse_value();
            validator.skip_whitespace();
            return validator.at_end();
        } catch (const std::exception&) {
            return false;
        }
    }

private:
    explicit JsonSyntaxValidator(std::string_view json) : json_(json) {}

    [[nodiscard]] bool at_end() const { return position_ == json_.size(); }
    [[nodiscard]] char peek() const {
        return position_ < json_.size() ? json_[position_] : '\0';
    }
    void advance() { ++position_; }

    void skip_whitespace() {
        while (position_ < json_.size()) {
            const char ch = json_[position_];
            if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') break;
            ++position_;
        }
    }

    void expect(char expected) {
        if (peek() != expected) throw std::runtime_error("unexpected JSON token");
        advance();
    }

    static bool is_digit(char ch) { return ch >= '0' && ch <= '9'; }
    static bool is_hex_digit(char ch) {
        return is_digit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
    }

    void parse_value() {
        switch (peek()) {
        case '{': parse_object(); break;
        case '[': parse_array(); break;
        case '"': parse_string(); break;
        case 't': parse_literal("true"); break;
        case 'f': parse_literal("false"); break;
        case 'n': parse_literal("null"); break;
        default: parse_number(); break;
        }
    }

    void parse_object() {
        expect('{');
        skip_whitespace();
        if (peek() == '}') {
            advance();
            return;
        }
        for (;;) {
            if (peek() != '"') throw std::runtime_error("object key must be a string");
            parse_string();
            skip_whitespace();
            expect(':');
            skip_whitespace();
            parse_value();
            skip_whitespace();
            if (peek() == ',') {
                advance();
                skip_whitespace();
                continue;
            }
            expect('}');
            return;
        }
    }

    void parse_array() {
        expect('[');
        skip_whitespace();
        if (peek() == ']') {
            advance();
            return;
        }
        for (;;) {
            parse_value();
            skip_whitespace();
            if (peek() == ',') {
                advance();
                skip_whitespace();
                continue;
            }
            expect(']');
            return;
        }
    }

    void parse_string() {
        expect('"');
        while (position_ < json_.size()) {
            const char ch = json_[position_];
            advance();
            if (ch == '"') return;
            if (static_cast<unsigned char>(ch) < 0x20) {
                throw std::runtime_error("raw control character in JSON string");
            }
            if (ch != '\\') continue;
            const char escape = peek();
            advance();
            switch (escape) {
            case '"': case '\\': case '/': case 'b': case 'f':
            case 'n': case 'r': case 't':
                break;
            case 'u':
                for (int digit = 0; digit < 4; ++digit) {
                    if (!is_hex_digit(peek())) {
                        throw std::runtime_error("bad unicode escape in JSON string");
                    }
                    advance();
                }
                break;
            default:
                throw std::runtime_error("bad escape in JSON string");
            }
        }
        throw std::runtime_error("unterminated JSON string");
    }

    void parse_number() {
        if (peek() == '-') advance();
        if (!is_digit(peek())) throw std::runtime_error("bad JSON number");
        if (peek() == '0') {
            advance();
        } else {
            while (is_digit(peek())) advance();
        }
        if (peek() == '.') {
            advance();
            if (!is_digit(peek())) throw std::runtime_error("bad JSON fraction");
            while (is_digit(peek())) advance();
        }
        if (peek() == 'e' || peek() == 'E') {
            advance();
            if (peek() == '+' || peek() == '-') advance();
            if (!is_digit(peek())) throw std::runtime_error("bad JSON exponent");
            while (is_digit(peek())) advance();
        }
    }

    void parse_literal(std::string_view literal) {
        if (json_.substr(position_, literal.size()) != literal) {
            throw std::runtime_error("bad JSON literal");
        }
        position_ += literal.size();
    }

    std::string_view json_;
    std::size_t position_{};
};

// ---------------------------------------------------------------------------
// Process memory snapshots.
// ---------------------------------------------------------------------------

struct MemorySnapshot {
    bool available{};
    std::string metric{"unavailable"};
    std::uint64_t bytes{};
    std::uint64_t resident_bytes{};
};

MemorySnapshot sample_process_memory() {
    MemorySnapshot snapshot;
#if defined(__APPLE__)
    task_vm_info_data_t info{};
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS) {
        snapshot.available = true;
        snapshot.metric = "phys_footprint";
        snapshot.bytes = static_cast<std::uint64_t>(info.phys_footprint);
        snapshot.resident_bytes = static_cast<std::uint64_t>(info.resident_size);
    }
#elif defined(__linux__)
    std::ifstream input("/proc/self/statm");
    std::uint64_t total_pages = 0;
    std::uint64_t resident_pages = 0;
    if (input >> total_pages >> resident_pages) {
        const auto page_size = static_cast<std::uint64_t>(::sysconf(_SC_PAGESIZE));
        snapshot.available = true;
        snapshot.metric = "resident_set_size";
        snapshot.bytes = resident_pages * page_size;
        snapshot.resident_bytes = snapshot.bytes;
    }
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS counters{};
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &counters, sizeof(counters))) {
        snapshot.available = true;
        snapshot.metric = "working_set";
        snapshot.bytes = static_cast<std::uint64_t>(counters.WorkingSetSize);
        snapshot.resident_bytes = snapshot.bytes;
    }
#endif
    return snapshot;
}

// ---------------------------------------------------------------------------
// Reusable per-sample barrier.
// ---------------------------------------------------------------------------

class Barrier {
public:
    explicit Barrier(int participants)
        : participants_(participants), waiting_(participants) {}

    Barrier(const Barrier&) = delete;
    Barrier& operator=(const Barrier&) = delete;

    void arrive() {
        std::unique_lock lock(mutex_);
        if (aborted_) return;
        const auto generation = generation_;
        if (--waiting_ == 0) {
            waiting_ = participants_;
            ++generation_;
            cv_.notify_all();
            return;
        }
        cv_.wait(lock, [&] { return generation_ != generation || aborted_; });
    }

    void abort() {
        std::lock_guard lock(mutex_);
        aborted_ = true;
        cv_.notify_all();
    }

    [[nodiscard]] bool aborted() const {
        std::lock_guard lock(mutex_);
        return aborted_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    int participants_;
    int waiting_;
    std::uint64_t generation_{};
    bool aborted_{};
};

// ---------------------------------------------------------------------------
// Text-heavy fixture.
// ---------------------------------------------------------------------------

// Instance-owned mutable paint state. It is only ever touched by the single
// worker thread that owns the fixture; it is deliberately not a global, not a
// static and not a thread_local.
struct FontSizeState {
    float size{kFixedFontSize};
};

class TextBenchLeaf final : public ui::Component {
public:
    TextBenchLeaf(ui::Size size, std::shared_ptr<FontSizeState> state)
        : size_(size), state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return size_;
    }

    void paint(ui::PaintContext& context) const override {
        const auto bounds = context.bounds();
        context.painter().fill_rounded_rect(bounds, 2.0f, ui::colors::panel);
        context.painter().text(
            {bounds.x + 3.0f, bounds.y + bounds.h * 0.5f},
            "NativeUI strike cache benchmark text",
            state_->size,
            ui::colors::text);
    }

private:
    ui::Size size_{};
    std::shared_ptr<FontSizeState> state_;
};

ui::GridTracks grid_tracks(std::size_t columns, std::size_t rows) {
    ui::GridTracks tracks;
    tracks.columns.reserve(columns);
    tracks.rows.reserve(rows);
    for (std::size_t column = 0; column < columns; ++column) {
        tracks.columns.push_back(ui::Track::auto_size());
    }
    for (std::size_t row = 0; row < rows; ++row) {
        tracks.rows.push_back(ui::Track::auto_size());
    }
    return tracks;
}

ui::Spec make_grid(std::size_t columns,
                   std::size_t rows,
                   std::vector<ui::Spec> children,
                   float gap = 1.0f) {
    auto tracks = grid_tracks(columns, rows);
    return ui::Spec{
        [tracks = std::move(tracks), gap] {
            return std::make_unique<ui::GridComponent>(tracks, gap, gap);
        },
        std::move(children)};
}

ui::Spec make_row(std::vector<ui::Spec> children) {
    return ui::Spec{
        [] { return std::make_unique<ui::RowComponent>(2.0f, ui::Align::Stretch, ui::Justify::Start); },
        std::move(children)};
}

ui::Spec make_column(std::vector<ui::Spec> children) {
    return ui::Spec{
        [] { return std::make_unique<ui::ColumnComponent>(2.0f, 2.0f, ui::Align::Stretch, ui::Justify::Start); },
        std::move(children)};
}

// Mirrors the T051 paint_text shape (4 groups x 2 grids x 10 text leaves) so the
// probe measures the same kind of retained text-heavy tree as the frozen
// benchmark workload.
ui::Spec make_text_tree(std::shared_ptr<FontSizeState> state) {
    std::vector<ui::Spec> root_children;
    root_children.reserve(4);
    for (int group = 0; group < 4; ++group) {
        std::vector<ui::Spec> row_children;
        row_children.reserve(2);
        for (int grid = 0; grid < 2; ++grid) {
            std::vector<ui::Spec> leaves;
            leaves.reserve(10);
            for (int leaf = 0; leaf < 10; ++leaf) {
                leaves.push_back(ui::Spec{
                    [state] {
                        return std::make_unique<TextBenchLeaf>(
                            ui::Size{38.0f, 24.0f}, state);
                    },
                    {}});
            }
            row_children.push_back(make_grid(5, 2, std::move(leaves)));
        }
        root_children.push_back(make_row(std::move(row_children)));
    }
    return make_column(std::move(root_children));
}

struct SpecRoot {
    ui::Spec value;
    ui::Spec spec() && { return std::move(value); }
};

std::unique_ptr<ui::UI> make_ui(ui::Spec spec) {
    return std::make_unique<ui::UI>(SpecRoot{std::move(spec)});
}

struct TextFixture {
    std::shared_ptr<FontSizeState> size_state;
    std::unique_ptr<ui::UI> tree;
    std::unique_ptr<ui::HeadlessRenderer> renderer;
};

TextFixture make_text_fixture(float initial_font_size) {
    TextFixture fixture;
    fixture.size_state = std::make_shared<FontSizeState>();
    fixture.size_state->size = initial_font_size;
    fixture.tree = make_ui(make_text_tree(fixture.size_state));
    fixture.renderer = std::make_unique<ui::HeadlessRenderer>(
        ui::Size{kViewportWidth, kViewportHeight}, 1.0f);
    return fixture;
}

void relayout(TextFixture& fixture) {
    fixture.tree->invalidate_layout();
    (void)fixture.tree->measure(ui::Constraints::tight({kViewportWidth, kViewportHeight}));
}

bool render(TextFixture& fixture) {
    return fixture.renderer->render(*fixture.tree);
}

std::string pixel_checksum(const ui::HeadlessRenderer& renderer) {
    constexpr std::uint64_t kFnvOffset = 1469598103934665603ull;
    constexpr std::uint64_t kFnvPrime = 1099511628211ull;
    std::uint64_t hash = kFnvOffset;
    for (const std::uint8_t byte : renderer.rgba_pixels()) {
        hash ^= byte;
        hash *= kFnvPrime;
    }
    std::ostringstream out;
    out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::string render_fixture_checksum(TextFixture& fixture, bool relayout_first) {
    if (relayout_first) relayout(fixture);
    if (!render(fixture)) throw std::runtime_error("T440 headless render failed");
    return pixel_checksum(*fixture.renderer);
}

// ---------------------------------------------------------------------------
// Concurrent fixed protocol.
// ---------------------------------------------------------------------------

struct WorkerContext {
    const ProbeWorkload* workload{};
    int index{};
    Barrier* barrier{};
    std::unique_ptr<TextFixture> fixture;
    std::size_t churn_cursor{};
    std::vector<double> sample_ns_per_op;
    SampleSummary summary;
    std::size_t font_cache_used_before_bytes{};
    std::size_t font_cache_used_after_bytes{};
    int font_cache_count_before{};
    int font_cache_count_after{};
    std::size_t font_cache_limit_bytes{};
    int font_cache_count_limit{};
    std::exception_ptr failure;
};

float initial_font_size(const ProbeWorkload& workload) {
    return workload.name == kFontSizeChurn ? kChurnFontSizes.front() : kFixedFontSize;
}

void reset_before_sample(WorkerContext& context) {
    if (context.workload->name != kFontSizeChurn) return;
    context.churn_cursor = 0;
    context.fixture->size_state->size = kChurnFontSizes.front();
}

void run_text_operation(WorkerContext& context) {
    if (context.workload->name == kFontSizeChurn) {
        context.fixture->size_state->size = kChurnFontSizes[context.churn_cursor];
        context.churn_cursor = (context.churn_cursor + 1) % kChurnFontSizes.size();
    }
    relayout(*context.fixture);
    if (!render(*context.fixture)) {
        throw std::runtime_error("T440 headless render failed inside measured batch");
    }
}

void sample_worker_font_cache(WorkerContext& context) {
    context.font_cache_used_before_bytes = SkGraphics::GetFontCacheUsed();
    context.font_cache_count_before = SkGraphics::GetFontCacheCountUsed();
    context.font_cache_limit_bytes = SkGraphics::GetFontCacheLimit();
    context.font_cache_count_limit = SkGraphics::GetFontCacheCountLimit();
}

void worker_main(WorkerContext& context) {
    bool failed = false;
    const auto record_failure = [&context, &failed] {
        failed = true;
        if (!context.failure) context.failure = std::current_exception();
    };

    try {
        context.fixture = std::make_unique<TextFixture>(
            make_text_fixture(initial_font_size(*context.workload)));
        sample_worker_font_cache(context);
        if (!render(*context.fixture)) {
            throw std::runtime_error("T440 headless render failed during fixture warmup");
        }
    } catch (...) {
        record_failure();
    }

    const int total_samples = kWarmupSamples + kMeasuredSamples;
    for (int sample = 0; sample < total_samples; ++sample) {
        if (context.barrier->aborted()) break;
        if (!failed) {
            try {
                reset_before_sample(context);
            } catch (...) {
                record_failure();
            }
        }
        context.barrier->arrive();
        if (context.barrier->aborted()) break;
        if (!failed) {
            const auto start = std::chrono::steady_clock::now();
            try {
                for (std::uint64_t operation = 0;
                     operation < context.workload->operations_per_sample;
                     ++operation) {
                    run_text_operation(context);
                }
            } catch (...) {
                record_failure();
            }
            const auto end = std::chrono::steady_clock::now();
            if (sample >= kWarmupSamples) {
                const auto elapsed =
                    std::chrono::duration<double, std::nano>(end - start).count();
                context.sample_ns_per_op[static_cast<std::size_t>(sample - kWarmupSamples)] =
                    elapsed / static_cast<double>(context.workload->operations_per_sample);
            }
        }
        context.barrier->arrive();
    }

    if (!failed) {
        context.font_cache_used_after_bytes = SkGraphics::GetFontCacheUsed();
        context.font_cache_count_after = SkGraphics::GetFontCacheCountUsed();
        try {
            context.summary = summarize_samples(context.sample_ns_per_op);
        } catch (...) {
            record_failure();
        }
    }

    // Retained UI and the headless renderer are destroyed on the thread that
    // owns them, matching the plugin-instance teardown model.
    if (context.fixture) {
        context.fixture->renderer.reset();
        context.fixture->tree.reset();
        context.fixture.reset();
    }
}

struct RunResult {
    int thread_count{};
    std::string workload;
    std::uint64_t operations_per_sample{};
    SampleSummary aggregate;
    std::vector<double> thread_median_ns_per_op;
    std::vector<double> thread_p95_ns_per_op;
    // Scope actually active while this run measured: true = each worker owns a
    // disjoint thread-local strike cache, false = all workers sample one shared
    // process-global cache. Required to interpret the raw per-thread arrays
    // correctly; only the derived scope-aware totals are comparable across A/B/C.
    bool threadlocal_effective{};
    std::vector<std::uint64_t> font_cache_used_before_bytes;
    std::vector<std::uint64_t> font_cache_used_after_bytes;
    std::vector<int> font_cache_count_before;
    std::vector<int> font_cache_count_after;
    std::uint64_t font_cache_limit_bytes{};
    int font_cache_count_limit{};
    MemorySnapshot memory_before;
    MemorySnapshot memory_after;
};

std::uint64_t font_cache_used_largest_thread(const RunResult& run) {
    std::uint64_t largest = 0;
    for (const auto value : run.font_cache_used_after_bytes) {
        largest = std::max(largest, value);
    }
    return largest;
}

std::uint64_t font_cache_used_sum(const RunResult& run) {
    std::uint64_t sum = 0;
    for (const auto value : run.font_cache_used_after_bytes) {
        sum += value;
    }
    return sum;
}

std::int64_t font_cache_count_largest_thread(const RunResult& run) {
    int largest = 0;
    for (const auto value : run.font_cache_count_after) {
        largest = std::max(largest, value);
    }
    return static_cast<std::int64_t>(largest);
}

std::int64_t font_cache_count_sum(const RunResult& run) {
    std::int64_t sum = 0;
    for (const auto value : run.font_cache_count_after) {
        sum += static_cast<std::int64_t>(value);
    }
    return sum;
}

// The only cross-scope comparable aggregate: the process-global cache value
// when the OFF build shares one cache, and the sum of the disjoint per-thread
// caches when the thread-local selector is active.
std::uint64_t font_cache_used_total(const RunResult& run) {
    return run.threadlocal_effective ? font_cache_used_sum(run)
                                     : font_cache_used_largest_thread(run);
}

std::int64_t font_cache_count_total(const RunResult& run) {
    return run.threadlocal_effective ? font_cache_count_sum(run)
                                     : font_cache_count_largest_thread(run);
}

RunResult run_concurrent(const ProbeWorkload& workload,
                         int thread_count,
                         bool threadlocal_effective) {
    if (thread_count < 1 || thread_count > kMaximumThreadCount) {
        throw std::invalid_argument("T440 thread count out of range");
    }

    std::vector<WorkerContext> workers(static_cast<std::size_t>(thread_count));
    std::vector<std::thread> threads;
    threads.reserve(workers.size());
    Barrier barrier(thread_count + 1);

    for (int index = 0; index < thread_count; ++index) {
        auto& worker = workers[static_cast<std::size_t>(index)];
        worker.workload = &workload;
        worker.index = index;
        worker.barrier = &barrier;
        worker.sample_ns_per_op.assign(kMeasuredSamples, 0.0);
    }

    const MemorySnapshot memory_before = sample_process_memory();
    bool started_all = true;
    try {
        for (auto& worker : workers) {
            threads.emplace_back([&worker] { worker_main(worker); });
        }
    } catch (...) {
        started_all = false;
    }

    std::vector<double> aggregate_samples;
    aggregate_samples.reserve(kMeasuredSamples);
    if (started_all) {
        const int total_samples = kWarmupSamples + kMeasuredSamples;
        for (int sample = 0; sample < total_samples; ++sample) {
            barrier.arrive();
            const auto start = std::chrono::steady_clock::now();
            barrier.arrive();
            const auto end = std::chrono::steady_clock::now();
            if (sample >= kWarmupSamples) {
                const auto elapsed =
                    std::chrono::duration<double, std::nano>(end - start).count();
                const auto total_operations = static_cast<double>(thread_count) *
                                              static_cast<double>(workload.operations_per_sample);
                aggregate_samples.push_back(elapsed / total_operations);
            }
        }
    } else {
        barrier.abort();
    }

    for (auto& thread : threads) thread.join();
    if (!started_all) {
        throw std::runtime_error("T440 probe failed to start all worker threads");
    }

    for (const auto& worker : workers) {
        if (!worker.failure) continue;
        try {
            std::rethrow_exception(worker.failure);
        } catch (const std::exception& error) {
            throw std::runtime_error("T440 worker " + std::to_string(worker.index) +
                                     " failed: " + error.what());
        }
    }

    const MemorySnapshot memory_after = sample_process_memory();

    RunResult result;
    result.thread_count = thread_count;
    result.workload = std::string(workload.name);
    result.operations_per_sample = workload.operations_per_sample;
    result.threadlocal_effective = threadlocal_effective;
    result.aggregate = summarize_samples(aggregate_samples);
    result.memory_before = memory_before;
    result.memory_after = memory_after;
    result.font_cache_limit_bytes = workers.front().font_cache_limit_bytes;
    result.font_cache_count_limit = workers.front().font_cache_count_limit;
    for (const auto& worker : workers) {
        result.thread_median_ns_per_op.push_back(worker.summary.median_ns_per_op);
        result.thread_p95_ns_per_op.push_back(worker.summary.p95_ns_per_op);
        result.font_cache_used_before_bytes.push_back(worker.font_cache_used_before_bytes);
        result.font_cache_used_after_bytes.push_back(worker.font_cache_used_after_bytes);
        result.font_cache_count_before.push_back(worker.font_cache_count_before);
        result.font_cache_count_after.push_back(worker.font_cache_count_after);
    }
    result.font_cache_used_before_bytes.shrink_to_fit();
    result.font_cache_used_after_bytes.shrink_to_fit();
    return result;
}

// ---------------------------------------------------------------------------
// Isolation and cache-lifetime checks.
// ---------------------------------------------------------------------------

struct CheckRecord {
    std::string name;
    bool passed{};
    std::uint64_t cycles{};
    std::string checksum_control;
    std::string checksum_control_alternate;
    std::string checksum_survivor;
    std::string checksum_cold_control;
    std::string checksum_cold_control_alternate;
    std::string checksum_after_purge;
    std::uint64_t font_cache_used_before_purge{};
    std::uint64_t font_cache_used_after_purge{};
    std::uint64_t font_cache_used_after_rerender{};
    bool purge_on_rendering_thread{};
    std::string detail;
};

// Single-instance control for the requested content: create, render, checksum,
// destroy.
std::string render_control_checksum(float font_size) {
    auto fixture = make_text_fixture(font_size);
    return render_fixture_checksum(fixture, false);
}

CheckRecord run_multi_instance_lifecycle_check() {
    CheckRecord check;
    check.name = "multi_instance_lifecycle";
    check.cycles = 50;
    check.checksum_control = render_control_checksum(kFixedFontSize);
    check.checksum_control_alternate = render_control_checksum(kAlternateFontSize);
    if (check.checksum_control == check.checksum_control_alternate) {
        check.passed = false;
        check.detail = "alternate-content control must differ from the fixed-content control";
        return check;
    }

    // The two simultaneously alive instances render different content. A stale
    // or contaminated cache that returned another instance's content-correct
    // strikes under a colliding key would then produce the wrong checksum.
    bool passed = true;
    std::string survivor_checksum;
    for (std::uint64_t cycle = 0; cycle < check.cycles && passed; ++cycle) {
        auto first = make_text_fixture(kFixedFontSize);
        auto second = make_text_fixture(kAlternateFontSize);
        passed = render_fixture_checksum(first, false) == check.checksum_control &&
                 render_fixture_checksum(second, false) == check.checksum_control_alternate;
        if (!passed) break;

        if (cycle % 2 == 0) {
            first.renderer.reset();
            first.tree.reset();
            survivor_checksum = render_fixture_checksum(second, false);
            passed = survivor_checksum == check.checksum_control_alternate;
            second.renderer.reset();
            second.tree.reset();
        } else {
            second.renderer.reset();
            second.tree.reset();
            survivor_checksum = render_fixture_checksum(first, false);
            passed = survivor_checksum == check.checksum_control;
            first.renderer.reset();
            first.tree.reset();
        }
    }

    check.passed = passed;
    check.checksum_survivor = survivor_checksum;
    check.detail = passed
        ? "50 create/render/checksum/destroy cycles of two different-content instances "
          "in both destruction orders; the surviving instance stayed functional and "
          "matched its own content control"
        : "instance isolation or survivor rendering diverged from the content control";
    return check;
}

CheckRecord run_cache_staleness_check() {
    CheckRecord check;
    check.name = "cache_staleness_after_destruction";

    // Establish a clean cache baseline on the rendering thread, then render two
    // cold single-instance controls with different content. The alternate
    // control is the non-vacuous post-purge reference: it proves the check
    // cannot pass by reusing the fixed-content strikes.
    SkGraphics::PurgeFontCache();
    check.checksum_cold_control = render_control_checksum(kFixedFontSize);
    check.checksum_cold_control_alternate = render_control_checksum(kAlternateFontSize);

    check.font_cache_used_before_purge = SkGraphics::GetFontCacheUsed();
    SkGraphics::PurgeFontCache();
    check.font_cache_used_after_purge = SkGraphics::GetFontCacheUsed();
    check.purge_on_rendering_thread = true;

    // Every renderer is destroyed at this point; re-render the alternate
    // content with a fresh instance and require its own cold control checksum.
    auto fresh = make_text_fixture(kAlternateFontSize);
    check.checksum_after_purge = render_fixture_checksum(fresh, false);
    check.font_cache_used_after_rerender = SkGraphics::GetFontCacheUsed();
    fresh.renderer.reset();
    fresh.tree.reset();

    const bool controls_differ =
        check.checksum_cold_control != check.checksum_cold_control_alternate;
    const bool purge_reduced_nonempty_cache =
        check.font_cache_used_before_purge > 0 &&
        (check.font_cache_used_after_purge == 0 ||
         check.font_cache_used_after_purge < check.font_cache_used_before_purge);
    check.passed = controls_differ && purge_reduced_nonempty_cache &&
                   check.checksum_after_purge == check.checksum_cold_control_alternate;
    check.detail = check.passed
        ? "purge on the rendering thread strictly reduced a non-empty cache after all "
          "renderers were destroyed; re-rendered alternate content matched its cold control"
        : "purge was a no-op on an empty cache, or re-rendered alternate content "
          "diverged from its cold control";
    return check;
}

// ---------------------------------------------------------------------------
// Result document.
// ---------------------------------------------------------------------------

struct Document {
    std::string variant;
    std::string skia_archive_sha256;
    bool runtime_threadlocal_default{};
    bool runtime_threadlocal_effective{};
    std::vector<int> requested_threads;
    std::vector<RunResult> runs;
    std::vector<CheckRecord> checks;
};

std::string variant_note(std::string_view variant) {
    if (variant == "A") {
        return "pinned skia-builder chrome/m153 archive; thread-local strike cache disabled";
    }
    if (variant == "B") {
        return "experimental skia-builder archive built with the thread-local strike cache enabled";
    }
    return "pinned chrome/m153 archive with the exported experimental runtime selector forced true";
}

std::string font_cache_scope(bool threadlocal_effective) {
    return threadlocal_effective ? "per-thread" : "process-global";
}

std::string to_json(const Document& document) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": " << json_quote(kSchema) << ",\n";
    out << "  \"probe\": \"nativeui_t440_strikecache_probe\",\n";
    out << "  \"nativeui_commit_sha\": " << json_quote(NATIVEUI_BENCHMARK_COMMIT_SHA) << ",\n";
    out << "  \"skia_archive_sha256\": " << json_quote(document.skia_archive_sha256) << ",\n";
    out << "  \"variant\": " << json_quote(document.variant) << ",\n";
    out << "  \"variant_note\": " << json_quote(variant_note(document.variant)) << ",\n";
    out << "  \"runtime_threadlocal_default\": "
        << (document.runtime_threadlocal_default ? "true" : "false") << ",\n";
    out << "  \"runtime_threadlocal_effective\": "
        << (document.runtime_threadlocal_effective ? "true" : "false") << ",\n";
    out << "  \"font_cache_scope\": "
        << json_quote(font_cache_scope(document.runtime_threadlocal_effective)) << ",\n";
    out << "  \"skia_milestone\": " << SK_MILESTONE << ",\n";
    out << "  \"compiler_id\": " << json_quote(NATIVEUI_BENCHMARK_COMPILER_ID) << ",\n";
    out << "  \"compiler_version\": " << json_quote(NATIVEUI_BENCHMARK_COMPILER_VERSION) << ",\n";
    out << "  \"compiler_major\": " << NATIVEUI_BENCHMARK_COMPILER_MAJOR << ",\n";
    out << "  \"build_type\": " << json_quote(NATIVEUI_BENCHMARK_BUILD_TYPE) << ",\n";
    out << "  \"os\": " << json_quote(NATIVEUI_BENCHMARK_OS) << ",\n";
    out << "  \"architecture\": " << json_quote(NATIVEUI_BENCHMARK_ARCH) << ",\n";
    out << "  \"warmup_samples\": " << kWarmupSamples << ",\n";
    out << "  \"measured_samples\": " << kMeasuredSamples << ",\n";

    out << "  \"requested_threads\": [";
    for (std::size_t index = 0; index < document.requested_threads.size(); ++index) {
        if (index > 0) out << ", ";
        out << document.requested_threads[index];
    }
    out << "],\n";

    out << "  \"workloads\": [\n";
    for (std::size_t index = 0; index < kWorkloads.size(); ++index) {
        const auto& workload = kWorkloads[index];
        out << "    {\"name\": " << json_quote(workload.name)
            << ", \"operations_per_sample\": " << workload.operations_per_sample
            << ", \"font_sizes\": " << workload.font_size_count << "}";
        out << (index + 1 == kWorkloads.size() ? "\n" : ",\n");
    }
    out << "  ],\n";

    out << "  \"runs\": [\n";
    for (std::size_t index = 0; index < document.runs.size(); ++index) {
        const auto& run = document.runs[index];
        out << "    {\n";
        out << "      \"threads\": " << run.thread_count << ",\n";
        out << "      \"workload\": " << json_quote(run.workload) << ",\n";
        out << "      \"operations_per_sample\": " << run.operations_per_sample << ",\n";
        out << "      \"font_cache_scope\": "
            << json_quote(font_cache_scope(run.threadlocal_effective)) << ",\n";
        out << "      \"aggregate_median_ns_per_op\": " << json_number(run.aggregate.median_ns_per_op) << ",\n";
        out << "      \"aggregate_p95_ns_per_op\": " << json_number(run.aggregate.p95_ns_per_op) << ",\n";
        out << "      \"aggregate_min_ns_per_op\": " << json_number(run.aggregate.min_ns_per_op) << ",\n";
        out << "      \"aggregate_max_ns_per_op\": " << json_number(run.aggregate.max_ns_per_op) << ",\n";
        out << "      \"aggregate_throughput_ops_per_second\": "
            << json_number(1.0e9 / run.aggregate.median_ns_per_op) << ",\n";

        out << "      \"thread_median_ns_per_op\": [";
        for (std::size_t thread = 0; thread < run.thread_median_ns_per_op.size(); ++thread) {
            if (thread > 0) out << ", ";
            out << json_number(run.thread_median_ns_per_op[thread]);
        }
        out << "],\n";

        out << "      \"thread_p95_ns_per_op\": [";
        for (std::size_t thread = 0; thread < run.thread_p95_ns_per_op.size(); ++thread) {
            if (thread > 0) out << ", ";
            out << json_number(run.thread_p95_ns_per_op[thread]);
        }
        out << "],\n";

        out << "      \"font_cache_used_before_bytes\": [";
        for (std::size_t thread = 0; thread < run.font_cache_used_before_bytes.size(); ++thread) {
            if (thread > 0) out << ", ";
            out << run.font_cache_used_before_bytes[thread];
        }
        out << "],\n";

        out << "      \"font_cache_used_after_bytes\": [";
        for (std::size_t thread = 0; thread < run.font_cache_used_after_bytes.size(); ++thread) {
            if (thread > 0) out << ", ";
            out << run.font_cache_used_after_bytes[thread];
        }
        out << "],\n";

        out << "      \"font_cache_count_used_before\": [";
        for (std::size_t thread = 0; thread < run.font_cache_count_before.size(); ++thread) {
            if (thread > 0) out << ", ";
            out << run.font_cache_count_before[thread];
        }
        out << "],\n";

        out << "      \"font_cache_count_used_after\": [";
        for (std::size_t thread = 0; thread < run.font_cache_count_after.size(); ++thread) {
            if (thread > 0) out << ", ";
            out << run.font_cache_count_after[thread];
        }
        out << "],\n";

        // Only the scope-aware totals are comparable across variants: under
        // process-global scope the raw per-thread arrays repeat one shared
        // cache, so summing them would multiply the real value by the thread
        // count. The raw arrays remain in the document as per-thread evidence.
        out << "      \"font_cache_used_after_largest_thread_bytes\": "
            << font_cache_used_largest_thread(run) << ",\n";
        out << "      \"font_cache_used_after_total_bytes\": " << font_cache_used_total(run) << ",\n";
        out << "      \"font_cache_count_used_largest_thread\": " << font_cache_count_largest_thread(run) << ",\n";
        out << "      \"font_cache_count_used_total\": " << font_cache_count_total(run) << ",\n";
        out << "      \"font_cache_limit_bytes\": " << run.font_cache_limit_bytes << ",\n";
        out << "      \"font_cache_count_limit\": " << run.font_cache_count_limit << ",\n";

        const auto memory_delta = [](const MemorySnapshot& before, const MemorySnapshot& after) {
            return static_cast<std::int64_t>(after.bytes) - static_cast<std::int64_t>(before.bytes);
        };
        const auto resident_delta = [](const MemorySnapshot& before, const MemorySnapshot& after) {
            return static_cast<std::int64_t>(after.resident_bytes) -
                   static_cast<std::int64_t>(before.resident_bytes);
        };
        out << "      \"process_memory_available\": "
            << (run.memory_before.available && run.memory_after.available ? "true" : "false") << ",\n";
        out << "      \"process_memory_metric\": "
            << json_quote(run.memory_before.available ? run.memory_before.metric : "unavailable") << ",\n";
        out << "      \"process_memory_before_bytes\": " << run.memory_before.bytes << ",\n";
        out << "      \"process_memory_after_bytes\": " << run.memory_after.bytes << ",\n";
        out << "      \"process_memory_delta_bytes\": "
            << memory_delta(run.memory_before, run.memory_after) << ",\n";
        out << "      \"process_resident_before_bytes\": " << run.memory_before.resident_bytes << ",\n";
        out << "      \"process_resident_after_bytes\": " << run.memory_after.resident_bytes << ",\n";
        out << "      \"process_resident_delta_bytes\": "
            << resident_delta(run.memory_before, run.memory_after) << "\n";
        out << "    }";
        out << (index + 1 == document.runs.size() ? "\n" : ",\n");
    }
    out << "  ],\n";

    out << "  \"checks\": [\n";
    for (std::size_t index = 0; index < document.checks.size(); ++index) {
        const auto& check = document.checks[index];
        out << "    {\n";
        out << "      \"name\": " << json_quote(check.name) << ",\n";
        out << "      \"passed\": " << (check.passed ? "true" : "false") << ",\n";
        out << "      \"cycles\": " << check.cycles << ",\n";
        out << "      \"checksum_control\": " << json_quote(check.checksum_control) << ",\n";
        out << "      \"checksum_control_alternate\": "
            << json_quote(check.checksum_control_alternate) << ",\n";
        out << "      \"checksum_survivor\": " << json_quote(check.checksum_survivor) << ",\n";
        out << "      \"checksum_cold_control\": " << json_quote(check.checksum_cold_control) << ",\n";
        out << "      \"checksum_cold_control_alternate\": "
            << json_quote(check.checksum_cold_control_alternate) << ",\n";
        out << "      \"checksum_after_purge\": " << json_quote(check.checksum_after_purge) << ",\n";
        out << "      \"font_cache_used_before_purge\": " << check.font_cache_used_before_purge << ",\n";
        out << "      \"font_cache_used_after_purge\": " << check.font_cache_used_after_purge << ",\n";
        out << "      \"font_cache_used_after_rerender\": " << check.font_cache_used_after_rerender << ",\n";
        out << "      \"purge_on_rendering_thread\": "
            << (check.purge_on_rendering_thread ? "true" : "false") << ",\n";
        out << "      \"detail\": " << json_quote(check.detail) << "\n";
        out << "    }";
        out << (index + 1 == document.checks.size() ? "\n" : ",\n");
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

// ---------------------------------------------------------------------------
// Self-test and command line.
// ---------------------------------------------------------------------------

std::vector<int> parse_threads(std::string_view value) {
    if (value.empty()) {
        throw std::invalid_argument("--threads requires a non-empty comma-separated list");
    }
    std::vector<int> threads;
    std::size_t start = 0;
    while (true) {
        const auto comma = value.find(',', start);
        const auto token = value.substr(
            start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
        if (token.empty()) {
            throw std::invalid_argument("--threads contains an empty entry");
        }
        int count = 0;
        for (const char ch : token) {
            if (ch < '0' || ch > '9') {
                throw std::invalid_argument("--threads accepts positive integers only");
            }
            count = count * 10 + (ch - '0');
            if (count > kMaximumThreadCount) {
                throw std::invalid_argument("--threads exceeds the supported worker count");
            }
        }
        if (count < 1) {
            throw std::invalid_argument("--threads entries must be positive");
        }
        threads.push_back(count);
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    std::sort(threads.begin(), threads.end());
    if (std::adjacent_find(threads.begin(), threads.end()) != threads.end()) {
        throw std::invalid_argument("--threads contains duplicate entries");
    }
    return threads;
}

std::string parse_variant(std::string_view value) {
    if (value == "A" || value == "B" || value == "C") return std::string(value);
    throw std::invalid_argument("--variant must be A, B or C");
}

std::string parse_archive_sha256(std::string_view value) {
    if (value.size() != 64) {
        throw std::invalid_argument("--skia-archive-sha256 must be 64 hexadecimal characters");
    }
    std::string normalized;
    normalized.reserve(64);
    for (const char ch : value) {
        if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')) {
            normalized.push_back(ch);
            continue;
        }
        if (ch >= 'A' && ch <= 'F') {
            normalized.push_back(static_cast<char>(ch - 'A' + 'a'));
            continue;
        }
        throw std::invalid_argument("--skia-archive-sha256 must be 64 hexadecimal characters");
    }
    return normalized;
}

struct Options {
    bool self_test{};
    std::string json_path;
    std::string skia_archive_sha256;
    std::vector<int> threads{1, 2, 4, 8};
    std::string variant{"A"};
};

Options parse_options(int argc, char** argv) {
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--self-test") {
            options.self_test = true;
        } else if (argument == "--json") {
            if (++index >= argc) throw std::invalid_argument("--json requires a path");
            options.json_path = argv[index];
        } else if (argument == "--skia-archive-sha256") {
            if (++index >= argc) {
                throw std::invalid_argument("--skia-archive-sha256 requires a value");
            }
            options.skia_archive_sha256 = parse_archive_sha256(argv[index]);
        } else if (argument == "--threads") {
            if (++index >= argc) throw std::invalid_argument("--threads requires a value");
            options.threads = parse_threads(argv[index]);
        } else if (argument == "--variant") {
            if (++index >= argc) throw std::invalid_argument("--variant requires a value");
            options.variant = parse_variant(argv[index]);
        } else {
            throw std::invalid_argument("unknown T440 probe argument: " + std::string(argument));
        }
    }
    return options;
}

Document make_self_test_document() {
    Document document;
    document.variant = "A";
    document.skia_archive_sha256 =
        "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    document.runtime_threadlocal_default = false;
    document.runtime_threadlocal_effective = false;
    document.requested_threads = {1};

    RunResult run;
    run.thread_count = 1;
    run.workload = std::string(kTextLayoutPaint);
    run.operations_per_sample = kWorkloads.front().operations_per_sample;
    run.threadlocal_effective = false;
    run.aggregate = SampleSummary{
        .median_ns_per_op = 100.0,
        .p95_ns_per_op = 120.0,
        .min_ns_per_op = 90.0,
        .max_ns_per_op = 130.0,
    };
    run.thread_median_ns_per_op = {100.0};
    run.thread_p95_ns_per_op = {120.0};
    run.font_cache_used_before_bytes = {0};
    run.font_cache_used_after_bytes = {1024};
    run.font_cache_count_before = {0};
    run.font_cache_count_after = {1};
    run.font_cache_limit_bytes = 2 * 1024 * 1024;
    run.font_cache_count_limit = 2048;
    run.memory_before = MemorySnapshot{true, "test_metric", 1000, 2000};
    run.memory_after = MemorySnapshot{true, "test_metric", 1500, 2500};
    document.runs.push_back(std::move(run));

    CheckRecord lifecycle;
    lifecycle.name = "multi_instance_lifecycle";
    lifecycle.passed = true;
    lifecycle.cycles = 1;
    lifecycle.checksum_control = "fnv1a64:0000000000000001";
    lifecycle.checksum_control_alternate = "fnv1a64:0000000000000003";
    lifecycle.checksum_survivor = lifecycle.checksum_control;
    lifecycle.detail = "self-test";
    document.checks.push_back(lifecycle);

    CheckRecord staleness;
    staleness.name = "cache_staleness_after_destruction";
    staleness.passed = true;
    staleness.checksum_cold_control = "fnv1a64:0000000000000002";
    staleness.checksum_cold_control_alternate = "fnv1a64:0000000000000004";
    staleness.checksum_after_purge = staleness.checksum_cold_control_alternate;
    staleness.purge_on_rendering_thread = true;
    staleness.detail = "self-test";
    document.checks.push_back(staleness);

    return document;
}

int run_self_test() {
    // Frozen protocol constants are shared with T051.
    if (kWarmupSamples != 5 || kMeasuredSamples != 30) {
        throw std::runtime_error("T440 self-test: frozen T051 sample counts drifted");
    }
    if (kWorkloads.size() != 2 || kWorkloads[0].name != kTextLayoutPaint ||
        kWorkloads[1].name != kFontSizeChurn) {
        throw std::runtime_error("T440 self-test: workload table drifted");
    }
    if (kWorkloads[0].operations_per_sample != 50 || kWorkloads[1].operations_per_sample != 24) {
        throw std::runtime_error("T440 self-test: workload batch counts drifted");
    }
    if (kWorkloads[1].font_size_count != kChurnFontSizes.size()) {
        throw std::runtime_error("T440 self-test: churn size count drifted");
    }

    // summarize_samples() stays the frozen T051 summary function.
    {
        std::vector<double> samples;
        samples.reserve(kMeasuredSamples);
        for (int value = 1; value <= kMeasuredSamples; ++value) {
            samples.push_back(static_cast<double>(value));
        }
        const auto summary = summarize_samples(samples);
        if (summary.median_ns_per_op != 15.5 || summary.p95_ns_per_op != 29.0) {
            throw std::runtime_error("T440 self-test: frozen summarize_samples semantics drifted");
        }
    }

    // Option parsing stays deterministic and rejects malformed input.
    {
        const auto parsed = parse_threads("2,1,4,8");
        if (parsed != std::vector<int>({1, 2, 4, 8})) {
            throw std::runtime_error("T440 self-test: --threads parsing drifted");
        }
        bool rejected = false;
        try { (void)parse_threads("1,1"); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("T440 self-test: duplicate threads accepted");
        rejected = false;
        try { (void)parse_threads("0"); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("T440 self-test: zero threads accepted");
        rejected = false;
        try { (void)parse_variant("D"); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("T440 self-test: unknown variant accepted");
        if (parse_archive_sha256(std::string(64, 'A')) != std::string(64, 'a')) {
            throw std::runtime_error("T440 self-test: --skia-archive-sha256 was not normalized");
        }
        rejected = false;
        try { (void)parse_archive_sha256("abcd"); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("T440 self-test: short --skia-archive-sha256 accepted");
        rejected = false;
        try { (void)parse_archive_sha256(std::string(64, 'g')); } catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("T440 self-test: non-hex --skia-archive-sha256 accepted");
    }

    // Deterministic paint: two independent fixtures must produce identical
    // pixels, and every timed workload must execute one full operation batch.
    {
        auto first = make_text_fixture(kFixedFontSize);
        auto second = make_text_fixture(kFixedFontSize);
        const auto first_checksum = render_fixture_checksum(first, true);
        const auto second_checksum = render_fixture_checksum(second, true);
        if (first_checksum.empty() || first_checksum != second_checksum) {
            throw std::runtime_error("T440 self-test: deterministic paint checksum drifted");
        }
    }

    // The isolation and staleness checks rely on alternate content being
    // visually distinct; a colliding checksum would make them vacuous.
    {
        auto fixed = make_text_fixture(kFixedFontSize);
        auto alternate = make_text_fixture(kAlternateFontSize);
        const auto fixed_checksum = render_fixture_checksum(fixed, true);
        const auto alternate_checksum = render_fixture_checksum(alternate, true);
        if (fixed_checksum.empty() || fixed_checksum == alternate_checksum) {
            throw std::runtime_error(
                "T440 self-test: alternate font size did not produce distinct pixels");
        }
    }

    // The strengthened staleness assertion requires a purge to strictly reduce
    // a non-empty cache; both halves are deterministic and wall-clock free.
    {
        SkGraphics::PurgeFontCache();
        auto fixture = make_text_fixture(kFixedFontSize);
        (void)render_fixture_checksum(fixture, true);
        const auto used_before = SkGraphics::GetFontCacheUsed();
        SkGraphics::PurgeFontCache();
        const auto used_after = SkGraphics::GetFontCacheUsed();
        if (used_before == 0) {
            throw std::runtime_error("T440 self-test: text render did not populate the strike cache");
        }
        if (!(used_after == 0 || used_after < used_before)) {
            throw std::runtime_error("T440 self-test: purge did not reduce the strike cache");
        }
    }

    {
        WorkerContext context;
        context.workload = &kWorkloads[0];
        context.fixture = std::make_unique<TextFixture>(make_text_fixture(kFixedFontSize));
        if (!render(*context.fixture)) {
            throw std::runtime_error("T440 self-test: text_layout_paint warmup render failed");
        }
        for (std::uint64_t operation = 0; operation < kWorkloads[0].operations_per_sample; ++operation) {
            run_text_operation(context);
        }
    }

    {
        WorkerContext context;
        context.workload = &kWorkloads[1];
        context.fixture = std::make_unique<TextFixture>(make_text_fixture(kChurnFontSizes.front()));
        if (!render(*context.fixture)) {
            throw std::runtime_error("T440 self-test: font_size_churn warmup render failed");
        }
        reset_before_sample(context);
        for (std::uint64_t operation = 0; operation < kWorkloads[1].operations_per_sample; ++operation) {
            run_text_operation(context);
        }
        if (context.churn_cursor != 0) {
            throw std::runtime_error("T440 self-test: churn cycle did not wrap cleanly");
        }
    }

    const auto lifecycle = run_multi_instance_lifecycle_check();
    if (!lifecycle.passed) {
        throw std::runtime_error("T440 self-test: multi-instance lifecycle check failed");
    }
    const auto staleness = run_cache_staleness_check();
    if (!staleness.passed) {
        throw std::runtime_error("T440 self-test: cache staleness check failed");
    }

    const auto document = make_self_test_document();
    const auto json = to_json(document);
    if (!JsonSyntaxValidator::is_valid(json)) {
        throw std::runtime_error("T440 self-test: result document is not valid JSON");
    }
    for (const std::string_view needle : {
             "\"schema\": \"t440-strikecache-v2\"",
             "\"variant\": \"A\"",
             "\"variant_note\":",
             "\"skia_archive_sha256\": \"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\"",
             "\"font_cache_scope\": \"process-global\"",
             "\"font_cache_used_after_total_bytes\": 1024",
             "\"font_cache_used_after_largest_thread_bytes\": 1024",
             "\"font_cache_count_used_total\": 1",
             "\"checksum_control_alternate\": \"fnv1a64:0000000000000003\"",
             "\"checksum_cold_control_alternate\": \"fnv1a64:0000000000000004\"",
             "\"process_memory_delta_bytes\": 500",
             "\"process_resident_delta_bytes\": 500",
             "\"multi_instance_lifecycle\"",
             "\"cache_staleness_after_destruction\"",
         }) {
        if (json.find(needle) == std::string::npos) {
            throw std::runtime_error("T440 self-test: result document missing expected field");
        }
    }

#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
    if (!sample_process_memory().available) {
        throw std::runtime_error("T440 self-test: process memory sampling unavailable");
    }
#endif

    std::cout << "PASS T440 strike cache probe self-test\n";
    return EXIT_SUCCESS;
}

void check_variant_provenance(std::string_view variant, bool runtime_default) {
    if (variant == "A" && runtime_default) {
        throw std::runtime_error(
            "--variant A requested but the linked Skia archive was built with the "
            "thread-local strike cache enabled; use --variant B for that archive");
    }
    if (variant == "B" && !runtime_default) {
        throw std::runtime_error(
            "--variant B requested but the linked Skia archive was not built with the "
            "thread-local strike cache enabled; a skia-builder variant-B archive is required");
    }
}

int run_research(const Options& options) {
    // Provenance: read the exported runtime selector before any Skia text API
    // runs, then apply the requested variant.
    const bool runtime_default = gSkUseThreadLocalStrikeCaches_IAcknowledgeThisIsIncrediblyExperimental;
    if (options.variant == "C") {
        gSkUseThreadLocalStrikeCaches_IAcknowledgeThisIsIncrediblyExperimental = true;
    }
    const bool runtime_effective = gSkUseThreadLocalStrikeCaches_IAcknowledgeThisIsIncrediblyExperimental;
    check_variant_provenance(options.variant, runtime_default);

    std::cout << "T440 strike cache probe"
              << " variant=" << options.variant
              << " runtime_threadlocal_default=" << (runtime_default ? "true" : "false")
              << " runtime_threadlocal_effective=" << (runtime_effective ? "true" : "false")
              << " skia_milestone=" << SK_MILESTONE
              << " commit=" << NATIVEUI_BENCHMARK_COMMIT_SHA << '\n';

    Document document;
    document.variant = options.variant;
    document.skia_archive_sha256 = options.skia_archive_sha256;
    document.runtime_threadlocal_default = runtime_default;
    document.runtime_threadlocal_effective = runtime_effective;
    document.requested_threads = options.threads;

    for (const auto& workload : kWorkloads) {
        for (const int thread_count : options.threads) {
            auto result = run_concurrent(workload, thread_count, runtime_effective);
            std::cout << "  " << result.workload
                      << " threads=" << thread_count
                      << " median=" << result.aggregate.median_ns_per_op << "ns/op"
                      << " p95=" << result.aggregate.p95_ns_per_op << "ns/op"
                      << " throughput=" << (1.0e9 / result.aggregate.median_ns_per_op) << " ops/s"
                      << " font_cache_scope=" << font_cache_scope(result.threadlocal_effective)
                      << " font_cache_used_total=" << font_cache_used_total(result) << "B"
                      << " font_cache_count_total=" << font_cache_count_total(result) << '\n';
            document.runs.push_back(std::move(result));
        }
    }

    document.checks.push_back(run_multi_instance_lifecycle_check());
    document.checks.push_back(run_cache_staleness_check());
    for (const auto& check : document.checks) {
        std::cout << "  check " << check.name << ": "
                  << (check.passed ? "PASS" : "FAIL") << " (" << check.detail << ")\n";
    }

    const auto json = to_json(document);
    if (!options.json_path.empty()) {
        std::ofstream output(options.json_path, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("unable to open T440 JSON output: " + options.json_path);
        }
        output << json;
        output.flush();
        if (!output) {
            throw std::runtime_error("failed writing T440 JSON output: " + options.json_path);
        }
        output.close();
        if (!output) {
            throw std::runtime_error("failed closing T440 JSON output: " + options.json_path);
        }
        // A stream can report success while the final bytes never reached the
        // file; decision-grade evidence must be complete or the run fails.
        std::error_code size_error;
        const auto written_size = std::filesystem::file_size(options.json_path, size_error);
        if (size_error || written_size != static_cast<std::uintmax_t>(json.size())) {
            throw std::runtime_error(
                "T440 JSON output is truncated or unreadable: " + options.json_path);
        }
        std::cout << "  wrote " << options.json_path << " (" << written_size << " bytes)\n";
    } else {
        std::cout << json;
    }

    for (const auto& check : document.checks) {
        if (!check.passed) return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        if (options.self_test) return run_self_test();
        if (std::string_view(NATIVEUI_BENCHMARK_BUILD_TYPE) != "Release") {
            throw std::runtime_error("T440 strike cache probe requires a Release build");
        }
        return run_research(options);
    } catch (const std::exception& error) {
        std::cerr << "T440 strike cache probe failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
