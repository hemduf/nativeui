#include "test_support.hpp"
#import <AppKit/AppKit.h>

namespace {
NSView* native(ui::NativeViewHandle handle) {
    return (__bridge NSView*)reinterpret_cast<void*>(handle);
}
struct HideFailure final : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct HideProbe {
    ui::State<float> value{0.5f};
    std::string trace;
    int focus_in{};
    int focus_out{};
    int deactivations{};
    bool throw_cancel{};
    bool throw_blur{};
    bool throw_deactivate{};
};

class HideProbeComponent final : public ui::Component {
public:
    explicit HideProbeComponent(HideProbe& probe)
        : probe_(probe), edit_(probe.value.binding(), {
            [&](ui::EditSource) { probe_.trace += 'B'; },
            [&](const float&, ui::EditSource) { probe_.trace += 'C'; },
            [&](ui::EditSource) { probe_.trace += 'E'; },
            [&](ui::EditSource) {
                probe_.trace += 'X';
                if (probe_.throw_cancel) throw HideFailure{"cancel"};
            }}) {}

    bool focusable() const noexcept override { return true; }
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {200, 120}; }
    void paint(ui::PaintContext&) const override {}
    void focus_changed(bool focused, ui::FocusContext& context) override {
        if (focused) {
            ++probe_.focus_in;
            context.set_text_input(true, {10, 20, 30, 18}, 2);
        } else {
            ++probe_.focus_out;
            // Deliberately leave native IME active: hide must clean it even
            // when both this callback and deactivate fail.
            if (probe_.throw_blur) throw HideFailure{"blur"};
        }
    }
    void deactivate(ui::LifecycleContext&) override {
        ++probe_.deactivations;
        if (probe_.throw_deactivate) throw HideFailure{"deactivate"};
    }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext& context) override {
        switch (event.type) {
            case ui::InputType::PointerDown:
                context.capture_pointer();
                edit_.begin(ui::EditSource::Pointer);
                return ui::EventResult::Handled;
            case ui::InputType::PointerMove:
                edit_.update(0.75f);
                return ui::EventResult::Handled;
            case ui::InputType::PointerUp:
                context.release_pointer();
                edit_.end();
                return ui::EventResult::Handled;
            case ui::InputType::PointerCancel:
                edit_.cancel();
                return ui::EventResult::Handled;
            default:
                return ui::EventResult::Ignored;
        }
    }
private:
    HideProbe& probe_;
    ui::EditSession<float> edit_;
};

NSRect candidate_rect(ui::EmbeddedView& child) {
    id<NSTextInputClient> client = (id<NSTextInputClient>)native(child.native_handle());
    return [client firstRectForCharacterRange:NSMakeRange(0, 0) actualRange:nullptr];
}

void hide_callback_failures() {
    ui::Application application;
    ui::UI parent_ui{ui::Spacer{440, 260}};
    ui::StandaloneWindow parent{application, parent_ui,
        ui::WindowDesc{.title = "Hide exception recovery", .size = {440, 260}}};
    NSWindow* window = native(parent.native_handle()).window;
    ui::State<float> sibling_value{0.5f};
    std::string sibling_trace;
    ui::UI sibling_ui{ui::Knob{"Sibling", sibling_value}.on_edit({
        [&](ui::EditSource) { sibling_trace += 'B'; },
        [&](const float&, ui::EditSource) { sibling_trace += 'C'; },
        [&](ui::EditSource) { sibling_trace += 'E'; }, {}})};
    ui::EmbeddedView sibling{sibling_ui, parent.native_handle(), {200, 120}};

    for (int failure = 0; failure < 4; ++failure) {
        HideProbe probe;
        probe.throw_cancel = failure == 0 || failure == 3;
        probe.throw_blur = failure == 1 || failure == 3;
        probe.throw_deactivate = failure == 2 || failure == 3;
        ui::UI child_ui{ui::Spec{[&] { return std::make_unique<HideProbeComponent>(probe); }, {}}};
        ui::EmbeddedView child{child_ui, parent.native_handle(), {200, 120}};
        child_ui.activate(child);
        NUI_CHECK(candidate_rect(child).size.height > 1.0);
        child_ui.dispatch(test::pointer(ui::InputType::PointerDown, 100, 60), child);
        NUI_CHECK(probe.trace == "B");
        const auto blur_before = probe.focus_out;
        const auto deactivate_before = probe.deactivations;
        bool caught = false;
        try {
            child.hide();
        } catch (const HideFailure& error) {
            caught = true;
            NUI_CHECK(std::string{error.what()} ==
                (probe.throw_cancel ? "cancel" : probe.throw_blur ? "blur" : "deactivate"));
        }
        NUI_CHECK(caught);
        NUI_CHECK(!child.visible() && native(child.native_handle()).hidden);
        NUI_CHECK(probe.trace == "BX");
        NUI_CHECK(probe.focus_out == blur_before + 1);
        NUI_CHECK(probe.deactivations == deactivate_before + 1);
        NUI_CHECK(candidate_rect(child).size.height == 1.0);
        NUI_CHECK(child_ui.cancel_pointer(child) == ui::EventResult::Ignored);
        NUI_CHECK(child.hide());
        NUI_CHECK(probe.trace == "BX");
        NUI_CHECK(probe.focus_out == blur_before + 1);
        NUI_CHECK(probe.deactivations == deactivate_before + 1);
        NUI_CHECK(window.visible);

        // Failure in A must leave the independently focused/edited B usable.
        sibling_trace.clear();
        sibling_value.set(0.5f);
        sibling_ui.activate(sibling);
        sibling_ui.dispatch(test::key(ui::Key::Right), sibling);
        NUI_CHECK(sibling_trace == "BCE" && sibling_value.get() > 0.5f);
        NUI_CHECK(sibling.visible() && !native(sibling.native_handle()).hidden);

        probe.throw_cancel = probe.throw_blur = probe.throw_deactivate = false;
        probe.trace.clear();
        const auto focus_before = probe.focus_in;
        NUI_CHECK(child.show());
        child_ui.activate(child);
        NUI_CHECK(probe.focus_in > focus_before);
        NUI_CHECK(candidate_rect(child).size.height > 1.0);
        child_ui.dispatch(test::pointer(ui::InputType::PointerDown, 100, 60), child);
        child_ui.dispatch(test::pointer(ui::InputType::PointerMove, 120, 40), child);
        child_ui.dispatch(test::pointer(ui::InputType::PointerUp, 120, 40), child);
        NUI_CHECK(probe.trace == "BCE" && probe.value.get() == 0.75f);
        NUI_CHECK(child.hide());
        NUI_CHECK(probe.trace == "BCE");
        NUI_CHECK(candidate_rect(child).size.height == 1.0);
        child.request_close();
        NUI_CHECK(sibling.visible());
    }
    sibling.request_close();
}

