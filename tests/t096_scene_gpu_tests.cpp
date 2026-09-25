#include "src/detail/platform_test_access.hpp"

#include <nativeui/nativeui.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using Pixel = ui::detail::PlatformReadbackPixel;
using Region = ui::detail::PlatformReadbackRegion;
using Diagnostics = ui::detail::SceneDiagnostics;
using ui::detail::PlatformTestAccess;
using ui::detail::SceneFaultStage;

constexpr int kMaxPolls = 64;

int fail(const char* message) {
    std::cerr << "T096 scene GPU: " << message << '\n';
    return 1;
}

bool failure(const char* message) {
    std::cerr << "T096 scene GPU: " << message << '\n';
    return false;
}

bool same_pixel(const Pixel& a, const Pixel& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

bool same_region(const Region& a, const Region& b) {
    if (a.x != b.x || a.y != b.y || a.width != b.width || a.height != b.height ||
        a.pixels.size() != b.pixels.size()) {
        return false;
    }
    for (std::size_t index = 0; index < a.pixels.size(); ++index) {
        if (!same_pixel(a.pixels[index], b.pixels[index])) return false;
    }
    return true;
}

bool opaque_black(const Pixel& pixel) {
    return pixel.r == 0 && pixel.g == 0 && pixel.b == 0 && pixel.a >= 250;
}

std::optional<Pixel> read_pixel(ui::Application& application,
                                ui::StandaloneWindow& window,
                                ui::Point point) {
    if (!PlatformTestAccess::request_gpu_readback(window, point)) {
        return std::nullopt;
    }
    for (int attempt = 0; attempt < kMaxPolls; ++attempt) {
        (void)application.poll(0.0);
        if (auto pixel = PlatformTestAccess::take_gpu_readback(window)) {
            return pixel;
        }
    }
    return std::nullopt;
}

std::optional<Region> read_region(ui::Application& application,
                                  ui::StandaloneWindow& window,
                                  int x,
                                  int y,
                                  int width,
                                  int height) {
    if (!PlatformTestAccess::request_gpu_readback_region(
            window, x, y, width, height)) {
        return std::nullopt;
    }
    for (int attempt = 0; attempt < kMaxPolls; ++attempt) {
        (void)application.poll(0.0);
        if (auto region = PlatformTestAccess::take_gpu_readback_region(window)) {
            return region;
        }
    }
    return std::nullopt;
}

std::optional<Region> read_surface(ui::Application& application,
                                   ui::StandaloneWindow& window) {
    const auto diagnostics = PlatformTestAccess::scene_diagnostics(window);
    if (diagnostics.scene_width <= 0 || diagnostics.scene_height <= 0) {
        return std::nullopt;
    }
    return read_region(application, window, 0, 0,
                       diagnostics.scene_width, diagnostics.scene_height);
}

const Pixel& surface_pixel(const Region& region, int x, int y) {
    return region.pixels[static_cast<std::size_t>(y) *
                             static_cast<std::size_t>(region.width) +
                         static_cast<std::size_t>(x)];
}

// Production derives the physical extent with an outward (ceil) conversion
// (`logical_to_physical_size`). Mirror that arithmetic for test-side extents
// and sample-point translation so fractional scales (for example 1.25/1.75 on
// high-DPI Windows) never under-cover the logical geometry. This changes test
// arithmetic only; production behavior is unchanged.
int covering_physical_pixel(float logical, float scale) {
    return static_cast<int>(std::ceil(logical * scale));
}

template <class Predicate>
bool wait_for(ui::Application& application,
              ui::StandaloneWindow& window,
              Predicate&& predicate) {
    for (int attempt = 0; attempt < kMaxPolls; ++attempt) {
        if (predicate(PlatformTestAccess::scene_diagnostics(window))) return true;
        (void)application.poll(0.0);
    }
    return predicate(PlatformTestAccess::scene_diagnostics(window));
}

bool wait_for_failed(ui::Application& application,
                     ui::StandaloneWindow& window,
                     std::uint64_t previous_failures) {
    return wait_for(application, window, [previous_failures](const Diagnostics& d) {
        return d.failed_exposes > previous_failures;
    });
}

bool wait_for_builds(ui::Application& application,
                     ui::StandaloneWindow& window,
                     std::uint64_t target) {
    return wait_for(application, window, [target](const Diagnostics& d) {
        return d.scene_builds >= target;
    });
}

bool wait_for_partial(ui::Application& application,
                      ui::StandaloneWindow& window,
                      std::uint64_t target) {
    return wait_for(application, window, [target](const Diagnostics& d) {
        return d.partial_scene_updates >= target;
    });
}

bool wait_for_presentations(ui::Application& application,
                            ui::StandaloneWindow& window,
                            std::uint64_t target) {
    return wait_for(application, window, [target](const Diagnostics& d) {
        return d.presentations >= target;
    });
}

bool settle(ui::Application& application, ui::StandaloneWindow& window) {
    for (int attempt = 0; attempt < 32; ++attempt) {
        (void)application.poll(0.0);
    }
    const auto diagnostics = PlatformTestAccess::scene_diagnostics(window);
    return diagnostics.scene_valid && !diagnostics.full_repaint_required;
}

// Retained UI activation is explicit for every fixture in this file: headless
// X11/xvfb runs never deliver PUGL_FOCUS_IN, and an inactive tree conservatively
// requires a full repaint (focus/availability structure stay dirty), which would
// hide the localized path under test. `activate` is idempotent when the platform
// already delivered focus, and the activation frame raises a full-viewport
// invalidation, so drain it before baselining partial diagnostics.
bool activate_and_settle(ui::UI& retained,
                         ui::Application& application,
                         ui::StandaloneWindow& window) {
    retained.activate(window);
    return settle(application, window);
}

// A configurable retained paint probe. `measure` reports a fixed size, the
// optional draw callback owns pixel output and `paint`/`layout_children` can
// raise deterministic faults.
struct ProbeState {
    int paints{};
    bool throw_on_paint{};
    bool throw_on_layout{};
    ui::Size size{24.0f, 32.0f};
    ui::ComponentAvailability availability{};
    std::function<void()> invalidate_availability;
    std::function<void(ui::PaintContext&)> draw;
};

class ProbeComponent final : public ui::Component {
public:
    explicit ProbeComponent(std::shared_ptr<ProbeState> state)
        : state_(std::move(state)) {}

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return state_->size;
    }

    [[nodiscard]] ui::ComponentAvailability local_availability() const noexcept override {
        return state_->availability;
    }

    void mount(ui::MountContext& context) override {
        state_->invalidate_availability = context.availability_invalidator();
    }

    void layout_children(ui::Rect,
                         const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>&) const override {
        if (state_->throw_on_layout) {
            throw std::runtime_error("injected T096 layout fault");
        }
    }

    void paint(ui::PaintContext& context) const override {
        ++state_->paints;
        if (state_->draw) state_->draw(context);
        if (state_->throw_on_paint) {
            throw std::runtime_error("injected T096 retained paint fault");
        }
    }

private:
    std::shared_ptr<ProbeState> state_;
};

ui::Spec probe_spec(std::shared_ptr<ProbeState> state) {
    return ui::Spec{
        [state = std::move(state)] {
            return std::make_unique<ProbeComponent>(state);
        },
        {}};
}

class Probe {
public:
    explicit Probe(std::shared_ptr<ProbeState> state) : state_(std::move(state)) {}

    ui::Spec spec() && { return probe_spec(std::move(state_)); }

private:
    std::shared_ptr<ProbeState> state_;
};

struct BenchmarkCounters {
    long long full_area{};
    long long partial_area{};
    int full_paints{};
    int partial_paints{};
    int present_only_paints{};
    std::uint64_t present_only_builds{};
    std::uint64_t scene_allocations{};
    bool storage_reused{};
    bool full_is_surface{};
    bool partial_is_strict_subset{};
};

