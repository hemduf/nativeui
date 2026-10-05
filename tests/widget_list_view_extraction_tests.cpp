#include "test_support.hpp"
#include <nativeui/list_view.hpp>

#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {
void keyboard_pointer_disabled_rows_and_read_only_keep_legacy_selection() {
    ui::State<std::optional<int>> selected{1}; int activations=0,key=0;
    ui::State<bool> read_only{true};
    ui::UI tree{ui::ReadOnly{read_only,ui::ListView{selected}.item(1,ui::Spacer{80.0f,24.0f})
        .item(2,ui::Spacer{80.0f,24.0f},false).item(3,ui::Spacer{80.0f,24.0f})
        .on_activate([&](const int& value) { ++activations; key=value; })}};
    test::MockPlatform platform;
    tree.resize({80.0f,80.0f}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Down),platform);
    NUI_CHECK(selected.get()==std::optional<int>{3});
    tree.dispatch(test::key(ui::Key::Space),platform);
    NUI_CHECK(activations==1 && key==3);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,12.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,12.0f),platform);
    NUI_CHECK(selected.get()==std::optional<int>{1} && activations==2 && key==1);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,36.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,36.0f),platform);
    NUI_CHECK(selected.get()==std::optional<int>{1} && activations==2);
    NUI_CHECK(platform.pointer_capture_begin_count==platform.pointer_capture_end_count);
}
struct CustomKey {
    int value;
    std::shared_ptr<bool> fail;
    friend bool operator==(const CustomKey& a,const CustomKey& b) {
        if (*a.fail || *b.fail) throw std::runtime_error("key equality");
        return a.value==b.value;
    }
};
void arbitrary_non_hashable_keys_use_eager_path_and_recover_equality_faults() {
    const auto fail=std::make_shared<bool>(false);
    ui::State<std::optional<CustomKey>> selected{CustomKey{1,fail}};
    int activated=0;
    ui::UI tree{ui::ListView{selected.binding()}.item(CustomKey{1,fail},ui::Spacer{80.0f,24.0f})
        .item(CustomKey{2,fail},ui::Spacer{80.0f,24.0f}).on_activate([&](const CustomKey& key) { activated=key.value; })};
    tree.resize({80.0f,60.0f}); test::MockPlatform platform; tree.activate(platform);
    *fail=true; bool threw=false;
    try { tree.dispatch(test::key(ui::Key::Down),platform); } catch (const std::runtime_error&) { threw=true; }
    *fail=false; NUI_CHECK(threw && selected.get()->value==1 && activated==0);
    tree.dispatch(test::key(ui::Key::Down),platform); tree.dispatch(test::key(ui::Key::Enter),platform);
    NUI_CHECK(selected.get()->value==2 && activated==2);
    bool duplicate=false;
    try { (void)ui::ListView{selected}.item(CustomKey{1,fail},ui::Spacer{0.0f}).item(CustomKey{1,fail},ui::Spacer{0.0f}); }
    catch (const std::invalid_argument&) { duplicate=true; }
    NUI_CHECK(duplicate);
}
void copied_specs_keep_contacts_isolated_and_expired_bindings_inert() {
    ui::State<std::optional<int>> selected{1}; int activations=0;
    auto spec=ui::make_spec(ui::ListView{selected}.item(1,ui::Spacer{80.0f,24.0f})
        .item(2,ui::Spacer{80.0f,24.0f}).on_activate([&](const int&) { ++activations; }));
    ui::UI first{ui::Spec{spec}},second{ui::Spec{spec}};
    test::MockPlatform first_platform,second_platform;
    first.resize({80.0f,60.0f}); second.resize({80.0f,60.0f}); first.activate(first_platform); second.activate(second_platform);
    first.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,36.0f),first_platform);
    second.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,36.0f),second_platform);
    NUI_CHECK(selected.get()==std::optional<int>{1} && activations==0);
    first.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,36.0f),first_platform);
    NUI_CHECK(selected.get()==std::optional<int>{2} && activations==1);
    auto source=std::make_unique<ui::State<std::optional<int>>>(std::optional<int>{1});
    int retired_activations=0;
    ui::UI expired{ui::ListView{source->binding()}.item(1,ui::Spacer{80.0f,24.0f})
        .item(2,ui::Spacer{80.0f,24.0f}).on_activate([&](const int&) { ++retired_activations; })};
    test::MockPlatform expired_platform; expired.resize({80.0f,60.0f}); expired.activate(expired_platform); source.reset();
    expired.dispatch(test::key(ui::Key::Down),expired_platform); expired.dispatch(test::key(ui::Key::Enter),expired_platform);
    NUI_CHECK(retired_activations==0);
}
struct ThrowOnCopy {
    std::shared_ptr<bool> fail;
    std::shared_ptr<int> calls;
    ThrowOnCopy(std::shared_ptr<bool> fail,std::shared_ptr<int> calls):fail(std::move(fail)),calls(std::move(calls)) {}
    ThrowOnCopy(const ThrowOnCopy& other):fail(other.fail),calls(other.calls) { if (*fail) throw std::runtime_error("callback copy"); }
    void operator()(const int&) const { ++*calls; }
};
void throwing_activation_copy_releases_contact_before_fallible_work() {
    const auto fail=std::make_shared<bool>(false); const auto calls=std::make_shared<int>(0);
    ui::State<std::optional<int>> selected{1};
    ui::UI tree{ui::ListView{selected}.item(1,ui::Spacer{80.0f,24.0f}).item(2,ui::Spacer{80.0f,24.0f})
        .on_activate(ThrowOnCopy{fail,calls})};
    test::MockPlatform platform; tree.resize({80.0f,60.0f}); tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,36.0f),platform); *fail=true;
    bool threw=false;
    try { tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,36.0f),platform); } catch (const std::runtime_error&) { threw=true; }
    *fail=false; NUI_CHECK(threw && selected.get()==std::optional<int>{1} && *calls==0);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,36.0f),platform);
    NUI_CHECK(selected.get()==std::optional<int>{1} && *calls==0);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,36.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,36.0f),platform);
    NUI_CHECK(selected.get()==std::optional<int>{2} && *calls==1);
    NUI_CHECK(platform.pointer_capture_begin_count==platform.pointer_capture_end_count);
}
void observer_first_failure_recovers_selected_render_without_replaying_activation() {
    ui::State<std::optional<int>> selected{1}; bool fail=false; int activations=0;
    const auto subscription=selected.observe([&](const auto&) { if (fail) throw std::runtime_error("first observer"); });
    ui::ListViewStyle style; style.base.row_fill=ui::Color{1.0f,0.0f,0.0f,1.0f};
    style.selected.row_fill=ui::Color{0.0f,1.0f,0.0f,1.0f}; style.selected.row_accent_width=0.0f;
    style.base.row_corner_radius=0.0f; style.base.row_horizontal_inset=0.0f; style.base.row_vertical_inset=0.0f;
    ui::UI tree{ui::ListView{selected}.item(1,ui::Spacer{80.0f,24.0f}).item(2,ui::Spacer{80.0f,24.0f})
        .on_activate([&](const int&) { ++activations; }).style(style)};
    test::MockPlatform platform; tree.resize({80.0f,60.0f}); tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,36.0f),platform); fail=true;
    bool threw=false;
    try { tree.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,36.0f),platform); } catch (const std::runtime_error&) { threw=true; }
    fail=false; NUI_CHECK(threw && selected.get()==std::optional<int>{2} && activations==0);
    ui::HeadlessRenderer renderer{{80.0f,60.0f},1.0f}; NUI_CHECK(renderer.render(tree));
    NUI_CHECK(renderer.pixel(10,36).g==255 && renderer.pixel(10,12).r==255);
    NUI_CHECK(activations==0 && platform.pointer_capture_begin_count==platform.pointer_capture_end_count);
}
void fixed_virtual_path_preserves_bounded_materialization_and_semantic_generations() {
    ui::State<std::optional<int>> selected{0}; std::size_t built=0;
    using Controller=ui::VirtualListState<int>;
    Controller state{selected,20.0f,[&](const Controller::Item&) { ++built; return ui::Spacer{80.0f,20.0f}; }};
    std::vector<Controller::Item> rows; rows.reserve(10000);
    for (int i=0;i<10000;++i) rows.emplace_back(i,"row "+std::to_string(i));
    NUI_CHECK(state.replace(std::move(rows)));
    ui::UI tree{ui::ListView{state}}; tree.resize({80.0f,100.0f});
    ui::HeadlessRenderer renderer{{80.0f,100.0f},1.0f}; NUI_CHECK(renderer.render(tree));
    NUI_CHECK(built<=20);
    const auto generation=state.dataset_generation(); const auto metadata=state.metadata_snapshot(); const auto before=built;
    const auto snapshot=state.semantic_children({0.0f,0.0f,80.0f,100.0f});
    NUI_CHECK(snapshot.item_at(9999).has_value());
    NUI_CHECK(built==before && state.metadata_snapshot().get()==metadata.get() && state.dataset_generation()==generation);
    NUI_CHECK(state.scroll_to_index(5000)); NUI_CHECK(renderer.render(tree));
    NUI_CHECK(built<=before+20 && state.dataset_generation()==generation);
    NUI_CHECK(!state.scroll_to_index(10000));
    NUI_CHECK(!state.replace({Controller::Item{1,"one"},Controller::Item{1,"duplicate"}}));
    NUI_CHECK(state.dataset_generation()==generation && state.metadata_snapshot().get()==metadata.get());
}
void suite() {
    keyboard_pointer_disabled_rows_and_read_only_keep_legacy_selection();
    arbitrary_non_hashable_keys_use_eager_path_and_recover_equality_faults();
    copied_specs_keep_contacts_isolated_and_expired_bindings_inert();
    throwing_activation_copy_releases_contact_before_fallible_work();
    observer_first_failure_recovers_selected_render_without_replaying_activation();
    fixed_virtual_path_preserves_bounded_materialization_and_semantic_generations();
}
}
int main() { return test::run("widget_list_view_extraction",&suite); }
