#include "test_support.hpp"
#include <nativeui/list_view.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {
struct PaintLog { std::vector<int> keys; std::size_t factories{}; };
class RowPaint final : public ui::Component {
public:
    RowPaint(int key,std::shared_ptr<PaintLog> log):key_(key),log_(std::move(log)) {}
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {80.0f,20.0f}; }
    void paint(ui::PaintContext&) const override { log_->keys.push_back(key_); }
private:
    int key_{};
    std::shared_ptr<PaintLog> log_;
};
ui::Spec row(int key,const std::shared_ptr<PaintLog>& log) {
    return {[key,log] { return std::make_unique<RowPaint>(key,log); },{}};
}
void paint(ui::UI& tree,ui::Size size,const std::shared_ptr<PaintLog>& log) {
    log->keys.clear(); ui::HeadlessRenderer renderer{size,1.0f}; NUI_CHECK(renderer.render(tree));
}
bool has(const std::shared_ptr<PaintLog>& log,int key) { return std::find(log->keys.begin(),log->keys.end(),key)!=log->keys.end(); }
std::vector<ui::VirtualListState<int>::Item> dataset(int first=0) {
    std::vector<ui::VirtualListState<int>::Item> result;
    for (int index=0;index<200;++index) result.emplace_back(first+index,"row "+std::to_string(first+index));
    return result;
}
void copied_specs_recycle_each_view_and_removal_keeps_the_other_subscription() {
    const auto log=std::make_shared<PaintLog>(); ui::State<std::optional<int>> selected{std::nullopt};
    ui::VirtualListState<int> controller{selected,20.0f,[log](const auto& item) { ++log->factories; return row(item.key,log); }};
    NUI_CHECK(controller.replace(dataset()));
    auto recipe=ui::make_spec(ui::ListView{controller});
    ui::UI first{ui::Spec{recipe}};
    auto second=std::make_unique<ui::UI>(ui::Spec{recipe});
    first.resize({80.0f,100.0f}); second->resize({80.0f,100.0f});
    paint(first,{80.0f,100.0f},log); NUI_CHECK(has(log,0));
    paint(*second,{80.0f,100.0f},log); NUI_CHECK(has(log,0));
    NUI_CHECK(controller.scroll_to_index(50,ui::ScrollAlignment::Start));
    paint(first,{80.0f,100.0f},log); NUI_CHECK(has(log,50) && !has(log,0));
    paint(*second,{80.0f,100.0f},log); NUI_CHECK(has(log,50) && !has(log,0));
    second.reset();
    NUI_CHECK(controller.scroll_to_index(80,ui::ScrollAlignment::Start));
    paint(first,{80.0f,100.0f},log); NUI_CHECK(has(log,80) && !has(log,50));
    NUI_CHECK(controller.replace(dataset(1000)));
    paint(first,{80.0f,100.0f},log); NUI_CHECK(has(log,1080) && !has(log,80));
}
void hover_and_pressed_state_remain_local_to_each_compilation() {
    ui::State<std::optional<int>> selected{std::nullopt};
    ui::VirtualListState<int> controller{selected,20.0f,[](const auto&) { return ui::Spacer{80.0f,20.0f}; }};
    NUI_CHECK(controller.replace(dataset()));
    ui::ListViewStyle style;
    style.base.row_fill=ui::Color{1,0,0,1}; style.hovered.row_fill=ui::Color{0,1,0,1}; style.pressed.row_fill=ui::Color{0,0,1,1};
    style.base.row_horizontal_inset=0.0f; style.base.row_vertical_inset=0.0f; style.base.row_corner_radius=0.0f;
    auto recipe=ui::make_spec(ui::ListView{controller}.style(style));
    ui::UI first{ui::Spec{recipe}},second{ui::Spec{recipe}};
    test::MockPlatform first_platform,second_platform;
    first.resize({80.0f,100.0f}); second.resize({80.0f,100.0f}); first.activate(first_platform); second.activate(second_platform);
    ui::HeadlessRenderer first_renderer{{80.0f,100.0f},1},second_renderer{{80.0f,100.0f},1};
    first.dispatch(test::pointer(ui::InputType::PointerMove,10.0f,10.0f),first_platform);
    NUI_CHECK(first_renderer.render(first)); NUI_CHECK(second_renderer.render(second));
    NUI_CHECK(first_renderer.pixel(10,10).g==255 && second_renderer.pixel(10,10).r==255);
    first.dispatch(test::pointer(ui::InputType::PointerDown,10.0f,10.0f),first_platform);
    NUI_CHECK(first_renderer.render(first)); NUI_CHECK(second_renderer.render(second));
    NUI_CHECK(first_renderer.pixel(10,10).b==255 && second_renderer.pixel(10,10).r==255);
    second.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,10.0f),second_platform); NUI_CHECK(!selected.get());
    first.dispatch(test::pointer(ui::InputType::PointerUp,10.0f,10.0f),first_platform); NUI_CHECK(selected.get()==std::optional<int>{0});
    NUI_CHECK(first_platform.pointer_capture_begin_count==first_platform.pointer_capture_end_count);
}
void different_viewports_do_not_share_materialization_windows() {
    const auto log=std::make_shared<PaintLog>(); ui::State<std::optional<int>> selected{std::nullopt};
    ui::VirtualListState<int> controller{selected,20.0f,[log](const auto& item) { ++log->factories; return row(item.key,log); },0};
    NUI_CHECK(controller.replace(dataset())); auto recipe=ui::make_spec(ui::ListView{controller});
    ui::UI small{ui::Spec{recipe}},large{ui::Spec{recipe}};
    small.resize({80.0f,60.0f}); large.resize({80.0f,180.0f});
    paint(small,{80.0f,60.0f},log); NUI_CHECK(log->keys.size()==3 && has(log,2) && !has(log,8));
    paint(large,{80.0f,180.0f},log); NUI_CHECK(log->keys.size()==9 && has(log,8));
    const auto factories=log->factories;
    small.resize({81.0f,60.0f});
    paint(small,{81.0f,60.0f},log); paint(large,{80.0f,180.0f},log);
    NUI_CHECK(log->factories==factories);
}
void first_attached_view_owns_public_metrics_until_its_removal() {
    const auto log=std::make_shared<PaintLog>(); ui::State<std::optional<int>> selected{std::nullopt};
    ui::VirtualListState<int> controller{selected,20.0f,[log](const auto& item) { ++log->factories; return row(item.key,log); },0};
    NUI_CHECK(controller.replace(dataset())); auto recipe=ui::make_spec(ui::ListView{controller});
    auto small=std::make_unique<ui::UI>(ui::Spec{recipe}); ui::UI large{ui::Spec{recipe}};
    small->resize({80.0f,60.0f}); large.resize({80.0f,180.0f});
    paint(*small,{80.0f,60.0f},log); paint(large,{80.0f,180.0f},log);
    NUI_CHECK(controller.viewport_size().h==60.0f && controller.content_size().h==4000.0f);
    NUI_CHECK(controller.scroll_to_index(195,ui::ScrollAlignment::Start));
    NUI_CHECK(controller.offset().y==3900.0f);
    for (int pass=0;pass<3;++pass) {
        paint(*small,{80.0f,60.0f},log); NUI_CHECK(has(log,195));
        paint(large,{80.0f,180.0f},log); NUI_CHECK(has(log,191));
        NUI_CHECK(controller.offset().y==3900.0f && controller.viewport_size().h==60.0f);
    }
    small.reset();
    // Transfer happens at the surviving view's next retained checkpoint.
    paint(large,{80.0f,180.0f},log);
    NUI_CHECK(controller.viewport_size().h==180.0f && controller.offset().y==3820.0f && has(log,191));
    ui::UI later{ui::Spec{recipe}}; later.resize({80.0f,60.0f});
    paint(later,{80.0f,60.0f},log); paint(large,{80.0f,180.0f},log);
    NUI_CHECK(controller.viewport_size().h==180.0f && controller.offset().y==3820.0f);
    NUI_CHECK(controller.scroll_to_index(190,ui::ScrollAlignment::Start));
    paint(later,{80.0f,60.0f},log); NUI_CHECK(has(log,190));
    paint(large,{80.0f,180.0f},log); NUI_CHECK(has(log,190));
}
void suite() { copied_specs_recycle_each_view_and_removal_keeps_the_other_subscription(); hover_and_pressed_state_remain_local_to_each_compilation(); different_viewports_do_not_share_materialization_windows(); first_attached_view_owns_public_metrics_until_its_removal(); }
}
int main(int argc,char** argv) {
    if (argc>1 && std::string_view{argv[1]}=="recycle") return test::run("virtual_recycle",&copied_specs_recycle_each_view_and_removal_keeps_the_other_subscription);
    if (argc>1 && std::string_view{argv[1]}=="presentation") return test::run("virtual_presentation",&hover_and_pressed_state_remain_local_to_each_compilation);
    if (argc>1 && std::string_view{argv[1]}=="viewport") return test::run("virtual_viewport",&different_viewports_do_not_share_materialization_windows);
    if (argc>1 && std::string_view{argv[1]}=="metrics") return test::run("virtual_metrics_authority",&first_attached_view_owns_public_metrics_until_its_removal);
    return test::run("virtual_isolation",&suite);
}