// Two separated ordinary components. Covers the localized partial commit, the
// independent full-before/full-after changed-pixel oracle, the partial-path
// fault/preparation matrix and deterministic warm-frame counters.
bool run_localized_partial_fixture(ui::Application& application,
                                  BenchmarkCounters& counters) {
    struct FixtureState {
        bool left_changed{};
        bool invalidate_during_paint{};
        bool throw_paint{};
        bool nested_entry{};
        bool nested_entry_painted{};
        ui::UI* ui{};
        ui::PlatformServices* platform{};
        SkCanvas* scratch{};
    } fixture;

    auto left = std::make_shared<ProbeState>();
    left->size = {24.0f, 32.0f};
    auto right = std::make_shared<ProbeState>();
    right->size = {24.0f, 32.0f};

    left->draw = [&fixture, left](ui::PaintContext& context) {
        auto& painter = context.painter();
        const ui::Rect bounds = context.bounds();
        if (fixture.invalidate_during_paint) {
            fixture.invalidate_during_paint = false;
            fixture.ui->invalidate({6.0f, 6.0f, 8.0f, 8.0f});
        }
        if (fixture.nested_entry) {
            fixture.nested_entry = false;
            // Structural/layout work and a nested paint request raised from an
            // active region pass. The nested entry must be rejected and the
            // new work must survive the active commit.
            fixture.ui->invalidate_layout();
            fixture.ui->invalidate({6.0f, 6.0f, 8.0f, 8.0f});
            const int before = left->paints;
            fixture.ui->paint(*fixture.scratch, *fixture.platform);
            if (left->paints != before) fixture.nested_entry_painted = true;
        }
        painter.fill_rounded_rect(bounds, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
        if (fixture.left_changed) {
            painter.fill_rounded_rect(
                {bounds.x + 6.0f, bounds.y + 6.0f, 8.0f, 8.0f},
                0.0f,
                {0.0f, 0.0f, 0.0f, 1.0f});
        }
        if (fixture.throw_paint) {
            fixture.throw_paint = false;
            throw std::runtime_error("injected T096 retained paint fault");
        }
    };
    right->draw = [](ui::PaintContext& context) {
        context.painter().fill_rounded_rect(
            context.bounds(), 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
    };

    ui::UI retained{ui::Row{Probe{left}, Probe{right}}.gap(0.0f)};
    fixture.ui = &retained;
    ui::StandaloneWindow window{
        application,
        retained,
        ui::WindowDesc{.title = "T096 partial scene",
                       .size = {48.0f, 32.0f},
                       .resizable = true}};
    if (!window.valid()) return failure("window creation failed");
    fixture.platform = &window;
    SkCanvas scratch;
    fixture.scratch = &scratch;

    if (!activate_and_settle(retained, application, window)) {
        return failure("partial fixture did not settle after activation");
    }

    const auto left_before = read_pixel(application, window, {8.0f, 12.0f});
    const auto right_before = read_pixel(application, window, {34.0f, 12.0f});
    if (!left_before || left_before->r < 240 || left_before->g > 10 ||
        !right_before || right_before->g < 240 || right_before->r > 10) {
        return failure("initial retained scene mismatch");
    }

    constexpr std::array<ui::Point, 20> sample_points{{
        {2.0f, 2.0f}, {8.0f, 2.0f}, {18.0f, 2.0f}, {30.0f, 2.0f},
        {44.0f, 2.0f}, {2.0f, 8.0f}, {8.0f, 8.0f}, {18.0f, 8.0f},
        {30.0f, 8.0f}, {44.0f, 8.0f}, {2.0f, 16.0f}, {8.0f, 16.0f},
        {18.0f, 16.0f}, {30.0f, 16.0f}, {44.0f, 16.0f}, {2.0f, 28.0f},
        {8.0f, 28.0f}, {18.0f, 28.0f}, {30.0f, 28.0f}, {44.0f, 28.0f},
    }};
    const auto capture_grid = [&]() -> std::optional<std::vector<Pixel>> {
        std::vector<Pixel> pixels;
        pixels.reserve(sample_points.size());
        for (const auto point : sample_points) {
            const auto pixel = read_pixel(application, window, point);
            if (!pixel) return std::nullopt;
            pixels.push_back(*pixel);
        }
        return pixels;
    };
    const auto initial_grid = capture_grid();
    if (!initial_grid) return failure("initial reference grid readback failed");
    const auto initial_surface = read_surface(application, window);
    if (!initial_surface) return failure("initial full-surface readback failed");

    // Localized partial commit.
    const int previous_left_paints = left->paints;
    const int previous_right_paints = right->paints;
    const auto previous_scene = PlatformTestAccess::scene_diagnostics(window);
    fixture.left_changed = true;
    retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});

    const auto changed = read_pixel(application, window, {8.0f, 12.0f});
    const auto preserved = read_pixel(application, window, {18.0f, 12.0f});
    const auto untouched = read_pixel(application, window, {34.0f, 12.0f});
    const auto updated_scene = PlatformTestAccess::scene_diagnostics(window);
    if (!changed || changed->r > 10 || changed->g > 10 || changed->b > 10 ||
        !preserved || preserved->r < 240 || preserved->g > 10 ||
        !untouched || untouched->g < 240 || untouched->r > 10) {
        return failure("partial update changed pixels outside its damage region");
    }
    if (left->paints <= previous_left_paints ||
        right->paints != previous_right_paints) {
        return failure("partial traversal repainted an unchanged sibling");
    }
    if (updated_scene.scene_allocations != previous_scene.scene_allocations ||
        updated_scene.scene_builds != previous_scene.scene_builds + 1 ||
        updated_scene.partial_scene_updates !=
            previous_scene.partial_scene_updates + 1 ||
        !updated_scene.scene_valid || updated_scene.full_repaint_required) {
        return failure("partial scene update did not commit into persistent storage");
    }
    // The localized update must be a strict subset of the device surface: this
    // proves a partial update rather than an accidental full-surface repaint.
    if (updated_scene.scene_width <= 0 || updated_scene.scene_height <= 0 ||
        updated_scene.last_update_width <= 0 ||
        updated_scene.last_update_height <= 0 ||
        (updated_scene.last_update_width == updated_scene.scene_width &&
         updated_scene.last_update_height == updated_scene.scene_height)) {
        return failure("localized update was not a strict subset of the device surface");
    }

    const auto partial_grid = capture_grid();
    if (!partial_grid) return failure("partial scene grid readback failed");
    const float scale = window.scale_factor();
    for (std::size_t index = 0; index < sample_points.size(); ++index) {
        const int x = covering_physical_pixel(sample_points[index].x, scale);
        const int y = covering_physical_pixel(sample_points[index].y, scale);
        const bool outside_update =
            x < updated_scene.last_update_x ||
            x >= updated_scene.last_update_x + updated_scene.last_update_width ||
            y < updated_scene.last_update_y ||
            y >= updated_scene.last_update_y + updated_scene.last_update_height;
        const auto& before = (*initial_grid)[index];
        const auto& after = (*partial_grid)[index];
        if (outside_update && (before.r != after.r || before.g != after.g ||
                               before.b != after.b || before.a != after.a)) {
            return failure("persistent pixels outside the actual device update changed");
        }
    }

    // Whole-scene changed-pixel-mask oracle. Full-before/full-after references
    // come from independent full renders, never from the partial diagnostics.
    retained.invalidate();
    if (!wait_for_builds(application, window, previous_scene.scene_builds + 2)) {
        return failure("independent full-after render did not complete");
    }
    const auto full_after_scene = PlatformTestAccess::scene_diagnostics(window);
    if (full_after_scene.last_update_width != full_after_scene.scene_width ||
        full_after_scene.last_update_height != full_after_scene.scene_height) {
        return failure("full-after reference was not a full-surface update");
    }
    const auto full_after_surface = read_surface(application, window);
    if (!full_after_surface) return failure("full-after surface readback failed");

    fixture.left_changed = false;
    retained.invalidate();
    if (!wait_for_builds(application, window, previous_scene.scene_builds + 3)) {
        return failure("independent full-before render did not complete");
    }
    const auto full_before_surface = read_surface(application, window);
    if (!full_before_surface) return failure("full-before surface readback failed");
    if (!same_region(*full_before_surface, *initial_surface)) {
        return failure("full-before reference did not restore the initial scene");
    }

    fixture.left_changed = true;
    retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
    if (!wait_for_partial(
            application, window, previous_scene.partial_scene_updates + 2)) {
        return failure("oracle partial update did not complete");
    }
    const auto oracle_scene = PlatformTestAccess::scene_diagnostics(window);
    if (oracle_scene.partial_scene_updates !=
            previous_scene.partial_scene_updates + 2 ||
        oracle_scene.scene_builds != previous_scene.scene_builds + 4 ||
        oracle_scene.scene_width <= 0 || oracle_scene.scene_height <= 0 ||
        (oracle_scene.last_update_width == oracle_scene.scene_width &&
         oracle_scene.last_update_height == oracle_scene.scene_height)) {
        return failure("oracle partial update did not stay localized");
    }
    const auto oracle_surface = read_surface(application, window);
    if (!oracle_surface) return failure("oracle surface readback failed");
    if (oracle_surface->width != initial_surface->width ||
        oracle_surface->height != initial_surface->height ||
        full_after_surface->width != initial_surface->width ||
        full_after_surface->height != initial_surface->height) {
        return failure("oracle readbacks disagreed on the device surface extent");
    }

    const int union_left = oracle_scene.last_update_x;
    const int union_top = oracle_scene.last_update_y;
    const int union_right = union_left + oracle_scene.last_update_width;
    const int union_bottom = union_top + oracle_scene.last_update_height;
    if (union_left < 0 || union_top < 0 ||
        union_right > oracle_scene.scene_width ||
        union_bottom > oracle_scene.scene_height) {
        return failure("implementation update union escaped the device surface");
    }
    long long changed_pixels = 0;
    for (int y = 0; y < initial_surface->height; ++y) {
        for (int x = 0; x < initial_surface->width; ++x) {
            const auto& before = surface_pixel(*initial_surface, x, y);
            const auto& after_full = surface_pixel(*full_after_surface, x, y);
            const auto& after_partial = surface_pixel(*oracle_surface, x, y);
            const bool inside = x >= union_left && x < union_right &&
                                y >= union_top && y < union_bottom;
            const bool changed_pixel = !same_pixel(before, after_full);
            if (changed_pixel) {
                ++changed_pixels;
                if (!inside) {
                    return failure("changed pixel lies outside the reported update union");
                }
            }
            if (inside) {
                // Coverage proof: every pixel the implementation claims to have
                // updated must equal the independent full render.
                if (!same_pixel(after_partial, after_full)) {
                    return failure("update-union pixel differs from the independent full render");
                }
            } else if (!same_pixel(after_partial, before)) {
                return failure("persistent pixel outside the update union changed");
            }
        }
    }
    if (changed_pixels == 0) {
        return failure("changed-pixel oracle saw no difference between full renders");
    }
    if (!same_region(*oracle_surface, *full_after_surface)) {
        return failure("partial scene differs from the independent full-render reference");
    }

    // Component paint exception during a localized partial pass: T130
    // containment, transaction rollback and next-frame full recovery. The
    // momentary post-abort state is deliberately not asserted: a racing
    // compositor-driven expose may already have completed the full fallback
    // before diagnostics are read, so only cumulative no-partial-publication
    // invariants and the eventual full-surface recovery are required.
    const auto before_exception = PlatformTestAccess::scene_diagnostics(window);
    fixture.throw_paint = true;
    retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
    if (!wait_for_failed(application, window, before_exception.failed_exposes)) {
        return failure("partial paint exception did not reach the native expose");
    }
    const auto exception_failed = PlatformTestAccess::scene_diagnostics(window);
    if (exception_failed.partial_scene_updates !=
        before_exception.partial_scene_updates) {
        return failure("failed partial pass published a partial scene update");
    }
    if (!window.valid() || window.should_close()) {
        return failure("retained paint exception crossed containment");
    }
    const auto exception_recovery_pixel =
        read_pixel(application, window, {16.0f, 12.0f});
    if (!exception_recovery_pixel || exception_recovery_pixel->r < 240 ||
        exception_recovery_pixel->g > 10) {
        return failure("partial paint exception did not recover on a later expose");
    }
    const auto exception_recovered = PlatformTestAccess::scene_diagnostics(window);
    if (!exception_recovered.scene_valid ||
        exception_recovered.full_repaint_required ||
        exception_recovered.scene_builds <= before_exception.scene_builds ||
        exception_recovered.partial_scene_updates !=
            before_exception.partial_scene_updates ||
        exception_recovered.last_update_width != exception_recovered.scene_width ||
        exception_recovered.last_update_height != exception_recovered.scene_height ||
        !window.last_error().empty()) {
        return failure("partial paint exception recovery did not commit a full scene");
    }

    // Preparation/layout throw after earlier layout progress: no partial
    // layout or partial scene update may be published and the next committed
    // frame must be a full-surface recovery. As above, the momentary post-fault
    // state is not asserted because a racing expose may have recovered already.
    const auto before_layout_fault = PlatformTestAccess::scene_diagnostics(window);
    right->throw_on_layout = true;
    retained.invalidate_layout();
    if (!wait_for_failed(application, window, before_layout_fault.failed_exposes)) {
        return failure("layout preparation fault did not reach the native expose");
    }
    const auto layout_failed = PlatformTestAccess::scene_diagnostics(window);
    if (layout_failed.partial_scene_updates !=
        before_layout_fault.partial_scene_updates) {
        return failure("layout preparation fault published a partial scene update");
    }
    if (!window.valid() || window.should_close()) {
        return failure("layout preparation fault closed the native view");
    }
    right->throw_on_layout = false;
    const auto layout_recovery_pixel =
        read_pixel(application, window, {16.0f, 12.0f});
    if (!layout_recovery_pixel || layout_recovery_pixel->r < 240 ||
        layout_recovery_pixel->g > 10) {
        return failure("layout preparation fault did not recover on the next frame");
    }
    const auto layout_recovered = PlatformTestAccess::scene_diagnostics(window);
    if (!layout_recovered.scene_valid ||
        layout_recovered.full_repaint_required ||
        layout_recovered.scene_builds <= before_layout_fault.scene_builds ||
        layout_recovered.partial_scene_updates !=
            before_layout_fault.partial_scene_updates ||
        layout_recovered.last_update_width != layout_recovered.scene_width ||
        layout_recovered.last_update_height != layout_recovered.scene_height ||
        !window.last_error().empty()) {
        return failure("layout preparation fault recovery did not commit a full scene");
    }

    // Structural/layout request plus attempted nested paint during an active
    // region pass: active targets stay stable, nested entry is rejected and the
    // newer layout work survives into a full follow-up frame.
    const auto before_nested = PlatformTestAccess::scene_diagnostics(window);
    fixture.nested_entry = true;
    retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
    if (!wait_for_builds(application, window, before_nested.scene_builds + 2)) {
        return failure("nested-entry follow-up frame did not complete");
    }
    const auto nested_scene = PlatformTestAccess::scene_diagnostics(window);
    if (fixture.nested_entry || fixture.nested_entry_painted) {
        return failure("nested paint re-entered the retained tree");
    }
    if (nested_scene.partial_scene_updates !=
            before_nested.partial_scene_updates + 1) {
        return failure("active region pass did not commit exactly one partial update");
    }
    if (!nested_scene.scene_valid || nested_scene.full_repaint_required ||
        nested_scene.last_update_width != nested_scene.scene_width ||
        nested_scene.last_update_height != nested_scene.scene_height) {
        return failure("layout work raised during the active pass did not force a full frame");
    }

    // Successful partial commit followed by a non-context presentation failure:
    // the committed scene stays valid and the next successful expose presents it
    // without repeating retained paint work. The momentary present_pending state
    // is not asserted because a racing expose may already have presented the
    // committed scene; committed counters and paint counts are cumulative and
    // therefore race-independent (no rebuild can follow a valid committed scene
    // with no pending retained work).
    const auto before_present_fault = PlatformTestAccess::scene_diagnostics(window);
    const int before_present_left = left->paints;
    const int before_present_right = right->paints;
    if (!PlatformTestAccess::inject_scene_fault(
            window, SceneFaultStage::PresentationCopy)) {
        return failure("presentation fault injection was rejected");
    }
    retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
    if (!wait_for_failed(application, window, before_present_fault.failed_exposes)) {
        return failure("presentation fault after partial commit did not execute");
    }
    const auto present_failed = PlatformTestAccess::scene_diagnostics(window);
    if (!present_failed.scene_valid || present_failed.full_repaint_required ||
        present_failed.partial_scene_updates !=
            before_present_fault.partial_scene_updates + 1 ||
        present_failed.scene_builds != before_present_fault.scene_builds + 1) {
        return failure("presentation failure lost the committed partial scene");
    }
    if (left->paints != before_present_left + 1 ||
        right->paints != before_present_right) {
        return failure("presentation failure partial pass was not localized");
    }
    if (!PlatformTestAccess::request_expose(window) ||
        !wait_for_presentations(application, window,
                                before_present_fault.presentations + 1)) {
        return failure("presentation retry did not run");
    }
    const auto present_retried = PlatformTestAccess::scene_diagnostics(window);
    if (present_retried.scene_builds != before_present_fault.scene_builds + 1 ||
        present_retried.partial_scene_updates !=
            before_present_fault.partial_scene_updates + 1 ||
        left->paints != before_present_left + 1 ||
        right->paints != before_present_right) {
        return failure("presentation retry repeated retained paint work");
    }
    if (!present_retried.scene_valid || present_retried.present_pending ||
        present_retried.full_repaint_required) {
        return failure("presentation retry did not settle the committed scene");
    }

    // Reentrant invalidation during paint: the active snapshot commits and the
    // newer generation survives completion of the active frame.
    const auto before_reentrant = PlatformTestAccess::scene_diagnostics(window);
    fixture.invalidate_during_paint = true;
    retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
    if (!wait_for_builds(application, window, before_reentrant.scene_builds + 2)) {
        return failure("overlapping invalidation raised during paint was lost");
    }
    const auto after_reentrant = PlatformTestAccess::scene_diagnostics(window);
    if (!after_reentrant.scene_valid || after_reentrant.full_repaint_required) {
        return failure("reentrant invalidation left the scene invalid");
    }

    // Production fault stages before/after retained paint and at checked scene
    // submission: no incomplete scene may be accepted. Each stage must leave
    // the partial-publication counter untouched and recover with a full-surface
    // commit; the momentary post-fault state is not asserted because a racing
    // expose may already have recovered before diagnostics are read.
    for (const auto stage : {SceneFaultStage::AfterSceneReset,
                             SceneFaultStage::AfterRetainedPaint,
                             SceneFaultStage::SceneSubmission}) {
        const auto before_failure = PlatformTestAccess::scene_diagnostics(window);
        if (!PlatformTestAccess::inject_scene_fault(window, stage)) {
            return failure("partial scene fault injection was rejected");
        }
        retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
        if (!wait_for_failed(application, window, before_failure.failed_exposes)) {
            return failure("failed partial update was not reported at the native expose");
        }
        const auto failed = PlatformTestAccess::scene_diagnostics(window);
        if (failed.partial_scene_updates != before_failure.partial_scene_updates) {
            return failure("failed partial update was accepted as a complete scene");
        }
        if (!read_pixel(application, window, {8.0f, 12.0f})) {
            return failure("failed partial update did not recover on a later expose");
        }
        const auto recovered = PlatformTestAccess::scene_diagnostics(window);
        if (!recovered.scene_valid || recovered.full_repaint_required ||
            recovered.scene_builds <= before_failure.scene_builds ||
            recovered.partial_scene_updates != before_failure.partial_scene_updates ||
            recovered.last_update_width != recovered.scene_width ||
            recovered.last_update_height != recovered.scene_height) {
            return failure("partial failure recovery did not commit a full scene");
        }
    }

    retained.invalidate();
    const auto full_reference = capture_grid();
    if (!full_reference || full_reference->size() != partial_grid->size()) {
        return failure("independent full-render reference failed");
    }
    for (std::size_t index = 0; index < partial_grid->size(); ++index) {
        const auto& partial = (*partial_grid)[index];
        const auto& full = (*full_reference)[index];
        if (partial.r != full.r || partial.g != full.g ||
            partial.b != full.b || partial.a != full.a) {
            return failure("partial scene differs from independent full-render reference");
        }
    }

    // Deterministic warm-frame counters. No hardware FPS claims and no
    // production readback/GPU wait: the readbacks above exist only behind
    // NATIVEUI_ENABLE_PLATFORM_TEST_SEAMS.
    if (!settle(application, window)) return failure("benchmark fixture did not settle");
    const auto benchmark_before = PlatformTestAccess::scene_diagnostics(window);

    const int full_left = left->paints;
    const int full_right = right->paints;
    retained.invalidate();
    if (!wait_for_builds(application, window, benchmark_before.scene_builds + 1)) {
        return failure("benchmark full frame did not complete");
    }
    const auto benchmark_full = PlatformTestAccess::scene_diagnostics(window);
    counters.full_area = static_cast<long long>(benchmark_full.last_update_width) *
                         static_cast<long long>(benchmark_full.last_update_height);
    counters.full_paints = (left->paints - full_left) + (right->paints - full_right);
    counters.full_is_surface =
        benchmark_full.last_update_width == benchmark_full.scene_width &&
        benchmark_full.last_update_height == benchmark_full.scene_height;

    fixture.left_changed = !fixture.left_changed;
    const int partial_left = left->paints;
    const int partial_right = right->paints;
    retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
    if (!wait_for_partial(application, window,
                          benchmark_full.partial_scene_updates + 1)) {
        return failure("benchmark partial frame did not complete");
    }
    const auto benchmark_partial = PlatformTestAccess::scene_diagnostics(window);
    counters.partial_area =
        static_cast<long long>(benchmark_partial.last_update_width) *
        static_cast<long long>(benchmark_partial.last_update_height);
    counters.partial_paints =
        (left->paints - partial_left) + (right->paints - partial_right);
    counters.partial_is_strict_subset =
        counters.partial_area < counters.full_area &&
        !(benchmark_partial.last_update_width == benchmark_partial.scene_width &&
          benchmark_partial.last_update_height == benchmark_partial.scene_height);

    const int present_left = left->paints;
    const int present_right = right->paints;
    if (!PlatformTestAccess::request_expose(window) ||
        !wait_for_presentations(application, window,
                                benchmark_partial.presentations + 1)) {
        return failure("benchmark present-only frame did not complete");
    }
    const auto benchmark_present = PlatformTestAccess::scene_diagnostics(window);
    counters.present_only_paints =
        (left->paints - present_left) + (right->paints - present_right);
    counters.present_only_builds =
        benchmark_present.scene_builds - benchmark_partial.scene_builds;
    counters.scene_allocations = benchmark_present.scene_allocations;
    counters.storage_reused =
        benchmark_present.scene_allocations == benchmark_before.scene_allocations;

    if (counters.full_paints != 2 || counters.partial_paints != 1 ||
        counters.present_only_paints != 0 ||
        counters.present_only_builds != 0 || !counters.full_is_surface ||
        !counters.partial_is_strict_subset || !counters.storage_reused) {
        return failure("warm-frame counters did not match the partial-repaint contract");
    }
    return true;
}

