#include "test_support.hpp"
#include <nativeui/form.hpp>
#include <nativeui/field.hpp>
#include <nativeui/fieldset.hpp>

#include <algorithm>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
struct Observation {
    ui::Rect bounds{};
    bool focused{};
    int mounts{};
    int unmounts{};
    ui::NodeId id{ui::kInvalidNodeId};
    ui::Size preferred{120.0f,28.0f};
    ui::Size minimum{30.0f,20.0f};
    std::optional<float> baseline{18.0f};
    std::string semantic_name;
    std::vector<float> measured_widths;
    bool fail_narrow{};
};
class ControlComponent final : public ui::Component {
public:
    explicit ControlComponent(std::shared_ptr<Observation> state) : state_(std::move(state)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return state_->preferred; }
    [[nodiscard]] ui::Size minimum_size(const std::vector<ui::ChildMetrics>&) const override { return state_->minimum; }
    [[nodiscard]] std::optional<float> first_baseline(ui::Size) const override { return state_->baseline; }
    [[nodiscard]] ui::ChildMetrics measure_constrained(const ui::Constraints& constraints,const std::vector<ui::ChildMetrics>& children) const override {
        state_->measured_widths.push_back(constraints.max.w);
        if (state_->fail_narrow && constraints.bounded_width() && constraints.max.w < 300.0f) throw std::runtime_error("narrow measure fault");
        return Component::measure_constrained(constraints,children);
    }
    void mount(ui::MountContext& context) override { ++state_->mounts; state_->id=context.node_id(); }
    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo info; info.role=ui::SemanticRole::TextInput; info.name=state_->semantic_name;
        info.enabled=effective_enabled(); info.read_only=effective_read_only(); return info;
    }
    void unmount(ui::LifecycleContext&) override { ++state_->unmounts; }
    void focus_changed(bool focused,ui::FocusContext&) override { state_->focused = focused; }
    void paint(ui::PaintContext& context) const override { state_->bounds = context.bounds(); }
