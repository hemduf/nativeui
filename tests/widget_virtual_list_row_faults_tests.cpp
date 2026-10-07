#include "test_support.hpp"
#include <nativeui/detail/virtual_list_row.hpp>

#include <functional>
#include <memory>
#include <stdexcept>

namespace {
struct Probe {
    bool logical_capture{};
    bool fail_copy{};
    int actions{};
    std::function<void()> presentation_callback;
};
struct ThrowOnCopy {
    std::shared_ptr<Probe> probe;
    explicit ThrowOnCopy(std::shared_ptr<Probe> value):probe(std::move(value)) {}
    ThrowOnCopy(const ThrowOnCopy& other):probe(other.probe) { if (probe->fail_copy) throw std::runtime_error("activation copy"); }
    bool operator()() const { ++probe->actions; return true; }
};
ui::Spec row(const std::shared_ptr<Probe>& probe) {
    std::function<bool()> activation=ThrowOnCopy{probe};
    return {[probe,activation] {
        return std::make_unique<ui::detail::VirtualListRowInteractionComponent>(
            [probe] { probe->logical_capture=true; return true; },[probe] { probe->logical_capture=false; },activation,
            [probe](bool before,bool after) { const auto callback=probe->presentation_callback; if (callback) callback(); return before!=after; });
    },{ui::make_spec(ui::Spacer{80.0f,20.0f})}};
}
void copy_fault_disarms_both_captures_before_propagation() {
    const auto probe=std::make_shared<Probe>(); ui::UI tree{row(probe)}; test::MockPlatform platform;
    tree.resize({80.0f,40.0f}); tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,10.0f),platform); NUI_CHECK(probe->logical_capture);
    probe->fail_copy=true; bool caught{};
    try { tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,10.0f),platform); } catch (const std::runtime_error&) { caught=true; }
    probe->fail_copy=false;
    NUI_CHECK(caught && !probe->logical_capture && probe->actions==0);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,10.0f),platform); NUI_CHECK(probe->actions==0);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,10.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,10.0f),platform);
    NUI_CHECK(probe->actions==1 && !probe->logical_capture);
    NUI_CHECK(platform.pointer_capture_begin_count==platform.pointer_capture_end_count);
}
void release_invalidation_cannot_activate_a_retired_row() {
    const auto probe=std::make_shared<Probe>(); ui::State<bool> shown{true};
    ui::UI tree{ui::If{shown,row(probe)}}; test::MockPlatform platform;
    tree.resize({80.0f,40.0f}); tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,10.0f),platform);
    ui::HeadlessRenderer renderer{{80.0f,40.0f},1.0f}; NUI_CHECK(renderer.render(tree));
    bool remove=true;
    tree.set_invalidation_callback([&](ui::Rect) { if (std::exchange(remove,false)) { shown.set(false); tree.resize({80.0f,40.0f}); } });
    tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,10.0f),platform);
    NUI_CHECK(!shown.get() && probe->actions==0 && !probe->logical_capture);
    NUI_CHECK(platform.pointer_capture_begin_count==platform.pointer_capture_end_count);
}
void presentation_callback_can_remove_a_row_during_down() {
    const auto probe=std::make_shared<Probe>(); ui::State<bool> shown{true};
    ui::UI tree{ui::If{shown,row(probe)}}; test::MockPlatform platform;
    tree.resize({80.0f,40.0f}); tree.activate(platform);
    bool remove=true;
    probe->presentation_callback=[&] { if (std::exchange(remove,false)) { shown.set(false); tree.resize({80.0f,40.0f}); } };
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,10.0f),platform);
    NUI_CHECK(!shown.get() && !probe->logical_capture && probe->actions==0);
    NUI_CHECK(platform.pointer_capture_begin_count==platform.pointer_capture_end_count);
}
void suite() { copy_fault_disarms_both_captures_before_propagation(); release_invalidation_cannot_activate_a_retired_row(); presentation_callback_can_remove_a_row_during_down(); }
}
int main(int argc,char** argv) {
    if (argc>1 && std::string_view{argv[1]}=="copy") return test::run("virtual_row_copy",&copy_fault_disarms_both_captures_before_propagation);
    if (argc>1 && std::string_view{argv[1]}=="retire") return test::run("virtual_row_retire",&release_invalidation_cannot_activate_a_retired_row);
    if (argc>1 && std::string_view{argv[1]}=="presentation") return test::run("virtual_row_presentation",&presentation_callback_can_remove_a_row_during_down);
    return test::run("virtual_row_faults",&suite);
}