// A sibling GaussianBlur becomes active during a localized partial frame. The
// frame must abort the partial commit, force full repaint and match an
// independent full render at the discrete filter fringe. This records the
// permitted full fallback for filtered effects.
bool run_filtered_effect_fixture(ui::Application& application) {
    struct EffectState {
        int paints{};
        bool filter_on{};
        bool changed{};
    };

    const auto configure = [](const std::shared_ptr<EffectState>& state) {
        auto probe = std::make_shared<ProbeState>();
        probe->size = {24.0f, 32.0f};
        probe->draw = [state](ui::PaintContext& context) {
            auto& painter = context.painter();
            const ui::Rect bounds = context.bounds();
            const ui::Rect source{
                bounds.x + 6.0f, bounds.y + 10.0f, 8.0f, 12.0f};
            if (state->filter_on) {
                auto layer = painter.scoped_layer(
                    bounds, ui::Effect::gaussian_blur(2.5f, 2.5f));
                painter.fill_rounded_rect(source, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            } else {
                painter.fill_rounded_rect(source, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            }
            if (state->changed) {
                painter.fill_rounded_rect(
                    {source.x, source.y, 4.0f, 4.0f},
                    0.0f,
                    {0.0f, 0.0f, 0.0f, 1.0f});
            }
        };
        return probe;
    };

    auto fixture_state = std::make_shared<EffectState>();
    auto reference_state = std::make_shared<EffectState>();
    auto fixture_probe = configure(fixture_state);
    auto fixture_right = std::make_shared<ProbeState>();
    fixture_right->size = {24.0f, 32.0f};
    fixture_right->draw = [](ui::PaintContext& context) {
        context.painter().fill_rounded_rect(
            context.bounds(), 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
    };

    ui::UI retained{
        ui::Row{Probe{fixture_probe}, Probe{fixture_right}}.gap(0.0f)};
    ui::StandaloneWindow window{
        application,
        retained,
        ui::WindowDesc{.title = "T096 filtered scene",
                       .size = {48.0f, 32.0f},
                       .resizable = true}};
    if (!window.valid()) return failure("filtered window creation failed");
    if (!activate_and_settle(retained, application, window)) {
        return failure("filtered fixture did not settle after activation");
    }

    const auto plain_surface = read_surface(application, window);
    if (!plain_surface) return failure("filtered plain surface readback failed");
    const auto plain_fringe = read_pixel(application, window, {3.0f, 16.0f});
    if (!plain_fringe || plain_fringe->r > 10) {
        return failure("plain filter fringe was not empty before the effect");
    }

    const auto before_effect = PlatformTestAccess::scene_diagnostics(window);
    fixture_state->filter_on = true;
    fixture_state->changed = true;
    retained.invalidate({6.0f, 10.0f, 4.0f, 4.0f});
    if (!wait_for_failed(application, window, before_effect.failed_exposes)) {
        return failure("filtered partial frame did not abort at the native expose");
    }
    // The aborted pass must never publish a partial scene update. This is a
    // cumulative invariant, so it holds even when a racing compositor-driven
    // expose has already completed the full fallback frame.
    const auto aborted = PlatformTestAccess::scene_diagnostics(window);
    if (aborted.partial_scene_updates != before_effect.partial_scene_updates) {
        return failure("filtered partial frame incremented a partial commit");
    }

    // The next committed scene build must be the full-surface fallback: the
    // filtered partial frame cannot commit, so recovery is exactly a full
    // update. A racing expose may already have produced it.
    if (!PlatformTestAccess::request_expose(window)) {
        return failure("filtered fallback expose request was refused");
    }
    if (!wait_for(application, window, [&before_effect](const Diagnostics& d) {
            return d.scene_valid && !d.full_repaint_required &&
                   d.scene_builds > before_effect.scene_builds;
        })) {
        return failure("filtered full fallback frame did not complete");
    }
    const auto fallback = PlatformTestAccess::scene_diagnostics(window);
    if (!fallback.scene_valid || fallback.full_repaint_required ||
        fallback.last_update_width != fallback.scene_width ||
        fallback.last_update_height != fallback.scene_height ||
        fallback.partial_scene_updates != before_effect.partial_scene_updates) {
        return failure("filtered fallback frame was not a full scene update");
    }
    const auto fallback_surface = read_surface(application, window);
    if (!fallback_surface) return failure("filtered fallback surface readback failed");

    auto reference_probe = configure(reference_state);
    auto reference_right = std::make_shared<ProbeState>();
    reference_right->size = {24.0f, 32.0f};
    reference_right->draw = [](ui::PaintContext& context) {
        context.painter().fill_rounded_rect(
            context.bounds(), 0.0f, {0.0f, 1.0f, 0.0f, 1.0f});
    };
    reference_state->filter_on = true;
    reference_state->changed = true;
    ui::UI reference{
        ui::Row{Probe{reference_probe}, Probe{reference_right}}.gap(0.0f)};
    ui::StandaloneWindow reference_window{
        application,
        reference,
        ui::WindowDesc{.title = "T096 filtered reference",
                       .size = {48.0f, 32.0f},
                       .resizable = true}};
    if (!reference_window.valid()) return failure("filtered reference window creation failed");
    if (!activate_and_settle(reference, application, reference_window)) {
        return failure("filtered reference fixture did not settle after activation");
    }
    const auto reference_surface = read_surface(application, reference_window);
    if (!reference_surface) return failure("filtered reference surface readback failed");
    if (!same_region(*fallback_surface, *reference_surface)) {
        return failure("filtered fallback differs from the independent full render");
    }

    // Source-outside-P fringe: the localized damage was a 4x4 logical rect, yet
    // the discrete blur envelope reaches further out. The fallback frame must
    // carry that contribution exactly like the independent full render.
    const auto fallback_fringe = read_pixel(application, window, {3.0f, 16.0f});
    const auto reference_fringe =
        read_pixel(application, reference_window, {3.0f, 16.0f});
    if (!fallback_fringe || !reference_fringe ||
        fallback_fringe->r != reference_fringe->r ||
        fallback_fringe->g != reference_fringe->g ||
        fallback_fringe->b != reference_fringe->b ||
        fallback_fringe->a != reference_fringe->a ||
        fallback_fringe->r <= plain_fringe->r) {
        return failure("filtered fallback lost the discrete fringe contribution");
    }
    return true;
}

// Canonical clipped reset, stale-pixel erasure, translucent overlap and
// asymmetric orientation/scale fixtures.
bool run_canonical_reset_fixture(ui::Application& application) {
    const auto pixel_at_logical = [](const Region& region,
                                     float scale,
                                     float x,
                                     float y) -> const Pixel& {
        const int device_x = covering_physical_pixel(x, scale);
        const int device_y = covering_physical_pixel(y, scale);
        return surface_pixel(region, device_x, device_y);
    };

    // Removed content on a transparent root: the canonical reset stays opaque
    // black inside the update and the neighbouring sibling stays untouched.
    {
        struct ResetState {
            bool a_visible{true};
        } state;
        auto a = std::make_shared<ProbeState>();
        a->size = {40.0f, 24.0f};
        a->draw = [&state](ui::PaintContext& context) {
            if (!state.a_visible) return;
            const ui::Rect bounds = context.bounds();
            context.painter().fill_rounded_rect(
                {bounds.x + 4.0f, bounds.y + 4.0f, 8.0f, 8.0f},
                0.0f,
                {1.0f, 0.0f, 0.0f, 1.0f});
        };
        auto b = std::make_shared<ProbeState>();
        b->size = {40.0f, 24.0f};
        b->draw = [](ui::PaintContext& context) {
            const ui::Rect bounds = context.bounds();
            context.painter().fill_rounded_rect(
                {bounds.x + 24.0f, bounds.y + 4.0f, 8.0f, 8.0f},
                0.0f,
                {0.0f, 1.0f, 0.0f, 1.0f});
        };

        ui::UI retained{ui::Stack{Probe{a}, Probe{b}}};
        ui::StandaloneWindow window{
            application,
            retained,
            ui::WindowDesc{.title = "T096 canonical reset",
                           .size = {40.0f, 24.0f},
                           .resizable = true}};
        if (!window.valid()) return failure("canonical reset window creation failed");
        if (!activate_and_settle(retained, application, window)) {
            return failure("canonical reset fixture did not settle after activation");
        }

        const auto before_surface = read_surface(application, window);
        if (!before_surface) return failure("canonical reset before readback failed");
        const float scale = window.scale_factor();
        const auto block_a_before = read_pixel(application, window, {8.0f, 8.0f});
        const auto block_b_before = read_pixel(application, window, {28.0f, 8.0f});
        const auto transparent_root = read_pixel(application, window, {18.0f, 14.0f});
        if (!block_a_before || block_a_before->r < 240 ||
            !block_b_before || block_b_before->g < 240 ||
            !transparent_root || !opaque_black(*transparent_root)) {
            return failure("canonical reset initial scene mismatch");
        }

        const auto before_erase = PlatformTestAccess::scene_diagnostics(window);
        state.a_visible = false;
        retained.invalidate({4.0f, 4.0f, 8.0f, 8.0f});
        if (!wait_for_partial(application, window,
                              before_erase.partial_scene_updates + 1)) {
            return failure("canonical erase partial update did not complete");
        }
        const auto erase_scene = PlatformTestAccess::scene_diagnostics(window);
        const auto after_surface = read_surface(application, window);
        if (!after_surface) return failure("canonical reset after readback failed");
        if (erase_scene.last_update_width == erase_scene.scene_width &&
            erase_scene.last_update_height == erase_scene.scene_height) {
            return failure("canonical erase update was not localized");
        }

        const int union_left = erase_scene.last_update_x;
        const int union_top = erase_scene.last_update_y;
        const int union_right = union_left + erase_scene.last_update_width;
        const int union_bottom = union_top + erase_scene.last_update_height;
        const int block_left = static_cast<int>(std::floor(4.0f * scale));
        const int block_top = static_cast<int>(std::floor(4.0f * scale));
        const int block_right = static_cast<int>(std::ceil(12.0f * scale));
        const int block_bottom = static_cast<int>(std::ceil(12.0f * scale));
        if (union_left > block_left || union_top > block_top ||
            union_right < block_right || union_bottom < block_bottom) {
            return failure("canonical erase update did not cover the removed content");
        }
        for (int y = block_top; y < block_bottom; ++y) {
            for (int x = block_left; x < block_right; ++x) {
                if (!opaque_black(surface_pixel(*after_surface, x, y))) {
                    return failure("removed content was not erased to opaque black inside the update");
                }
            }
        }
        for (int y = 0; y < after_surface->height; ++y) {
            for (int x = 0; x < after_surface->width; ++x) {
                const bool inside = x >= union_left && x < union_right &&
                                    y >= union_top && y < union_bottom;
                if (!inside &&
                    !same_pixel(surface_pixel(*after_surface, x, y),
                                surface_pixel(*before_surface, x, y))) {
                    return failure("pixel outside the canonical erase update changed");
                }
            }
        }
        // Pixels immediately outside the actual device clip stay untouched.
        const int mid_x = (union_left + union_right) / 2;
        const int mid_y = (union_top + union_bottom) / 2;
        const std::array<std::pair<int, int>, 4> outside_samples{{
            {union_right, mid_y},
            {union_left - 1, mid_y},
            {mid_x, union_bottom},
            {mid_x, union_top - 1},
        }};
        for (const auto& [x, y] : outside_samples) {
            if (x < 0 || y < 0 || x >= after_surface->width ||
                y >= after_surface->height) {
                continue;
            }
            if (!same_pixel(surface_pixel(*after_surface, x, y),
                            surface_pixel(*before_surface, x, y))) {
                return failure("pixel adjacent to the device clip changed");
            }
        }
        if (!same_pixel(pixel_at_logical(*after_surface, scale, 28.0f, 8.0f),
                        *block_b_before)) {
            return failure("canonical erase damaged a neighbouring sibling");
        }

        retained.invalidate();
        if (!wait_for_builds(application, window, erase_scene.scene_builds + 1)) {
            return failure("canonical erase full reference did not complete");
        }
        const auto full_after = read_surface(application, window);
        if (!full_after || !same_region(*after_surface, *full_after)) {
            return failure("canonical erase differs from the independent full render");
        }

        state.a_visible = true;
        retained.invalidate({4.0f, 4.0f, 8.0f, 8.0f});
        if (!wait_for_partial(application, window,
                              erase_scene.partial_scene_updates + 1)) {
            return failure("canonical reconstruct partial update did not complete");
        }
        const auto reconstructed = read_pixel(application, window, {8.0f, 8.0f});
        if (!reconstructed || reconstructed->r < 240 ||
            reconstructed->g > 10) {
            return failure("canonical update did not reconstruct removed content");
        }

        // Hidden availability is a preparation/availability change and takes
        // the permitted full fallback; stale pixels must still be erased.
        const auto before_hidden = PlatformTestAccess::scene_diagnostics(window);
        a->availability.visibility = ui::VisibilityMode::Hidden;
        if (!a->invalidate_availability) {
            return failure("canonical hidden availability invalidator was not captured");
        }
        a->invalidate_availability();
        if (!wait_for_builds(application, window, before_hidden.scene_builds + 1)) {
            return failure("canonical hidden availability frame did not complete");
        }
        const auto hidden_scene = PlatformTestAccess::scene_diagnostics(window);
        if (!hidden_scene.scene_valid || hidden_scene.full_repaint_required) {
            return failure("canonical hidden availability did not commit a valid scene");
        }
        const auto hidden_pixel = read_pixel(application, window, {8.0f, 8.0f});
        const auto hidden_sibling = read_pixel(application, window, {28.0f, 8.0f});
        if (!hidden_pixel || !opaque_black(*hidden_pixel) ||
            !hidden_sibling || hidden_sibling->g < 240) {
            return failure("hidden content left stale pixels or damaged a sibling");
        }
    }

    // Translucent overlapping siblings: the partial pass must not compound
    // alpha or erase the correct overlap.
    {
        struct OverlapState {
            bool red_visible{true};
        } state;
        auto red = std::make_shared<ProbeState>();
        red->size = {32.0f, 24.0f};
        red->draw = [&state](ui::PaintContext& context) {
            if (!state.red_visible) return;
            const ui::Rect bounds = context.bounds();
            context.painter().fill_rounded_rect(
                {bounds.x + 2.0f, bounds.y + 2.0f, 16.0f, 16.0f},
                0.0f,
                {1.0f, 0.0f, 0.0f, 0.5f});
        };
        auto green = std::make_shared<ProbeState>();
        green->size = {32.0f, 24.0f};
        green->draw = [](ui::PaintContext& context) {
            const ui::Rect bounds = context.bounds();
            context.painter().fill_rounded_rect(
                {bounds.x + 10.0f, bounds.y + 6.0f, 16.0f, 16.0f},
                0.0f,
                {0.0f, 1.0f, 0.0f, 0.5f});
        };

        ui::UI retained{ui::Stack{Probe{red}, Probe{green}}};
        ui::StandaloneWindow window{
            application,
            retained,
            ui::WindowDesc{.title = "T096 translucent overlap",
                           .size = {32.0f, 24.0f},
                           .resizable = true}};
        if (!window.valid()) return failure("translucent window creation failed");
        if (!activate_and_settle(retained, application, window)) {
            return failure("translucent fixture did not settle after activation");
        }

        const auto before_surface = read_surface(application, window);
        if (!before_surface) return failure("translucent before readback failed");
        const auto overlap_before = read_pixel(application, window, {14.0f, 12.0f});
        if (!overlap_before || overlap_before->r < 60 || overlap_before->g < 60 ||
            overlap_before->a < 250) {
            return failure("translucent overlap was not established");
        }

        const auto before_partial = PlatformTestAccess::scene_diagnostics(window);
        state.red_visible = false;
        retained.invalidate({2.0f, 2.0f, 16.0f, 16.0f});
        if (!wait_for_partial(application, window,
                              before_partial.partial_scene_updates + 1)) {
            return failure("translucent partial update did not complete");
        }
        const auto partial_scene = PlatformTestAccess::scene_diagnostics(window);
        const auto after_surface = read_surface(application, window);
        if (!after_surface) return failure("translucent after readback failed");

        retained.invalidate();
        if (!wait_for_builds(application, window, partial_scene.scene_builds + 1)) {
            return failure("translucent full reference did not complete");
        }
        const auto full_after = read_surface(application, window);
        if (!full_after || !same_region(*after_surface, *full_after)) {
            return failure("translucent partial output differs from the full render");
        }
        const auto overlap_after = read_pixel(application, window, {14.0f, 12.0f});
        if (!overlap_after || overlap_after->a < 250 ||
            overlap_after->g < 60) {
            return failure("translucent overlap was erased or alpha-compounded");
        }
        const int union_left = partial_scene.last_update_x;
        const int union_top = partial_scene.last_update_y;
        const int union_right = union_left + partial_scene.last_update_width;
        const int union_bottom = union_top + partial_scene.last_update_height;
        for (int y = 0; y < after_surface->height; ++y) {
            for (int x = 0; x < after_surface->width; ++x) {
                const bool inside = x >= union_left && x < union_right &&
                                    y >= union_top && y < union_bottom;
                if (!inside &&
                    !same_pixel(surface_pixel(*after_surface, x, y),
                                surface_pixel(*before_surface, x, y))) {
                    return failure("pixel outside the translucent update changed");
                }
            }
        }

        // Reconstruct the translucent overlap through a localized partial pass.
        state.red_visible = true;
        retained.invalidate({2.0f, 2.0f, 16.0f, 16.0f});
        if (!wait_for_partial(application, window,
                              partial_scene.partial_scene_updates + 1)) {
            return failure("translucent reconstruct partial update did not complete");
        }
        const auto reconstructed = read_surface(application, window);
        retained.invalidate();
        if (!wait_for_builds(application, window,
                             PlatformTestAccess::scene_diagnostics(window).scene_builds + 1)) {
            return failure("translucent reconstruct full reference did not complete");
        }
        const auto reconstructed_full = read_surface(application, window);
        if (!reconstructed || !reconstructed_full ||
            !same_region(*reconstructed, *reconstructed_full)) {
            return failure("reconstructed translucent overlap differs from the full render");
        }
    }

    // Asymmetric non-square surface: orientation and scale must survive the
    // localized path without flipping or stretching content.
    {
        struct MarkerState {
            bool changed{false};
        } state;
        auto markers = std::make_shared<ProbeState>();
        markers->size = {61.0f, 23.0f};
        markers->draw = [&state](ui::PaintContext& context) {
            auto& painter = context.painter();
            const ui::Rect bounds = context.bounds();
            const auto rect = [&bounds](float x, float y, float w, float h) {
                return ui::Rect{bounds.x + x, bounds.y + y, w, h};
            };
            painter.fill_rounded_rect(rect(0.0f, 0.0f, 5.0f, 5.0f), 0.0f,
                                      {1.0f, 0.0f, 0.0f, 1.0f});
            painter.fill_rounded_rect(rect(56.0f, 0.0f, 5.0f, 5.0f), 0.0f,
                                      {0.0f, 1.0f, 0.0f, 1.0f});
            painter.fill_rounded_rect(rect(0.0f, 18.0f, 5.0f, 5.0f), 0.0f,
                                      {0.0f, 0.0f, 1.0f, 1.0f});
            painter.fill_rounded_rect(rect(56.0f, 18.0f, 5.0f, 5.0f), 0.0f,
                                      {1.0f, 1.0f, 0.0f, 1.0f});
            painter.fill_rounded_rect(
                rect(20.0f, 4.0f, 3.0f, 2.0f),
                0.0f,
                state.changed ? ui::Color{0.0f, 1.0f, 1.0f, 1.0f}
                              : ui::Color{1.0f, 0.0f, 1.0f, 1.0f});
        };

        ui::UI retained{ui::Stack{Probe{markers}}};
        ui::StandaloneWindow window{
            application,
            retained,
            ui::WindowDesc{.title = "T096 asymmetric surface",
                           .size = {61.0f, 23.0f},
                           .resizable = true}};
        if (!window.valid()) return failure("asymmetric window creation failed");
        if (!activate_and_settle(retained, application, window)) {
            return failure("asymmetric fixture did not settle after activation");
        }

        const auto surface = read_surface(application, window);
        if (!surface) return failure("asymmetric surface readback failed");
        const float scale = window.scale_factor();
        const int expected_width = covering_physical_pixel(61.0f, scale);
        const int expected_height = covering_physical_pixel(23.0f, scale);
        if (surface->width != expected_width ||
            surface->height != expected_height ||
            surface->width == surface->height) {
            return failure("asymmetric surface extent or scale mismatch");
        }
        const auto corner_tl = read_pixel(application, window, {2.0f, 2.0f});
        const auto corner_tr = read_pixel(application, window, {58.0f, 2.0f});
        const auto corner_bl = read_pixel(application, window, {2.0f, 20.0f});
        const auto corner_br = read_pixel(application, window, {58.0f, 20.0f});
        const auto marker = read_pixel(application, window, {21.0f, 5.0f});
        if (!corner_tl || corner_tl->r < 240 || corner_tl->b > 10 ||
            !corner_tr || corner_tr->g < 240 || corner_tr->r > 10 ||
            !corner_bl || corner_bl->b < 240 || corner_bl->r > 10 ||
            !corner_br || corner_br->r < 240 || corner_br->g < 240 ||
            corner_br->b > 10 ||
            !marker || marker->r < 240 || marker->b < 240 || marker->g > 10) {
            return failure("asymmetric orientation or scale mismatch");
        }

        // Localized change on the asymmetric surface must still match the full
        // render at the same scale.
        const auto before_partial = PlatformTestAccess::scene_diagnostics(window);
        state.changed = true;
        retained.invalidate({20.0f, 4.0f, 3.0f, 2.0f});
        if (!wait_for_partial(application, window,
                              before_partial.partial_scene_updates + 1)) {
            return failure("asymmetric partial update did not complete");
        }
        const auto partial = read_surface(application, window);
        retained.invalidate();
        if (!wait_for_builds(application, window,
                             PlatformTestAccess::scene_diagnostics(window).scene_builds + 1)) {
            return failure("asymmetric full reference did not complete");
        }
        const auto full = read_surface(application, window);
        if (!partial || !full || !same_region(*partial, *full)) {
            return failure("asymmetric partial output differs from the full render");
        }
        const auto changed_marker = read_pixel(application, window, {21.0f, 5.0f});
        if (!changed_marker || changed_marker->g < 240 || changed_marker->b < 240 ||
            changed_marker->r > 10) {
            return failure("asymmetric localized change was not applied");
        }
    }

    return true;
}

// Two independent views each commit a localized partial update. Destroying
// view A must leave view B's scene, damage and present state untouched; B must
// then still commit its next localized partial update without a full repaint
// and match an independent full render. The fixture stays effect-free so the
// documented partial path (not the filtered fallback) applies.
bool run_sibling_destruction_isolation_fixture(ui::Application& application) {
    struct ViewState {
        bool changed{};
    };

    const auto make_probe = [](const std::shared_ptr<ViewState>& state) {
        auto probe = std::make_shared<ProbeState>();
        probe->size = {48.0f, 32.0f};
        probe->draw = [state](ui::PaintContext& context) {
            auto& painter = context.painter();
            const ui::Rect bounds = context.bounds();
            painter.fill_rounded_rect(bounds, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});
            if (state->changed) {
                painter.fill_rounded_rect(
                    {bounds.x + 6.0f, bounds.y + 6.0f, 8.0f, 8.0f},
                    0.0f,
                    {0.0f, 0.0f, 0.0f, 1.0f});
            }
        };
        return probe;
    };

    // Toggle one view's marker and wait for its next localized partial commit.
    const auto next_partial = [&application](
                                  ui::UI& retained,
                                  ui::StandaloneWindow& window,
                                  const std::shared_ptr<ViewState>& state)
        -> std::optional<Diagnostics> {
        const auto before = PlatformTestAccess::scene_diagnostics(window);
        state->changed = !state->changed;
        retained.invalidate({6.0f, 6.0f, 8.0f, 8.0f});
        if (!wait_for_partial(application, window,
                              before.partial_scene_updates + 1)) {
            return std::nullopt;
        }
        return PlatformTestAccess::scene_diagnostics(window);
    };

    const auto same_diagnostics = [](const Diagnostics& a, const Diagnostics& b) {
        return a.scene_allocations == b.scene_allocations &&
               a.scene_builds == b.scene_builds &&
               a.partial_scene_updates == b.partial_scene_updates &&
               a.presentations == b.presentations &&
               a.failed_exposes == b.failed_exposes &&
               a.deferred_redraw_attempts == b.deferred_redraw_attempts &&
               a.deferred_redraw_rejections == b.deferred_redraw_rejections &&
               a.redraw_requests_during_render == b.redraw_requests_during_render &&
               a.scene_width == b.scene_width &&
               a.scene_height == b.scene_height &&
               a.last_update_x == b.last_update_x &&
               a.last_update_y == b.last_update_y &&
               a.last_update_width == b.last_update_width &&
               a.last_update_height == b.last_update_height &&
               a.scene_valid == b.scene_valid &&
               a.full_repaint_required == b.full_repaint_required &&
               a.present_pending == b.present_pending;
    };

    // View A is created first and commits its localized partial while it is the
    // active view. View B is created next, becomes the active view and commits
    // its own localized partial; both commits must stay localized.
    auto state_a = std::make_shared<ViewState>();
    auto retained_a =
        std::make_unique<ui::UI>(ui::Stack{Probe{make_probe(state_a)}});
    auto window_a = std::make_unique<ui::StandaloneWindow>(
        application,
        *retained_a,
        ui::WindowDesc{.title = "T096 sibling isolation A",
                       .size = {48.0f, 32.0f},
                       .resizable = true});
    if (!window_a->valid()) return failure("sibling isolation view A creation failed");
    if (!activate_and_settle(*retained_a, application, *window_a)) {
        return failure("sibling isolation view A did not settle after activation");
    }
    const auto a_partial = next_partial(*retained_a, *window_a, state_a);
    if (!a_partial) {
        return failure("sibling isolation view A partial update did not complete");
    }
    if (!a_partial->scene_valid || a_partial->full_repaint_required ||
        (a_partial->last_update_width == a_partial->scene_width &&
         a_partial->last_update_height == a_partial->scene_height)) {
        return failure("sibling isolation view A partial update was not localized");
    }

    auto state_b = std::make_shared<ViewState>();
    auto retained_b =
        std::make_unique<ui::UI>(ui::Stack{Probe{make_probe(state_b)}});
    auto window_b = std::make_unique<ui::StandaloneWindow>(
        application,
        *retained_b,
        ui::WindowDesc{.title = "T096 sibling isolation B",
                       .size = {48.0f, 32.0f},
                       .resizable = true});
    if (!window_b->valid()) return failure("sibling isolation view B creation failed");
    if (!activate_and_settle(*retained_b, application, *window_b)) {
        return failure("sibling isolation view B did not settle after activation");
    }
    const auto b_partial = next_partial(*retained_b, *window_b, state_b);
    if (!b_partial) {
        return failure("sibling isolation view B partial update did not complete");
    }
    if (!b_partial->scene_valid || b_partial->full_repaint_required ||
        (b_partial->last_update_width == b_partial->scene_width &&
         b_partial->last_update_height == b_partial->scene_height)) {
        return failure("sibling isolation view B partial update was not localized");
    }
    if (!settle(application, *window_b)) {
        return failure("sibling isolation view B did not settle after its partial update");
    }

    const auto b_marker_before = read_pixel(application, *window_b, {8.0f, 8.0f});
    const auto b_sibling_before = read_pixel(application, *window_b, {30.0f, 8.0f});
    if (!b_marker_before || !opaque_black(*b_marker_before) ||
        !b_sibling_before || b_sibling_before->r < 240 ||
        b_sibling_before->g > 10) {
        return failure("sibling isolation view B markers were not established");
    }
    const auto b_before_destroy = PlatformTestAccess::scene_diagnostics(*window_b);

    // Destroy view A (window first, then its retained UI). No pump runs between
    // the two diagnostics snapshots, so B must be bit-for-bit stable.
    window_a.reset();
    retained_a.reset();

    const auto b_after_destroy = PlatformTestAccess::scene_diagnostics(*window_b);
    if (!same_diagnostics(b_before_destroy, b_after_destroy)) {
        return failure("destroying view A changed view B's scene/damage/present state");
    }
    if (!b_after_destroy.scene_valid || b_after_destroy.full_repaint_required ||
        b_after_destroy.present_pending) {
        return failure("view B was not fully settled after destroying view A");
    }

    // The platform may hand focus to the surviving view when A is destroyed.
    // Activating B's retained tree and draining that lifecycle frame is not the
    // localized invalidation under test: it only ensures B is the active view
    // for the partial path (an inactive tree conservatively requires a full
    // repaint). Plain fills keep the surviving view effect-free.
    retained_b->activate(*window_b);
    if (!settle(application, *window_b)) {
        return failure("sibling isolation view B did not settle after activation");
    }
    const auto b_marker_after = read_pixel(application, *window_b, {8.0f, 8.0f});
    const auto b_sibling_after = read_pixel(application, *window_b, {30.0f, 8.0f});
    if (!b_marker_after || !same_pixel(*b_marker_after, *b_marker_before) ||
        !b_sibling_after || !same_pixel(*b_sibling_after, *b_sibling_before)) {
        return failure("destroying view A changed view B's presented pixels");
    }

    // B's next localized invalidation must still commit a partial update and
    // match an independent full render of the surviving view.
    const auto b_stable = PlatformTestAccess::scene_diagnostics(*window_b);
    const auto b_next = next_partial(*retained_b, *window_b, state_b);
    if (!b_next) {
        return failure("sibling isolation view B follow-up partial did not complete");
    }
    if (!b_next->scene_valid || b_next->full_repaint_required ||
        b_next->partial_scene_updates != b_stable.partial_scene_updates + 1 ||
        (b_next->last_update_width == b_next->scene_width &&
         b_next->last_update_height == b_next->scene_height)) {
        return failure("sibling isolation follow-up was not a localized partial update");
    }
    const auto b_partial_surface = read_surface(application, *window_b);
    if (!b_partial_surface) {
        return failure("sibling isolation partial surface readback failed");
    }
    retained_b->invalidate();
    if (!wait_for_builds(application, *window_b, b_next->scene_builds + 1)) {
        return failure("sibling isolation full reference did not complete");
    }
    const auto b_full_surface = read_surface(application, *window_b);
    if (!b_full_surface || !same_region(*b_partial_surface, *b_full_surface)) {
        return failure("sibling isolation partial output differs from the independent full render");
    }
    return true;
}

} // namespace

int main() {
    ui::Application application;
    if (!application.valid()) return fail("application creation failed");
    // The fixtures create and destroy independent windows in sequence; only an
    // explicit quit request should terminate the test application.
    application.set_quit_policy(ui::QuitPolicy::ExplicitOnly);

    BenchmarkCounters counters;
    if (!run_localized_partial_fixture(application, counters)) return 1;
    if (!run_filtered_effect_fixture(application)) return 1;
    if (!run_canonical_reset_fixture(application)) return 1;
    if (!run_sibling_destruction_isolation_fixture(application)) return 1;

    std::cout << "T096 benchmark: full " << counters.full_area << " px/"
              << counters.full_paints << " paints; partial "
              << counters.partial_area << " px/" << counters.partial_paints
              << " paints; present-only 0 px/" << counters.present_only_paints
              << " paints/" << counters.present_only_builds
              << " scene builds; scene allocations "
              << counters.scene_allocations << " reused="
              << (counters.storage_reused ? "yes" : "no")
              << "; filtered frames: full fallback (permitted); "
                 "readback/GPU waits: test-seam only\n";
    return 0;
}