private:
    std::shared_ptr<Observation> state_;
};
class Control {
public:
    explicit Control(std::shared_ptr<Observation> state) : state_(std::move(state)) {}
    ui::Spec spec() && {
        const auto state = state_;
        return {[state] { return std::make_unique<ControlComponent>(state); },{}};
    }
private:
    std::shared_ptr<Observation> state_;
};
void render(ui::UI& tree,ui::Size size) {
    ui::HeadlessRenderer renderer{size,1.0f}; NUI_CHECK(renderer.render(tree));
}
ui::InputEvent command(ui::Command value) {
    ui::InputEvent event;
    event.type = ui::InputType::Command; event.command = value; return event;
}
void aligned_fields_cross_fieldset_but_nested_form_opens_new_scope() {
    auto outer = std::make_shared<Observation>();
    auto grouped = std::make_shared<Observation>();
    auto inner = std::make_shared<Observation>();
    ui::UI tree{ui::Form{
        ui::Field{"A substantially longer outer label",Control{outer}},
        ui::Fieldset{"Group",ui::Field{"Short",Control{grouped}},
            ui::Form{ui::Field{"X",Control{inner}}}}}};
    tree.resize({640.0f,300.0f}); render(tree,{640.0f,300.0f});
    NUI_CHECK_NEAR(outer->bounds.x,grouped->bounds.x,0.01f);
    NUI_CHECK(inner->bounds.x < grouped->bounds.x);
    NUI_CHECK(outer->bounds.w >= 30.0f && grouped->bounds.w >= 30.0f);
    NUI_CHECK(outer->mounts == 1 && grouped->mounts == 1 && inner->mounts == 1);
}
void responsive_reflows_without_remounting_or_changing_focused_control() {
    auto first = std::make_shared<Observation>();
    auto second = std::make_shared<Observation>();
    ui::UI tree{ui::Form{ui::Field{"Long responsive label",Control{first}},ui::Field{"X",Control{second}}}
        .layout(ui::FormLayout::Responsive).stacked_below(360.0)};
    test::MockPlatform platform;
    tree.resize({500.0f,240.0f}); tree.activate(platform); render(tree,{500.0f,240.0f});
    const auto aligned_x = first->bounds.x;
    tree.dispatch(test::key(ui::Key::Tab),platform);
    NUI_CHECK(second->focused);
    tree.resize({300.0f,300.0f}); render(tree,{300.0f,300.0f});
    NUI_CHECK(first->bounds.x < aligned_x);
    NUI_CHECK_NEAR(first->bounds.x,second->bounds.x,0.01f);
    NUI_CHECK(second->focused && first->mounts == 1 && second->mounts == 1);
    tree.resize({500.0f,240.0f}); render(tree,{500.0f,240.0f});
    NUI_CHECK_NEAR(first->bounds.x,aligned_x,0.01f);
    for (double threshold : {-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        bool threw = false;
        try { (void)ui::Form{}.stacked_below(threshold).spec(); }
        catch (const std::invalid_argument&) { threw = true; }
        NUI_CHECK(threw);
    }
}
void only_explicit_submit_command_submits_and_descendants_can_consume_cancel() {
    ui::State<std::string> text{""};
    int submits = 0, cancels = 0, outer_commands = 0;
    ui::UI tree{ui::CommandScope{[&](ui::Command) { ++outer_commands; return ui::EventResult::Handled; },
        ui::Form{ui::Field{"Text",ui::TextArea{"",text}}}
            .on_submit([&] { ++submits; }).on_cancel([&] { ++cancels; })}};
    test::MockPlatform platform;
    tree.resize({500.0f,240.0f}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Enter),platform);
    NUI_CHECK(submits == 0 && text.get() == "\n");
    tree.dispatch(command(ui::Command::Submit),platform);
    NUI_CHECK(submits == 1 && cancels == 0 && outer_commands == 0);
    tree.dispatch(command(ui::Command::Cancel),platform);
    NUI_CHECK(cancels == 1 && outer_commands == 0);
    int consumed = 0;
    ui::UI consumed_tree{ui::Form{ui::CommandScope{[&](ui::Command value) {
        if (value == ui::Command::Cancel) { ++consumed; return ui::EventResult::Handled; }
        return ui::EventResult::Ignored;
    },ui::Button{"Control",[] {}}}}.on_cancel([&] { ++cancels; })};
    test::MockPlatform consumed_platform;
    consumed_tree.resize({320.0f,160.0f}); consumed_tree.activate(consumed_platform);
    consumed_tree.dispatch(command(ui::Command::Cancel),consumed_platform);
    NUI_CHECK(consumed == 1 && cancels == 1);
    ui::UI unhandled{ui::CommandScope{[&](ui::Command) { ++outer_commands; return ui::EventResult::Handled; },
        ui::Form{ui::Button{"Control",[] {}}}}};
    test::MockPlatform unhandled_platform;
    unhandled.resize({320.0f,160.0f}); unhandled.activate(unhandled_platform);
    unhandled.dispatch(command(ui::Command::Submit),unhandled_platform);
    NUI_CHECK(outer_commands == 1);
}
void label_click_focuses_keyed_target_and_missing_key_has_no_fallback() {
    auto first = std::make_shared<Observation>();
    auto target = std::make_shared<Observation>();
    ui::UI tree{ui::Field{"Target",ui::Row{Control{first},ui::keyed("chosen",Control{target})}}.target("chosen")};
    test::MockPlatform platform;
    tree.resize({360.0f,180.0f}); tree.activate(platform);
    NUI_CHECK(first->focused && !target->focused);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,4.0f,4.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,4.0f,4.0f),platform);
    NUI_CHECK(!first->focused && target->focused);
    auto missing = std::make_shared<Observation>();
    auto outside = std::make_shared<Observation>();
    ui::UI absent{ui::Column{Control{outside},ui::Field{"Missing",Control{missing}}.target("missing")}.padding(0.0f).gap(0.0f)};
    test::MockPlatform absent_platform;
    absent.resize({360.0f,240.0f}); absent.activate(absent_platform); render(absent,{360.0f,240.0f});
    const auto field_label_y = outside->bounds.y+outside->bounds.h+4.0f;
    absent.dispatch(test::pointer(ui::InputType::PointerDown,4.0f,field_label_y),absent_platform);
    absent.dispatch(test::pointer(ui::InputType::PointerUp,4.0f,field_label_y),absent_platform);
    NUI_CHECK(outside->focused && !missing->focused);
}
void fieldset_availability_blocks_writes_without_erasing_application_values() {
    ui::State<bool> allowed{true},locked{false},checked{false};
    ui::UI tree{ui::Fieldset{"Preferences",ui::Field{"Option",ui::Checkbox{checked,""}}}
        .enabled(allowed).read_only(locked)};
    test::MockPlatform platform;
    tree.resize({360.0f,180.0f}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Space),platform);
    auto up = test::key(ui::Key::Space); up.type = ui::InputType::KeyUp; tree.dispatch(up,platform);
    NUI_CHECK(checked.get());
    locked.set(true); tree.dispatch(test::key(ui::Key::Space),platform); tree.dispatch(up,platform);
    NUI_CHECK(checked.get());
    locked.set(false); allowed.set(false); tree.dispatch(test::key(ui::Key::Space),platform); tree.dispatch(up,platform);
    NUI_CHECK(checked.get());
    allowed.set(true); tree.dispatch(test::key(ui::Key::Space),platform); tree.dispatch(up,platform);
    NUI_CHECK(!checked.get());
}

