#include <nativeui/nativeui.hpp>

#include "../src/detail/native_view_fault_probe.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace {

enum class NativeFault {
    None,
    Layout,
};

struct FaultState {
    NativeFault fault{NativeFault::None};
    bool throw_focus_gain{};
    bool throw_pointer_cancel{};
    bool throw_focus_loss{};
    bool throw_deactivate{};
    int activate_calls{};
    int deactivate_calls{};
    int focus_gain_calls{};
    int focus_loss_calls{};
    int pointer_cancel_calls{};
};

class FaultComponent final : public ui::Component {
public:
    explicit FaultComponent(FaultState& state) : state_(&state) {}

    [[nodiscard]] bool focusable() const noexcept override { return true; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {120.0f, 60.0f};
    }

    void layout_children(
        ui::Rect,
        const std::vector<ui::ChildMetrics>&,
        std::vector<ui::ChildPlacement>&) const override {
        if (state_->fault == NativeFault::Layout) {
            throw std::runtime_error("T130 injected native construction layout failure");
        }
    }

    void activate(ui::LifecycleContext&) override {
        ++state_->activate_calls;
    }

    void deactivate(ui::LifecycleContext&) override {
        ++state_->deactivate_calls;
        if (state_->throw_deactivate) {
            throw std::runtime_error("T130 injected native teardown failure");
        }
    }

    void focus_changed(bool focused, ui::FocusContext&) override {
        if (focused) {
            ++state_->focus_gain_calls;
            if (state_->throw_focus_gain) {
                throw std::runtime_error("T130 injected activation focus failure");
            }
            return;
        }
        ++state_->focus_loss_calls;
        if (state_->throw_focus_loss) {
            throw std::runtime_error("T130 injected native focus-loss failure");
        }
    }

    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        if (event.type == ui::InputType::PointerDown) {
            context.capture_pointer();
            return ui::EventResult::Handled;
        }
        if (event.type == ui::InputType::PointerCancel) {
            ++state_->pointer_cancel_calls;
            if (state_->throw_pointer_cancel) {
                throw std::runtime_error("T130 injected native pointer-cancel failure");
            }
            return ui::EventResult::Handled;
        }
        return ui::EventResult::Ignored;
    }

    void paint(ui::PaintContext&) const override {}

private:
    FaultState* state_{};
};

class FaultRoot final {
public:
    explicit FaultRoot(FaultState& state) : state_(&state) {}

    ui::Spec spec() && {
        auto* state = state_;
        return ui::Spec{
            [state] { return std::make_unique<FaultComponent>(*state); },
            {}};
    }

private:
    FaultState* state_{};
};

bool pump(ui::Application& app, int iterations) {
    for (int i = 0; i < iterations; ++i) {
        if (!app.poll(0.0) && !app.quit_requested()) return false;
    }
    return true;
}

int fail(std::string_view message) {
    std::cerr << "FAIL t066_destroy_survivor_tests: " << message << '\n';
    return EXIT_FAILURE;
}

bool construction_fault_balanced(
    const ui::detail::NativeViewConstructionFaultResult& result,
    ui::detail::NativeViewConstructionFaultStage stage) {
    const auto ordinal = static_cast<unsigned>(stage);
    const auto at_least = [ordinal](ui::detail::NativeViewConstructionFaultStage threshold) {
        return ordinal >= static_cast<unsigned>(threshold);
    };
    const std::uint32_t realized =
        at_least(ui::detail::NativeViewConstructionFaultStage::AfterRealize) ? 1u : 0u;
    const std::uint32_t ime =
        at_least(ui::detail::NativeViewConstructionFaultStage::AfterImeCreate) ? 1u : 0u;
    const std::uint32_t constraints =
        at_least(ui::detail::NativeViewConstructionFaultStage::AfterSizeConstraints) ? 1u : 0u;
    const std::uint32_t resized =
        at_least(ui::detail::NativeViewConstructionFaultStage::AfterInitialResize) ? 1u : 0u;
    const std::uint32_t shown =
        at_least(ui::detail::NativeViewConstructionFaultStage::AfterShow) ? 1u : 0u;
    const std::uint32_t invalidation =
        at_least(ui::detail::NativeViewConstructionFaultStage::AfterInvalidationCallback) ? 1u : 0u;

    return result.injected_failure && !result.unexpected_failure && !result.unexpected_success &&
           result.world_acquired == 1u && result.world_released == 1u &&
           result.view_acquired == 1u && result.view_released == 1u &&
           result.realize_succeeded == realized && result.unrealize_released == realized &&
           result.ime_acquired == ime && result.ime_released == ime &&
           result.size_constraints_applied == constraints &&
           result.initial_resize_completed == resized &&
           result.show_completed == shown &&
           result.invalidation_attached == invalidation &&
           result.invalidation_cleared == invalidation;
}