void run() {
    ui::Application application;
    ui::UI parent_ui{ui::Spacer{400,240}};
    ui::StandaloneWindow parent{application,parent_ui,
        ui::WindowDesc{.title="Embedded visibility test",.size={400,240}}};
    NSWindow* window=native(parent.native_handle()).window;
    NSResponder* responder=window.firstResponder;
    const auto frame=window.frame;
    ui::State<float> value{0.5f}; std::string trace;
    ui::UI child_ui{ui::Knob{"v",value}.on_edit({
        [&](ui::EditSource){trace+='B';},{},{},[&](ui::EditSource){trace+='X';}})};
    ui::EmbeddedView child{child_ui,parent.native_handle(),{200,120},{},
        ui::EmbeddedViewOptions{.initially_visible=false}};
    NUI_CHECK(!child.visible()); NUI_CHECK(native(child.native_handle()).hidden);
    NUI_CHECK(window.firstResponder==responder);
    NUI_CHECK(window.visible); NUI_CHECK(NSEqualRects(frame,window.frame));
    NUI_CHECK(child.hide()); NUI_CHECK(child.show()); NUI_CHECK(child.show());
    NUI_CHECK(child.visible()); NUI_CHECK(!native(child.native_handle()).hidden);
    NUI_CHECK(window.firstResponder==responder);
    // Retained cancellation is exercised with a deterministic UI input route.
    test::MockPlatform platform; child_ui.activate(platform);
    child_ui.dispatch(test::pointer(ui::InputType::PointerDown,100,60),platform);
    NUI_CHECK(trace=="B"); NUI_CHECK(child.hide()); NUI_CHECK(trace=="BX");
    NUI_CHECK(child.hide()); NUI_CHECK(trace=="BX");
    NUI_CHECK(native(child.native_handle()).hidden); NUI_CHECK(window.visible);
    NUI_CHECK(child.set_size({180,100})); value.set(0.8f);
    for(int i=0;i<4;++i){application.poll(0);child.poll();}
    NUI_CHECK(!child.visible()); NUI_CHECK(child.show());
    ui::UI sibling_ui{ui::Spacer{80,80}};
    ui::EmbeddedView sibling{sibling_ui,parent.native_handle(),{80,80}};
    NUI_CHECK(sibling.visible()); NUI_CHECK(!native(sibling.native_handle()).hidden);
    child.hide(); NUI_CHECK(sibling.visible()); NUI_CHECK(!native(sibling.native_handle()).hidden);
    sibling.request_close(); child.request_close();
    NUI_CHECK(!child.visible()); NUI_CHECK(!child.show()); NUI_CHECK(!child.hide());
    NUI_CHECK(window.visible); NUI_CHECK(NSEqualRects(frame,window.frame));
}
}
int main() {
    return test::run("embedded visibility macOS", [] { run(); hide_callback_failures(); });
}