void field_baseline_and_fallback_match_measured_font_geometry() {
    ui::TextStyle text; text.size=32.0f;
    ui::FieldStyle style; style.label_text=text;
    const auto font=ui::TextService::measure("Mg",text);
    const auto baseline=font.height*0.5f-(font.ascent+font.descent)*0.5f;
    auto aligned=std::make_shared<Observation>();
    ui::UI aligned_tree{ui::Form{ui::Field{"Label",Control{aligned}}.style(style)}};
    render(aligned_tree,{500.0f,120.0f});
    NUI_CHECK_NEAR(aligned->bounds.y,std::max(0.0f,baseline-18.0f),0.01f);
    auto fallback=std::make_shared<Observation>(); fallback->baseline.reset();
    ui::UI fallback_tree{ui::Form{ui::Field{"Label",Control{fallback}}.style(style)}};
    render(fallback_tree,{500.0f,120.0f});
    NUI_CHECK_NEAR(fallback->bounds.y,std::max(0.0f,(font.height-fallback->preferred.h)*0.5f),0.01f);
}
void second_measurement_pass_preserves_controls_minima_and_wraps_labels() {
    auto first=std::make_shared<Observation>(),second=std::make_shared<Observation>();
    first->minimum.w=80.0f; second->minimum.w=80.0f;
    ui::UI tree{ui::Form{ui::Field{"A very long label that must wrap to preserve control minimum",Control{first}},
        ui::Field{"X",Control{second}}}};
    render(tree,{140.0f,600.0f});
    NUI_CHECK_NEAR(first->bounds.x,second->bounds.x,0.01f);
    NUI_CHECK(first->bounds.w>=80.0f && second->bounds.w>=80.0f);
    NUI_CHECK(second->bounds.y>first->bounds.y+first->preferred.h+8.0f);
    NUI_CHECK(std::find(first->measured_widths.begin(),first->measured_widths.end(),ui::kUnboundedExtent)!=first->measured_widths.end());
    NUI_CHECK(std::any_of(first->measured_widths.begin(),first->measured_widths.end(),[](float width) { return std::isfinite(width) && width<=80.01f; }));
    first->fail_narrow=true; const auto previous=first->bounds; bool threw=false;
    try { tree.resize({130.0f,600.0f}); } catch (const std::runtime_error&) { threw=true; }
    NUI_CHECK(threw && first->bounds.x==previous.x && first->bounds.y==previous.y);
    first->fail_narrow=false; render(tree,{130.0f,600.0f});
    NUI_CHECK(first->bounds.w>=80.0f && first->mounts==1 && second->mounts==1);
}
void field_help_error_expiration_and_semantic_names_recover_after_first_observer_failure() {
    auto help=std::make_unique<ui::State<std::string>>("Initial help");
    auto error=std::make_unique<ui::State<std::string>>("");
    bool fail=false;
    const auto observer=error->observe([&](const std::string&) { if (fail) throw std::runtime_error("first observer"); });
    auto control=std::make_shared<Observation>(); control->semantic_name="Existing explicit name";
    ui::UI tree{ui::Field{"Decorated",Control{control}}.description(help->binding()).error(error->binding()).required()};
    render(tree,{240.0f,360.0f});
    auto info=tree.component_semantics(control->id);
    NUI_CHECK(info && info->name=="Existing explicit name" && info->description.find("Initial help")!=std::string::npos);
    const auto old_height=tree.measure(ui::Constraints{{},{240.0f,ui::kUnboundedExtent}}).preferred.h;
    fail=true; bool threw=false;
    try { error->set("A sufficiently long error description to wrap across multiple lines in this field"); }
    catch (const std::runtime_error&) { threw=true; }
    fail=false; NUI_CHECK(threw); render(tree,{240.0f,360.0f});
    info=tree.component_semantics(control->id);
    NUI_CHECK(info && info->description.find("long error")!=std::string::npos);
    const auto with_error=tree.measure(ui::Constraints{{},{240.0f,ui::kUnboundedExtent}}).preferred.h;
    NUI_CHECK(with_error>old_height);
    help.reset(); error.reset();
    const auto after_expiry=tree.measure(ui::Constraints{{},{240.0f,ui::kUnboundedExtent}}).preferred.h;
    NUI_CHECK(after_expiry<with_error);
    render(tree,{240.0f,360.0f}); info=tree.component_semantics(control->id);
    NUI_CHECK(info && info->description.find("Initial help")==std::string::npos && info->description.find("long error")==std::string::npos);
    NUI_CHECK(info->description.find("Required field")!=std::string::npos);
}
void removing_field_participant_recomputes_shared_column_without_remounting_neighbor() {
    ui::State<bool> present{true}; auto longest=std::make_shared<Observation>(),remaining=std::make_shared<Observation>();
    ui::UI tree{ui::Form{ui::If{present,ui::Field{"A very long participant label",Control{longest}}},ui::Field{"X",Control{remaining}}}};
    render(tree,{500.0f,220.0f}); const auto before=remaining->bounds.x;
    present.set(false); render(tree,{500.0f,220.0f});
    NUI_CHECK(remaining->bounds.x<before && remaining->mounts==1 && longest->unmounts==1);
    present.set(true); render(tree,{500.0f,220.0f});
    NUI_CHECK_NEAR(remaining->bounds.x,before,0.01f);
    NUI_CHECK(longest->mounts==2 && remaining->mounts==1);
}
void escape_cancels_once_and_label_checkbox_action_respects_read_only() {
    int canceled=0;
    ui::UI cancel{ui::Form{ui::Button{"Control",[] {}}}.on_cancel([&] { ++canceled; })};
    test::MockPlatform cancel_platform; cancel.resize({300.0f,140.0f}); cancel.activate(cancel_platform);
    cancel.dispatch(test::key(ui::Key::Escape),cancel_platform); NUI_CHECK(canceled==1);
    ui::State<bool> value{false},locked{false};
    ui::UI tree{ui::ReadOnly{locked,ui::Field{"Toggle option",ui::Checkbox{value,""}}}};
    test::MockPlatform platform; tree.resize({300.0f,180.0f}); tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,4.0f,4.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,4.0f,4.0f),platform);
    NUI_CHECK(value.get()); locked.set(true);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,4.0f,4.0f),platform);
    tree.dispatch(test::pointer(ui::InputType::PointerUp,4.0f,4.0f),platform);
    NUI_CHECK(value.get() && platform.pointer_capture_begin_count==platform.pointer_capture_end_count);
}


