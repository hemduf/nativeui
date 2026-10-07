#include "test_support.hpp"
#include <nativeui/detail/theme_binding.hpp>

#include <memory>
#include <vector>

namespace {
struct Observation { ui::Rect bounds{}; ui::Color color{}; std::string family; };
class ControlComponent final : public ui::Component,public ui::detail::ThemeBinding {
public:
    explicit ControlComponent(std::shared_ptr<Observation> value) : state_(std::move(value)) {}
    [[nodiscard]] bool focusable() const noexcept override { return true; }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {30.0f,28.0f}; }
    [[nodiscard]] std::optional<float> first_baseline(ui::Size) const override { return 18.0f; }
    void paint(ui::PaintContext& context) const override {
        state_->bounds = context.bounds(); state_->color = current_theme().palette.surface; state_->family = current_theme().typography.family;
        context.painter().fill_rounded_rect(context.bounds(),0.0f,state_->color);
    }
private:
    std::shared_ptr<Observation> state_;
};
class Control {
public:
    explicit Control(std::shared_ptr<Observation> value) : state_(std::move(value)) {}
    ui::Spec spec() && { const auto state = state_; return {[state] { return std::make_unique<ControlComponent>(state); },{}}; }
private:
    std::shared_ptr<Observation> state_;
};
class BaselineHost final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {400.0f,100.0f}; }
    void layout_children(ui::Rect bounds,const std::vector<ui::ChildMetrics>& children,
                         std::vector<ui::ChildPlacement>& placements) const override {
        for (std::size_t i = 0; i < placements.size(); ++i) {
            const auto baseline = children[i].first_baseline;
            placements[i].bounds = {bounds.x+45.0f*static_cast<float>(i),bounds.y+(baseline ? 40.0f-*baseline : 0.0f),
                children[i].preferred.w,children[i].preferred.h};
        }
    }
    void paint(ui::PaintContext&) const override {}
};
void wrappers_preserve_child_baseline_and_padding_offsets_it() {
    ui::State<bool> enabled{true},read_only{false},shown{true},scope_active{false};
    std::vector<std::shared_ptr<Observation>> records;
    for (int i = 0; i < 8; ++i) records.push_back(std::make_shared<Observation>());
    ui::Spec recipe{[] { return std::make_unique<BaselineHost>(); },{
        ui::make_spec(ui::Enabled{enabled,Control{records[0]}}),
        ui::make_spec(ui::ReadOnly{read_only,Control{records[1]}}),
        ui::make_spec(ui::Visibility{shown,Control{records[2]}}),
        ui::make_spec(ui::Flex{Control{records[3]}}),
        ui::make_spec(ui::FocusScope{scope_active,Control{records[4]}}),
        ui::make_spec(ui::CommandScope{[](ui::Command) { return ui::EventResult::Ignored; },Control{records[5]}}),
        ui::make_spec(ui::StyleScope{ui::StyleScopeOverrides{},Control{records[6]}}),
        ui::make_spec(ui::Padding{5.0f,Control{records[7]}})}};
    ui::UI tree{std::move(recipe)}; tree.resize({400.0f,100.0f});
    ui::HeadlessRenderer renderer{{400.0f,100.0f},1.0f}; NUI_CHECK(renderer.render(tree));
    for (const auto& record : records) NUI_CHECK_NEAR(record->bounds.y+18.0f,40.0f,0.001f);
    NUI_CHECK_NEAR(records.back()->bounds.x,320.0f,0.001f);
}
void copied_command_and_style_scopes_keep_complete_per_instance_recipes() {
    auto first_control = std::make_shared<Observation>();
    std::vector<int> calls;
    auto command_recipe = ui::CommandScope{[count=0,&calls](ui::Command) mutable {
        calls.push_back(++count); return ui::EventResult::Handled;
    },Control{first_control}}.spec();
    ui::UI first{command_recipe},second{command_recipe}; test::MockPlatform a,b;
    first.resize({100.0f,100.0f}); second.resize({100.0f,100.0f}); first.activate(a); second.activate(b);
    ui::InputEvent command; command.type = ui::InputType::Command; command.command = ui::Command::Copy;
    first.dispatch(command,a); second.dispatch(command,b); first.dispatch(command,a); second.dispatch(command,b);
    NUI_CHECK(calls == std::vector<int>({1,1,2,2}));
    auto color = std::make_shared<Observation>();
    ui::StyleScopeOverrides overrides; overrides.palette.surface = ui::Color{1.0f,0.0f,0.0f,1.0f};
    overrides.typography.family = "Per-instance scope family";
    auto style_recipe = ui::StyleScope{overrides,Control{color}}.spec();
    ui::UI style_first{style_recipe},style_second{style_recipe};
    ui::HeadlessRenderer renderer{{100.0f,100.0f},1.0f};
    NUI_CHECK(renderer.render(style_first)); NUI_CHECK(color->color.r == 1.0f && color->color.g == 0.0f && color->family == "Per-instance scope family");
    NUI_CHECK(renderer.render(style_second)); NUI_CHECK(color->color.r == 1.0f && color->color.g == 0.0f && color->family == "Per-instance scope family");
}
void suite() { wrappers_preserve_child_baseline_and_padding_offsets_it(); copied_command_and_style_scopes_keep_complete_per_instance_recipes(); }
}
int main(int argc,char** argv) {
    if (argc == 2 && std::string_view{argv[1]} == "copied") return test::run("wrapper_copied_scopes",&copied_command_and_style_scopes_keep_complete_per_instance_recipes);
    return test::run("widget_wrapper_baseline",&suite);
}