#if defined(_WIN32)
void diagnose_failed_resize(const ui::StandaloneWindow& window) {
    const auto native = window.native_handle();
    const auto hwnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(native));
    const BOOL valid = hwnd ? IsWindow(hwnd) : FALSE;

    RECT rect{};
    const BOOL have_rect = valid ? GetWindowRect(hwnd, &rect) : FALSE;
    BOOL raw_resize = FALSE;
    DWORD raw_error = ERROR_SUCCESS;
    if (have_rect) {
        SetLastError(ERROR_SUCCESS);
        const int width = std::max(1L, rect.right - rect.left + 20L);
        const int height = std::max(1L, rect.bottom - rect.top + 10L);
        raw_resize = SetWindowPos(
            hwnd,
            nullptr,
            0,
            0,
            width,
            height,
            SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
        raw_error = raw_resize ? ERROR_SUCCESS : GetLastError();
    }

    std::cerr << "T066 Windows survivor diagnostic: native_handle=" << native
              << " IsWindow=" << (valid ? 1 : 0)
              << " GetWindowRect=" << (have_rect ? 1 : 0)
              << " raw_SetWindowPos=" << (raw_resize ? 1 : 0)
              << " raw_error=" << raw_error
              << " should_close=" << (window.should_close() ? 1 : 0)
              << " is_closed=" << (window.is_closed() ? 1 : 0)
              << " last_error='" << window.last_error() << "'\n";
}
#endif

} // namespace