void fieldset_first_observer_failure_is_reconciled_before_the_next_input() {
    ui::State<bool> allowed{true},checked{false}; bool fail=false;
    const auto observer=allowed.observe([&](bool) { if (fail) throw std::runtime_error("first availability observer"); });
    ui::UI tree{ui::Fieldset{"Group",ui::Field{"Option",ui::Checkbox{checked,""}}}.enabled(allowed)};
    test::MockPlatform platform; tree.resize({300.0f,200.0f}); tree.activate(platform);
    fail=true; bool threw=false;
    try { allowed.set(false); } catch (const std::runtime_error&) { threw=true; }
    fail=false; NUI_CHECK(threw);
    tree.dispatch(test::key(ui::Key::Space),platform);
    auto up=test::key(ui::Key::Space); up.type=ui::InputType::KeyUp; tree.dispatch(up,platform);
    NUI_CHECK(!checked.get()); allowed.set(true); tree.refresh_focus(platform);
    tree.dispatch(test::key(ui::Key::Space),platform); tree.dispatch(up,platform);
    NUI_CHECK(checked.get());
}
void target_removed_and_release_invalidation_cannot_activate_retired_field() {
    ui::State<bool> target_present{true},checked{false};
    ui::UI tree{ui::Field{"Option",ui::If{target_present,ui::keyed("target",ui::Checkbox{checked,""})}}.target("target")};
    test::MockPlatform platform; tree.resize({300.0f,200.0f}); tree.activate(platform);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,4.0f,4.0f),platform);
    target_present.set(false); tree.resize({300.0f,200.0f});
    tree.dispatch(test::pointer(ui::InputType::PointerUp,4.0f,4.0f),platform);
    NUI_CHECK(!checked.get());
    ui::State<bool> field_present{true};
    ui::UI removable{ui::If{field_present,ui::Field{"Option",ui::Checkbox{checked,""}}}};
    test::MockPlatform removable_platform; removable.resize({300.0f,200.0f}); removable.activate(removable_platform);
    removable.dispatch(test::pointer(ui::InputType::PointerDown,4.0f,4.0f),removable_platform);
    render(removable,{300.0f,200.0f});
    bool remove=false;
    removable.set_invalidation_callback([&](ui::Rect) { if (remove) { remove=false; field_present.set(false); removable.resize({300.0f,200.0f}); } });
    remove=true; removable.dispatch(test::pointer(ui::InputType::PointerUp,4.0f,4.0f),removable_platform);
    NUI_CHECK(!checked.get() && !field_present.get());
    NUI_CHECK(removable_platform.pointer_capture_begin_count==removable_platform.pointer_capture_end_count);
}
void empty_labels_do_not_create_a_text_column_and_copied_forms_keep_theme_contexts_local() {
    auto empty=std::make_shared<Observation>();
    ui::UI empty_tree{ui::Form{ui::Field{"",Control{empty}}}}; render(empty_tree,{400.0f,180.0f});
    NUI_CHECK_NEAR(empty->bounds.x,0.0f,0.01f);
    auto plain=std::make_shared<Observation>();
    auto recipe=ui::make_spec(ui::Form{ui::Field{"Shared recipe",Control{plain}}});
    auto normal=ui::default_theme(),large=normal; large.typography.label_size=32.0f;
    ui::UI first{ui::Spec{recipe},normal}; render(first,{500.0f,200.0f}); const auto first_column=plain->bounds.x;
    ui::UI second{ui::Spec{recipe},large}; render(second,{500.0f,200.0f});
    NUI_CHECK(plain->bounds.x>first_column);
    (void)first.measure(ui::Constraints{{},{500.0f,ui::kUnboundedExtent}});
    render(first,{500.0f,200.0f});
    NUI_CHECK_NEAR(plain->bounds.x,first_column,0.01f);
    NUI_CHECK(plain->mounts==2);
}

void suite() {
    aligned_fields_cross_fieldset_but_nested_form_opens_new_scope();
    responsive_reflows_without_remounting_or_changing_focused_control();
    only_explicit_submit_command_submits_and_descendants_can_consume_cancel();
    label_click_focuses_keyed_target_and_missing_key_has_no_fallback();
    fieldset_availability_blocks_writes_without_erasing_application_values();
    field_baseline_and_fallback_match_measured_font_geometry();
    second_measurement_pass_preserves_controls_minima_and_wraps_labels();
    field_help_error_expiration_and_semantic_names_recover_after_first_observer_failure();
    removing_field_participant_recomputes_shared_column_without_remounting_neighbor();
    escape_cancels_once_and_label_checkbox_action_respects_read_only();
    fieldset_first_observer_failure_is_reconciled_before_the_next_input();
    target_removed_and_release_invalidation_cannot_activate_retired_field();
    empty_labels_do_not_create_a_text_column_and_copied_forms_keep_theme_contexts_local();
}
}
int main() { return test::run("widget_form_fields",&suite); }
