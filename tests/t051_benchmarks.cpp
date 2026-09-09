#include "benchmarks/t051_benchmark_harness.hpp"
#include "test_support.hpp"

#include <nativeui/nativeui.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <malloc.h>
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

// T051 deliberately keeps allocation interception in this benchmark-only
// executable. Production NativeUI allocator/lifetime contracts remain untouched.
namespace benchmark_allocation {

bool enabled{};
std::uint64_t allocations{};
std::uint64_t bytes{};

void record(std::size_t size) noexcept {
    if (!enabled) return;
    ++allocations;
    bytes += static_cast<std::uint64_t>(size);
}

void* allocate(std::size_t size) {
    size = size == 0 ? 1 : size;
    if (void* memory = std::malloc(size)) {
        record(size);
        return memory;
    }
    throw std::bad_alloc{};
}

void* allocate_aligned(std::size_t size, std::size_t alignment) {
    size = size == 0 ? 1 : size;
#if defined(_MSC_VER)
    if (void* memory = _aligned_malloc(size, alignment)) {
        record(size);
        return memory;
    }
#else
    void* memory = nullptr;
    if (posix_memalign(&memory, alignment, size) == 0 && memory != nullptr) {
        record(size);
        return memory;
    }
#endif
    throw std::bad_alloc{};
}

void deallocate_aligned(void* memory) noexcept {
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

struct Snapshot {
    std::uint64_t allocations{};
    std::uint64_t bytes{};
};

class Scope {
public:
    Scope() {
        allocations = 0;
        bytes = 0;
        enabled = true;
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

    ~Scope() { enabled = false; }

    Snapshot finish() noexcept {
        enabled = false;
        return Snapshot{allocations, bytes};
    }
};

} // namespace benchmark_allocation

void* operator new(std::size_t size) { return benchmark_allocation::allocate(size); }
void* operator new[](std::size_t size) { return benchmark_allocation::allocate(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void* operator new(std::size_t size, std::align_val_t alignment) {
    return benchmark_allocation::allocate_aligned(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
    return benchmark_allocation::allocate_aligned(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* memory, std::align_val_t) noexcept {
    benchmark_allocation::deallocate_aligned(memory);
}
void operator delete[](void* memory, std::align_val_t) noexcept {
    benchmark_allocation::deallocate_aligned(memory);
}
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept {
    benchmark_allocation::deallocate_aligned(memory);
}
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
    benchmark_allocation::deallocate_aligned(memory);
}

namespace {

using nativeui::bench::ResultRecord;
using nativeui::bench::WorkloadSpec;

struct SpecRoot {
    ui::Spec value;
    ui::Spec spec() && { return std::move(value); }
};

class BenchLeafComponent final : public ui::Component {
public:
    BenchLeafComponent(ui::Size size, bool focusable, bool paint_text, unsigned variant)
        : size_(size), focusable_(focusable), paint_text_(paint_text), variant_(variant) {}

    [[nodiscard]] bool focusable() const noexcept override { return focusable_; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return size_;
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        switch (event.type) {
        case ui::InputType::PointerDown:
        case ui::InputType::PointerMove:
        case ui::InputType::PointerUp:
        case ui::InputType::KeyDown:
        case ui::InputType::KeyUp:
            event_count_ += static_cast<std::uint64_t>(event.type) + 1u;
            return ui::EventResult::Handled;
        default:
            return ui::EventResult::Ignored;
        }
    }

    void paint(ui::PaintContext& context) const override {
        const auto bounds = context.bounds();
        const auto color = (variant_ % 3u == 0u)
                               ? ui::colors::panel
                               : ((variant_ % 3u == 1u) ? ui::colors::knob : ui::colors::track);
        context.painter().fill_rounded_rect(bounds, 2.0f, color);
        if (paint_text_) {
            context.painter().text(
                {bounds.x + 3.0f, bounds.y + bounds.h * 0.5f},
                "NativeUI deterministic benchmark text",
                11.0f,
                ui::colors::text);
        }
    }

private:
    ui::Size size_{};
    bool focusable_{};
    bool paint_text_{};
    unsigned variant_{};
    std::uint64_t event_count_{};
};

ui::Spec make_leaf(ui::Size size = {32.0f, 24.0f},
                   bool focusable = false,
                   bool paint_text = false,
                   unsigned variant = 0u) {
    return ui::Spec{
        [size, focusable, paint_text, variant] {
            return std::make_unique<BenchLeafComponent>(size, focusable, paint_text, variant);
        },
        {}};
}

ui::GridTracks grid_tracks(std::size_t columns, std::size_t rows) {
    ui::GridTracks tracks;
    tracks.columns.reserve(columns);
    tracks.rows.reserve(rows);
    for (std::size_t i = 0; i < columns; ++i) tracks.columns.push_back(ui::Track::auto_size());
    for (std::size_t i = 0; i < rows; ++i) tracks.rows.push_back(ui::Track::auto_size());
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

ui::Spec make_layout_spec(int groups, bool focusable, bool paint_text) {
    std::vector<ui::Spec> root_children;
    root_children.reserve(static_cast<std::size_t>(groups));
    unsigned variant = 0u;
    for (int group = 0; group < groups; ++group) {
        std::vector<ui::Spec> row_children;
        row_children.reserve(2);
        for (int grid = 0; grid < 2; ++grid) {
            std::vector<ui::Spec> leaves;
            leaves.reserve(10);
            for (int leaf = 0; leaf < 10; ++leaf) {
                leaves.push_back(make_leaf({38.0f, 24.0f}, focusable, paint_text, variant++));
            }
            row_children.push_back(make_grid(5, 2, std::move(leaves)));
        }
        root_children.push_back(make_row(std::move(row_children)));
    }
    return make_column(std::move(root_children));
}

ui::Spec make_interactive_grid_spec() {
    std::vector<ui::Spec> leaves;
    leaves.reserve(100);
    for (unsigned i = 0; i < 100; ++i) {
        leaves.push_back(make_leaf({80.0f, 60.0f}, true, false, i));
    }
    return make_grid(10, 10, std::move(leaves), 0.0f);
}

ui::Spec wrap_one(ui::Spec child, bool clip) {
    std::vector<ui::Spec> children;
    children.push_back(std::move(child));
    if (clip) {
        return ui::Spec{[] { return std::make_unique<ui::ClipComponent>(); }, std::move(children)};
    }
    return ui::Spec{[] { return std::make_unique<ui::PaddingComponent>(1.0f); }, std::move(children)};
}

ui::Spec make_deep_spec() {
    auto node = make_leaf({96.0f, 48.0f}, true, false, 0u);
    for (int depth = 0; depth < 32; ++depth) {
        node = wrap_one(std::move(node), (depth % 2) == 0);
    }
    return node;
}

std::unique_ptr<ui::UI> make_ui(ui::Spec spec) {
    return std::make_unique<ui::UI>(SpecRoot{std::move(spec)});
}

struct BenchCase {
    WorkloadSpec contract;
    std::uint64_t node_count{};
    std::uint64_t actions_per_operation{};
    int logical_width{};
    int logical_height{};
    std::function<void()> before_sample;
    std::function<void()> operation;
};

struct TreeState {
    std::unique_ptr<ui::UI> tree;
    test::MockPlatform platform;
    std::size_t cursor{};
};

BenchCase make_layout_case(std::string_view name, int groups, std::uint64_t operations) {
    auto state = std::make_shared<TreeState>();
    state->tree = make_ui(make_layout_spec(groups, false, false));
    state->tree->resize({1024.0f, 768.0f});
    return BenchCase{
        .contract = {name, operations, true},
        .node_count = 1u + static_cast<std::uint64_t>(groups) * 23u,
        .actions_per_operation = 1,
        .logical_width = 1024,
        .logical_height = 768,
        .before_sample = [] {},
        .operation = [state] {
            state->tree->invalidate_layout();
            (void)state->tree->measure(ui::Constraints::tight({1024.0f, 768.0f}));
            state->tree->resize({1024.0f, 768.0f});
        },
    };
}

BenchCase make_hit_test_case() {
    auto state = std::make_shared<TreeState>();
    state->tree = make_ui(make_deep_spec());
    state->tree->resize({512.0f, 512.0f});
    constexpr std::array<ui::Point, 8> positions{{
        {32.0f, 32.0f}, {64.0f, 64.0f}, {96.0f, 96.0f}, {128.0f, 128.0f},
        {160.0f, 160.0f}, {192.0f, 192.0f}, {224.0f, 224.0f}, {256.0f, 256.0f},
    }};
    return BenchCase{
        .contract = {"hit_test_deep", 2000, true},
        .node_count = 33,
        .actions_per_operation = 1,
        .logical_width = 512,
        .logical_height = 512,
        .before_sample = [state] { state->cursor = 0; },
        .operation = [state] {
            const auto position = positions[state->cursor++ % positions.size()];
            ui::InputEvent event{};
            event.type = ui::InputType::PointerMove;
            event.position = position;
            (void)state->tree->dispatch(event, state->platform);
        },
    };
}

BenchCase make_pointer_case() {
    auto state = std::make_shared<TreeState>();
    state->tree = make_ui(make_interactive_grid_spec());
    state->tree->resize({800.0f, 600.0f});
    return BenchCase{
        .contract = {"dispatch_pointer", 2000, true},
        .node_count = 101,
        .actions_per_operation = 1,
        .logical_width = 800,
        .logical_height = 600,
        .before_sample = [state] { state->cursor = 0; },
        .operation = [state] {
            const auto index = state->cursor++ % 100u;
            const float x = static_cast<float>((index % 10u) * 80u + 40u);
            const float y = static_cast<float>((index / 10u) * 60u + 30u);
            ui::InputEvent event{};
            event.type = ui::InputType::PointerMove;
            event.position = {x, y};
            (void)state->tree->dispatch(event, state->platform);
        },
    };
}

BenchCase make_keyboard_case() {
    auto state = std::make_shared<TreeState>();
    state->tree = make_ui(make_leaf({120.0f, 40.0f}, true, false, 0u));
    state->tree->resize({320.0f, 120.0f});
    state->tree->activate(state->platform);
    return BenchCase{
        .contract = {"dispatch_keyboard", 1000, true},
        .node_count = 1,
        .actions_per_operation = 2,
        .logical_width = 320,
        .logical_height = 120,
        .before_sample = [] {},
        .operation = [state] {
            ui::InputEvent down{};
            down.type = ui::InputType::KeyDown;
            down.key = ui::Key::Right;
            ui::InputEvent up = down;
            up.type = ui::InputType::KeyUp;
            (void)state->tree->dispatch(down, state->platform);
            (void)state->tree->dispatch(up, state->platform);
        },
    };
}

struct TextState {
    explicit TextState(std::string initial) : model(std::move(initial)) {}
    ui::TextEditModel model;
};

BenchCase make_text_short_case() {
    constexpr std::string_view initial = "NativeUI short edit";
    auto state = std::make_shared<TextState>(std::string(initial));
    return BenchCase{
        .contract = {"text_edit_short", 500, true},
        .node_count = 0,
        .actions_per_operation = 8,
        .before_sample = [state] { state->model.set_text("NativeUI short edit", false, true); },
        .operation = [state] {
            state->model.move_to(state->model.text().size());
            (void)state->model.insert("x");
            (void)state->model.backspace();
            state->model.move_left();
            state->model.move_right();
            state->model.select_range(0, 1);
            state->model.collapse_to_end();
            state->model.move_to(state->model.text().size());
        },
    };
}

BenchCase make_text_multiline_case() {
    constexpr std::string_view initial =
        "line one deterministic\nline two benchmark\nline three retained editor";
    auto state = std::make_shared<TextState>(std::string(initial));
    return BenchCase{
        .contract = {"text_edit_multiline", 100, true},
        .node_count = 0,
        .actions_per_operation = 16,
        .before_sample = [state] {
            state->model.set_text(
                "line one deterministic\nline two benchmark\nline three retained editor", false, true);
        },
        .operation = [state] {
            state->model.move_to(state->model.text().size());
            (void)state->model.insert("x");
            (void)state->model.backspace();
            state->model.move_up();
            state->model.move_down();
            state->model.move_line_start();
            state->model.move_line_end();
            state->model.move_left(ui::TextMotion::Word);
            state->model.move_right(ui::TextMotion::Word);
            state->model.select_range(0, 4);
            state->model.collapse_to_end();
            state->model.move_to(state->model.text().size());
            state->model.move_left();
            state->model.move_right();
            state->model.select_all();
            state->model.collapse_to_end();
        },
    };
}

struct PaintState {
    std::unique_ptr<ui::UI> tree;
    std::unique_ptr<ui::HeadlessRenderer> renderer;
};

BenchCase make_paint_case(bool text) {
    const int groups = text ? 4 : 5;
    auto state = std::make_shared<PaintState>();
    state->tree = make_ui(make_layout_spec(groups, false, text));
    state->renderer = std::make_unique<ui::HeadlessRenderer>(ui::Size{1024.0f, 768.0f}, 1.0f);
    if (!state->renderer->render(*state->tree)) {
        throw std::runtime_error("T051 failed to initialize headless paint surface");
    }
    return BenchCase{
        .contract = {text ? "paint_text" : "paint_controls", text ? 50u : 20u, true},
        .node_count = 1u + static_cast<std::uint64_t>(groups) * 23u,
        .actions_per_operation = text ? 2u : 1u,
        .logical_width = 1024,
        .logical_height = 768,
        .before_sample = [] {},
        .operation = [state, text] {
            if (text) {
                state->tree->invalidate_layout();
                (void)state->tree->measure(ui::Constraints::tight({1024.0f, 768.0f}));
            } else {
                state->tree->invalidate();
            }
            if (!state->renderer->render(*state->tree)) {
                throw std::runtime_error("T051 headless paint failed");
            }
        },
    };
}

BenchCase make_multi_instance_case() {
    return BenchCase{
        .contract = {"multi_instance", 20, true},
        .node_count = 47,
        .actions_per_operation = 5,
        .logical_width = 640,
        .logical_height = 480,
        .before_sample = [] {},
        .operation = [] {
            test::MockPlatform platform;
            auto tree = make_ui(make_layout_spec(2, true, false));
            tree->resize({640.0f, 480.0f});
            tree->activate(platform);
            ui::InputEvent event{};
            event.type = ui::InputType::PointerMove;
            event.position = {24.0f, 24.0f};
            (void)tree->dispatch(event, platform);
            tree->deactivate(platform);
        },
    };
}

std::vector<BenchCase> make_timed_cases() {
    std::vector<BenchCase> cases;
    cases.reserve(10);
    cases.push_back(make_layout_case("layout_small", 5, 100));
    cases.push_back(make_layout_case("layout_large", 43, 20));
    cases.push_back(make_hit_test_case());
    cases.push_back(make_pointer_case());
    cases.push_back(make_keyboard_case());
    cases.push_back(make_text_short_case());
    cases.push_back(make_text_multiline_case());
    cases.push_back(make_paint_case(false));
    cases.push_back(make_paint_case(true));
    cases.push_back(make_multi_instance_case());
    return cases;
}

struct AllocationMetrics {
    double allocations_per_op{};
    double bytes_per_op{};
};

AllocationMetrics measure_allocations(BenchCase& benchmark) {
    for (int sample = 0; sample < nativeui::bench::kWarmupSamples; ++sample) {
        benchmark.before_sample();
        for (std::uint64_t op = 0; op < benchmark.contract.operations_per_sample; ++op) {
            benchmark.operation();
        }
    }

    std::uint64_t allocations = 0;
    std::uint64_t bytes = 0;
    for (int sample = 0; sample < nativeui::bench::kMeasuredSamples; ++sample) {
        benchmark.before_sample();
        benchmark_allocation::Scope scope;
        for (std::uint64_t op = 0; op < benchmark.contract.operations_per_sample; ++op) {
            benchmark.operation();
        }
        const auto snapshot = scope.finish();
        allocations += snapshot.allocations;
        bytes += snapshot.bytes;
    }

    const double operations = static_cast<double>(benchmark.contract.operations_per_sample) *
                              static_cast<double>(nativeui::bench::kMeasuredSamples);
    return AllocationMetrics{
        .allocations_per_op = static_cast<double>(allocations) / operations,
        .bytes_per_op = static_cast<double>(bytes) / operations,
    };
}

nativeui::bench::ComparisonMetadata metadata_for(const BenchCase& benchmark) {
    return nativeui::bench::ComparisonMetadata{
        .schema_version = nativeui::bench::kSchemaVersion,
        .workload_version = nativeui::bench::kWorkloadVersion,
        .benchmark_name = std::string(benchmark.contract.name),
        .os = NATIVEUI_BENCHMARK_OS,
        .architecture = NATIVEUI_BENCHMARK_ARCH,
        .compiler_id = NATIVEUI_BENCHMARK_COMPILER_ID,
        .compiler_major = NATIVEUI_BENCHMARK_COMPILER_MAJOR,
        .compiler_version = NATIVEUI_BENCHMARK_COMPILER_VERSION,
        .build_type = NATIVEUI_BENCHMARK_BUILD_TYPE,
        .operations_per_sample = benchmark.contract.operations_per_sample,
        .commit_sha = NATIVEUI_BENCHMARK_COMMIT_SHA,
    };
}

ResultRecord run_benchmark(BenchCase& benchmark) {
    const auto timing = nativeui::bench::run_fixed_protocol(
        benchmark.contract.operations_per_sample,
        benchmark.before_sample,
        benchmark.operation);
    const auto allocations = measure_allocations(benchmark);
    return ResultRecord{
        .metadata = metadata_for(benchmark),
        .node_count = benchmark.node_count,
        .actions_per_operation = benchmark.actions_per_operation,
        .logical_width = benchmark.logical_width,
        .logical_height = benchmark.logical_height,
        .warmup_samples = nativeui::bench::kWarmupSamples,
        .measured_samples = nativeui::bench::kMeasuredSamples,
        .timing = timing.summary,
        .allocation_metrics_available = true,
        .allocations_per_op = allocations.allocations_per_op,
        .bytes_allocated_per_op = allocations.bytes_per_op,
    };
}

ResultRecord run_idle_invalidation() {
    auto tree = make_ui(make_leaf({160.0f, 48.0f}, false, false, 0u));
    ui::HeadlessRenderer renderer{{320.0f, 120.0f}, 1.0f};
    if (!renderer.render(*tree)) {
        throw std::runtime_error("T051 idle fixture failed initial render");
    }

    test::MockPlatform platform;
    std::uint64_t invalidations = 0;
    tree->set_invalidation_callback([&](ui::Rect) { ++invalidations; });

    ui::InputEvent tick{};
    tick.type = ui::InputType::Tick;
    benchmark_allocation::Scope allocation_scope;
    for (std::uint64_t checkpoint = 0; checkpoint < 1000; ++checkpoint) {
        (void)tree->dispatch(tick, platform);
    }
    const auto allocation_snapshot = allocation_scope.finish();
    if (!nativeui::bench::idle_invalidation_passes(invalidations)) {
        throw std::runtime_error("idle_invalidation emitted redraw/layout invalidation");
    }

    return ResultRecord{
        .metadata = nativeui::bench::ComparisonMetadata{
            .schema_version = nativeui::bench::kSchemaVersion,
            .workload_version = nativeui::bench::kWorkloadVersion,
            .benchmark_name = "idle_invalidation",
            .os = NATIVEUI_BENCHMARK_OS,
            .architecture = NATIVEUI_BENCHMARK_ARCH,
            .compiler_id = NATIVEUI_BENCHMARK_COMPILER_ID,
            .compiler_major = NATIVEUI_BENCHMARK_COMPILER_MAJOR,
            .compiler_version = NATIVEUI_BENCHMARK_COMPILER_VERSION,
            .build_type = NATIVEUI_BENCHMARK_BUILD_TYPE,
            .operations_per_sample = 1000,
            .commit_sha = NATIVEUI_BENCHMARK_COMMIT_SHA,
        },
        .node_count = 1,
        .actions_per_operation = 1,
        .logical_width = 320,
        .logical_height = 120,
        .warmup_samples = 0,
        .measured_samples = 0,
        .timing = {},
        .allocation_metrics_available = true,
        .allocations_per_op = static_cast<double>(allocation_snapshot.allocations) / 1000.0,
        .bytes_allocated_per_op = static_cast<double>(allocation_snapshot.bytes) / 1000.0,
    };
}

bool selected(std::string_view name, std::string_view filter) {
    return filter.empty() || name.starts_with(filter);
}

void write_json_document(const std::string& path, const std::vector<ResultRecord>& results) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("unable to open benchmark JSON output: " + path);
    output << "{\n  \"schema_version\":" << nativeui::bench::kSchemaVersion << ",\n  \"results\":[\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        output << "    " << nativeui::bench::to_json(results[i]);
        if (i + 1 != results.size()) output << ',';
        output << '\n';
    }
    output << "  ]\n}\n";
}

void check_contract_shape(const std::vector<BenchCase>& cases) {
    if (cases.size() != 10) throw std::runtime_error("T051 timed workload count drifted");
    for (const auto& workload : nativeui::bench::kFixedWorkloads) {
        if (!workload.timed) continue;
        const auto it = std::find_if(cases.begin(), cases.end(), [&](const BenchCase& benchmark) {
            return benchmark.contract.name == workload.name;
        });
        if (it == cases.end()) throw std::runtime_error("missing T051 workload: " + std::string(workload.name));
        if (it->contract.operations_per_sample != workload.operations_per_sample) {
            throw std::runtime_error("T051 workload batch count drifted: " + std::string(workload.name));
        }
    }
}

int self_test() {
    auto cases = make_timed_cases();
    check_contract_shape(cases);

    for (auto& benchmark : cases) {
        benchmark.before_sample();
        benchmark.operation();
    }

    {
        benchmark_allocation::Scope scope;
        void* memory = ::operator new(64);
        ::operator delete(memory);
        const auto snapshot = scope.finish();
        if (snapshot.allocations != 1 || snapshot.bytes < 64) {
            throw std::runtime_error("T051 allocation interception self-test failed");
        }
    }

    ResultRecord schema_probe{
        .metadata = metadata_for(cases.front()),
        .node_count = cases.front().node_count,
        .actions_per_operation = cases.front().actions_per_operation,
        .logical_width = cases.front().logical_width,
        .logical_height = cases.front().logical_height,
        .timing = {.median_ns_per_op = 1.0, .p95_ns_per_op = 2.0, .min_ns_per_op = 0.5, .max_ns_per_op = 3.0},
        .allocation_metrics_available = true,
        .allocations_per_op = 0.0,
        .bytes_allocated_per_op = 0.0,
    };
    if (!(nativeui::bench::parse_result_json(nativeui::bench::to_json(schema_probe)) == schema_probe)) {
        throw std::runtime_error("T051 JSON schema round-trip self-test failed");
    }

    (void)run_idle_invalidation();
    std::cout << "PASS T051 benchmark self-test\n";
    return EXIT_SUCCESS;
}

struct Options {
    bool self_test{};
    std::string filter;
    std::string json_path;
};

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];
        if (argument == "--self-test") {
            options.self_test = true;
        } else if (argument == "--filter") {
            if (++i >= argc) throw std::invalid_argument("--filter requires a value");
            options.filter = argv[i];
        } else if (argument == "--json") {
            if (++i >= argc) throw std::invalid_argument("--json requires a path");
            options.json_path = argv[i];
        } else {
            throw std::invalid_argument("unknown T051 benchmark argument: " + std::string(argument));
        }
    }
    return options;
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto options = parse_options(argc, argv);
        if (options.self_test) return self_test();
        if (std::string_view(NATIVEUI_BENCHMARK_BUILD_TYPE) != "Release") {
            throw std::runtime_error("T051 benchmark executable requires a Release build");
        }

        auto cases = make_timed_cases();
        check_contract_shape(cases);
        std::vector<ResultRecord> results;
        for (auto& benchmark : cases) {
            if (!selected(benchmark.contract.name, options.filter)) continue;
            auto result = run_benchmark(benchmark);
            std::cout << result.metadata.benchmark_name
                      << " median=" << result.timing.median_ns_per_op
                      << "ns/op p95=" << result.timing.p95_ns_per_op
                      << "ns/op alloc=" << result.allocations_per_op << "/op\n";
            results.push_back(std::move(result));
        }

        if (selected("idle_invalidation", options.filter)) {
            auto idle = run_idle_invalidation();
            std::cout << "idle_invalidation invalidations=0 alloc="
                      << idle.allocations_per_op << "/checkpoint\n";
            results.push_back(std::move(idle));
        }
        if (results.empty()) {
            throw std::runtime_error("T051 --filter selected no workloads");
        }
        if (!options.json_path.empty()) write_json_document(options.json_path, results);
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "T051 benchmark failure: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