int main() {
    ui::Application app;
    if (!app.valid()) return fail("Application construction failed");

    // A ViewCore construction failure after native realization must not leave a
    // borrowed Application world with a leaked/dangling Pugl view. Trigger the
    // failure from synchronous initial layout, then immediately reuse the same
    // Application for live windows below.
    FaultState construction_fault{.fault = NativeFault::Layout};
    ui::UI failing_tree{FaultRoot{construction_fault}};
    {
        ui::StandaloneWindow failed(
            app,
            failing_tree,
            ui::WindowDesc{.title = "T130 construction failure",
                           .size = {300.0f, 160.0f},
                           .resizable = true});
        if (failed.valid()) return fail("injected construction failure produced a valid window");
        if (failed.native_handle()) return fail("failed construction retained a public native handle");
    }

    // Keep one healthy Application-owned window alive while the temporary
    // activation rollback fixture is destroyed. StandaloneWindow intentionally
    // requests Application quit when the final live window closes, so allowing
    // the temporary fixture to become the sole live window would make the later
    // survivor construction test invalid rather than exercise T130 recovery.
    ui::UI tree_b{ui::Label{"T066 direct-destroy B"}};
    auto b = std::make_unique<ui::StandaloneWindow>(
        app,
        tree_b,
        ui::WindowDesc{.title = "T066 direct-destroy B",
                       .size = {360.0f, 180.0f},
                       .resizable = true});
    if (!b->valid()) return fail("survivor B construction failed");

    // Probe each meaningful owned-world ViewCore construction boundary with a
    // real embedded native child. Every injected failure must unwind each
    // resource acquired before that boundary exactly once while leaving the
    // healthy Application-owned parent untouched.
    constexpr std::array construction_stages{
        ui::detail::NativeViewConstructionFaultStage::AfterViewCreation,
        ui::detail::NativeViewConstructionFaultStage::AfterRealize,
        ui::detail::NativeViewConstructionFaultStage::AfterImeCreate,
        ui::detail::NativeViewConstructionFaultStage::AfterSizeConstraints,
        ui::detail::NativeViewConstructionFaultStage::AfterInitialResize,
        ui::detail::NativeViewConstructionFaultStage::AfterShow,
        ui::detail::NativeViewConstructionFaultStage::AfterInvalidationCallback,
    };
    for (const auto stage : construction_stages) {
        ui::UI probe_ui{ui::Spacer{120.0f, 60.0f}};
        const auto result = ui::detail::exercise_native_view_construction_fault(
            probe_ui,
            *b,
            b->native_handle(),
            {180.0f, 90.0f},
            stage);
        if (!construction_fault_balanced(result, stage)) {
            return fail("ViewCore constructor-stage fault cleanup was not balanced");
        }
        if (app.quit_requested() || !b->native_handle() || b->should_close()) {
            return fail("ViewCore constructor-stage fault damaged the healthy parent window");
        }
    }
    if (!b->set_title("T066 B survives construction fault matrix")) {
        return fail("survivor B title update failed after construction fault matrix");
    }

    // Activation is not committed merely because component activate() returned.
    // A later focus callback can still fail after active/platform publication.
    // The failed transition must perform best-effort deactivation and a retry on
    // the same UI/platform must execute a fresh activation rather than no-op on
    // stale active_ state.
    {
        FaultState activation_fault{};
        ui::UI activation_tree{FaultRoot{activation_fault}};
        ui::StandaloneWindow activation_window(
            app,
            activation_tree,
            ui::WindowDesc{.title = "T130 activation rollback",
                           .size = {300.0f, 160.0f},
                           .resizable = true});
        if (!activation_window.valid()) return fail("activation rollback window construction failed");

        // Normalize any native focus delivered during construction before the
        // deterministic direct C++ activation fault below.
        activation_tree.deactivate(activation_window);
        const int activate_before = activation_fault.activate_calls;
        const int deactivate_before = activation_fault.deactivate_calls;
        const int focus_loss_before = activation_fault.focus_loss_calls;

        activation_fault.throw_focus_gain = true;
        bool propagated_focus_failure = false;
        try {
            activation_tree.activate(activation_window);
        } catch (const std::runtime_error& error) {
            propagated_focus_failure =
                std::string_view{error.what()} == "T130 injected activation focus failure";
        }
        if (!propagated_focus_failure) return fail("activation focus failure did not propagate");
        if (activation_fault.activate_calls != activate_before + 1) {
            return fail("failed activation did not run component activate exactly once");
        }
        if (activation_fault.deactivate_calls != deactivate_before + 1) {
            return fail("failed post-activate focus publication did not roll back deactivation");
        }
        if (activation_fault.focus_loss_calls != focus_loss_before + 1) {
            return fail("failed focus publication did not roll back focus state");
        }

        activation_fault.throw_focus_gain = false;
        activation_tree.activate(activation_window);
        if (activation_fault.activate_calls != activate_before + 2) {
            return fail("failed activation left stale active state and blocked retry");
        }
        activation_tree.deactivate(activation_window);
        if (activation_fault.deactivate_calls != deactivate_before + 2) {
            return fail("retried activation did not deactivate cleanly");
        }
    }
    if (app.quit_requested()) {
        return fail("activation rollback window destruction requested quit while B remained live");
    }

    FaultState teardown_fault{};
    ui::UI tree_a{FaultRoot{teardown_fault}};
    auto a = std::make_unique<ui::StandaloneWindow>(
        app,
        tree_a,
        ui::WindowDesc{.title = "T130 throwing teardown A",
                       .size = {420.0f, 220.0f},
                       .resizable = true});
    if (!a->valid()) return fail("throwing teardown A construction failed");

    // Make teardown deterministic without depending on native focus timing, and
    // establish a real retained capture so close_native_view() first exercises
    // PointerCancel before focus loss and component deactivation.
    tree_a.activate(*a);
    if (teardown_fault.activate_calls < 1 || teardown_fault.focus_gain_calls < 1) {
        return fail("throwing teardown fixture did not become active and focused");
    }
    ui::InputEvent down{};
    down.type = ui::InputType::PointerDown;
    down.position = {10.0f, 10.0f};
    if (tree_a.dispatch(down, *a) != ui::EventResult::Handled) {
        return fail("throwing teardown fixture did not establish pointer capture");
    }

    const int activate_before_close = teardown_fault.activate_calls;
    const int focus_loss_before_close = teardown_fault.focus_loss_calls;
    const int deactivate_before_close = teardown_fault.deactivate_calls;
    teardown_fault.throw_pointer_cancel = true;
    teardown_fault.throw_focus_loss = true;
    teardown_fault.throw_deactivate = true;

    // All three retained callbacks throw during ViewCore teardown. The native
    // boundary must still close A, attempt every later retained phase, publish
    // inactive/no-platform Tree state and leave sibling B fully usable.
    a.reset();
    if (teardown_fault.pointer_cancel_calls < 1) {
        return fail("throwing pointer cancellation was not attempted");
    }
    if (teardown_fault.focus_loss_calls != focus_loss_before_close + 1) {
        return fail("throwing focus loss did not run exactly once during teardown");
    }
    if (teardown_fault.deactivate_calls != deactivate_before_close + 1) {
        return fail("throwing component deactivate was not attempted exactly once");
    }
    if (app.quit_requested()) return fail("destroying A requested quit while B remained live");
    if (!b->native_handle()) return fail("surviving B lost its native handle");
    if (b->should_close()) return fail("surviving B became closing after destroying A");

    // Prove the failed native teardown did not leave Tree::active_ or the dead
    // PlatformServices pointer published. Reuse the exact same UI against B;
    // the activation hook must run again rather than returning from stale state.
    teardown_fault.throw_pointer_cancel = false;
    teardown_fault.throw_focus_loss = false;
    teardown_fault.throw_deactivate = false;
    tree_a.activate(*b);
    if (teardown_fault.activate_calls != activate_before_close + 1) {
        return fail("failed teardown left stale active state and blocked reactivation");
    }
    tree_a.deactivate(*b);
    if (teardown_fault.deactivate_calls != deactivate_before_close + 2) {
        return fail("reactivated teardown fixture did not deactivate cleanly");
    }

    // EmbeddedView owns its own module world/view/IME and must provide the same
    // no-throw/best-effort retained teardown guarantee as StandaloneWindow. Keep
    // B alive as the host parent and prove its native state survives all three
    // injected retained teardown exceptions.
    FaultState embedded_fault{};
    ui::UI embedded_tree{FaultRoot{embedded_fault}};
    auto embedded = std::make_unique<ui::EmbeddedView>(
        embedded_tree,
        b->native_handle(),
        ui::Size{260.0f, 140.0f});
    if (!embedded->native_handle()) return fail("throwing EmbeddedView construction failed");
    embedded_tree.activate(*embedded);
    if (embedded_fault.activate_calls < 1 || embedded_fault.focus_gain_calls < 1) {
        return fail("throwing EmbeddedView fixture did not become active and focused");
    }
    if (embedded_tree.dispatch(down, *embedded) != ui::EventResult::Handled) {
        return fail("throwing EmbeddedView fixture did not establish pointer capture");
    }
    const int embedded_activate_before_close = embedded_fault.activate_calls;
    const int embedded_focus_loss_before_close = embedded_fault.focus_loss_calls;
    const int embedded_deactivate_before_close = embedded_fault.deactivate_calls;
    embedded_fault.throw_pointer_cancel = true;
    embedded_fault.throw_focus_loss = true;
    embedded_fault.throw_deactivate = true;
    embedded.reset();
    if (embedded_fault.pointer_cancel_calls < 1) {
        return fail("EmbeddedView throwing pointer cancellation was not attempted");
    }
    if (embedded_fault.focus_loss_calls != embedded_focus_loss_before_close + 1) {
        return fail("EmbeddedView throwing focus loss did not run exactly once");
    }
    if (embedded_fault.deactivate_calls != embedded_deactivate_before_close + 1) {
        return fail("EmbeddedView throwing deactivate was not attempted exactly once");
    }
    if (app.quit_requested() || !b->native_handle() || b->should_close()) {
        return fail("throwing EmbeddedView teardown damaged its parent window");
    }
    embedded_fault.throw_pointer_cancel = false;
    embedded_fault.throw_focus_loss = false;
    embedded_fault.throw_deactivate = false;
    embedded_tree.activate(*b);
    if (embedded_fault.activate_calls != embedded_activate_before_close + 1) {
        return fail("EmbeddedView teardown left stale active state and blocked reactivation");
    }
    embedded_tree.deactivate(*b);
    if (embedded_fault.deactivate_calls != embedded_deactivate_before_close + 2) {
        return fail("EmbeddedView teardown fixture did not deactivate cleanly after reuse");
    }

    // Keep resize as the first native mutation after sibling destruction. This
    // matches the T060 regression sequence exactly and prevents an unrelated
    // synchronous title update from masking a stale native-window condition.
    if (!b->set_size({380.0f, 190.0f})) {
#if defined(_WIN32)
        diagnose_failed_resize(*b);
#endif
        return fail("surviving B resize failed");
    }
    if (!b->set_title("T066 direct-destroy B survives")) {
        return fail("surviving B title update failed");
    }
    if (!pump(app, 4)) return fail("Application stopped while B remained live");

    b.reset();
    if (!app.quit_requested()) return fail("destroying final window did not request quit");

    std::cout << "PASS t066_destroy_survivor_tests\n";
    return EXIT_SUCCESS;
}
