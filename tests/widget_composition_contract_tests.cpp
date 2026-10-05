#include "test_support.hpp"

#include <nativeui/detail/focus_group.hpp>
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"
#include <array>
#include <memory>
#include <functional>
#include <stdexcept>

namespace {
struct Control {
    int mounts{}, destructions{}, hits{}, focus{}, drops{}, commits{}, key_events{};
    bool focused{};
    ui::NodeId id{};
    ui::Rect committed{};
    ui::Color color{};
    std::function<void()> request;
};
class Leaf final : public ui::Component {
public:
    explicit Leaf(std::shared_ptr<Control> c) : c_(std::move(c)) {}
    ~Leaf() override { ++c_->destructions; }
    bool focusable() const noexcept override { return true; }
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40, 20}; }
    void mount(ui::MountContext& c) override { ++c_->mounts; c_->id = c.node_id(); c_->request = c.focus_requester(); }
    void focus_changed(bool focus, ui::FocusContext&) override { c_->focused = focus; if (focus) ++c_->focus; }
    ui::EventResult input(const ui::InputEvent& e, ui::InputContext&) override {
        if (e.type == ui::InputType::PointerDown) { ++c_->hits; return ui::EventResult::Handled; }
        if (e.type == ui::InputType::DropOffer) { ++c_->drops; return ui::EventResult::Handled; }
        if (e.type == ui::InputType::KeyDown) ++c_->key_events;
        return ui::EventResult::Ignored;
    }
    void paint(ui::PaintContext& context) const override { context.painter().fill_rounded_rect(context.bounds(), 0, c_->color); }
private:
    void layout_committed(ui::Rect, ui::Rect current) noexcept override { ++c_->commits; c_->committed = current; }
public:
private:
    std::shared_ptr<Control> c_;
};
ui::Spec leaf(std::shared_ptr<Control> c) { return {[c] { return std::make_unique<Leaf>(c); }, {}}; }
class Root final : public ui::Component {
public:
    std::shared_ptr<Control> control{std::make_shared<Control>()};
    ui::Size measure(const std::vector<ui::ChildMetrics>& c) const override { return c.empty() ? ui::Size{} : c[0].preferred; }
    void layout_children(ui::Rect b, const std::vector<ui::ChildMetrics>&, std::vector<ui::ChildPlacement>& c) const override {
        for (auto& child : c) child.bounds = b;
    }
    void paint(ui::PaintContext&) const override {}
};
void children_are_created_per_compilation_and_fail_before_mount() {
    std::vector<std::shared_ptr<Control>> controls;
    bool fault{};
    ui::Spec spec{[] { return std::make_unique<Root>(); }, {}};
    spec.children_factory = [&](ui::Component& c) {
        auto control = static_cast<Root&>(c).control;
        controls.push_back(control);
        std::vector<ui::Spec> children{leaf(control)};
        if (fault) throw std::runtime_error("children fault");
        return children;
    };
    auto first = std::make_unique<ui::UI>(ui::Spec{spec});
    ui::UI second{ui::Spec{spec}};
    NUI_CHECK(controls.size() == 2 && controls[0] != controls[1]);
    NUI_CHECK(controls[0]->mounts == 1 && controls[1]->mounts == 1);
    test::MockPlatform platform;
    second.resize({40, 20}); second.activate(platform);
    second.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
    NUI_CHECK(controls[0]->hits == 0 && controls[1]->hits == 1);
    fault = true;
    bool caught{};
    try { ui::UI rejected{ui::Spec{spec}}; } catch (const std::runtime_error& e) { caught = std::string_view{e.what()} == "children fault"; }
    NUI_CHECK(caught && controls[2]->mounts == 0);
    fault = false;
    ui::UI recovered{ui::Spec{spec}};
    NUI_CHECK(controls[3]->mounts == 1);
    first.reset();
    second.dispatch(test::pointer(ui::InputType::PointerDown, 10, 10), platform);
    NUI_CHECK(controls[1]->hits == 2);
    second.deactivate(platform);
}
void retained_focus_is_deferred_and_inert_after_destruction() {
    auto a = std::make_shared<Control>();
    auto b = std::make_shared<Control>();
    auto tree = std::make_unique<ui::UI>(ui::Row{leaf(a), leaf(b)});
    test::MockPlatform platform;
    tree->resize({80, 20}); tree->activate(platform);
    NUI_CHECK(a->focus == 1 && b->focus == 0);
    NUI_CHECK(static_cast<bool>(b->request));
    b->request();
    NUI_CHECK(b->focus == 0);
    tree->refresh_focus(platform);
    NUI_CHECK(b->focus >= 1);
    auto stale = b->request;
    tree.reset();
    stale();
    NUI_CHECK(a->destructions == 1 && b->destructions == 1);
}

class Overlap final : public ui::Component {
public:
    explicit Overlap(std::shared_ptr<bool> fault, bool decoration = false)
        : fault_(std::move(fault)), decoration_(decoration) {}
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40,20}; }
    std::optional<std::size_t> foreground_child_index() const noexcept override { return 1; }
    bool allows_child_interaction() const noexcept override { return !decoration_; }
    void layout_children(ui::Rect bounds, const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& children) const override {
        for (auto& child : children) child.bounds = bounds;
        if (*fault_) throw std::runtime_error("layout fault");
    }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<bool> fault_;
    bool decoration_{};
};
ui::Spec overlap(std::shared_ptr<bool> fault, std::vector<ui::Spec> children,
                 bool decoration = false) {
    return {[fault,decoration] { return std::make_unique<Overlap>(fault,decoration); },std::move(children)};
}
void foreground_matches_full_partial_hit_drop_and_logical_focus() {
    auto a = std::make_shared<Control>(), b = std::make_shared<Control>(), c = std::make_shared<Control>();
    a->color = {1,0,0,1}; b->color = {0,1,0,1}; c->color = {0,0,1,1};
    auto fault = std::make_shared<bool>(false);
    auto spec = overlap(fault,{leaf(a),leaf(b),leaf(c)});
    ui::Tree tree{ui::compile(std::move(spec))};
    test::MockPlatform platform;
    tree.mount(); tree.layout({40,20}); tree.activate_focus(platform);
    NUI_CHECK(a->focus == 1 && b->focus == 0 && c->focus == 0);
    tree.dispatch(test::key(ui::Key::Tab),platform);
    NUI_CHECK(b->focus == 1 && c->focus == 0);
    tree.dispatch(test::key(ui::Key::Tab),platform);
    NUI_CHECK(c->focus == 1);
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10,10),platform);
    NUI_CHECK(b->hits == 1 && a->hits == 0 && c->hits == 0);
    auto drop = test::pointer(ui::InputType::DropOffer,10,10);
    tree.dispatch(drop,platform);
    NUI_CHECK(b->drops == 1 && a->drops == 0 && c->drops == 0);
    auto info = SkImageInfo::MakeN32Premul(40,20);
    auto full = SkSurfaces::Raster(info), partial = SkSurfaces::Raster(info);
    NUI_CHECK(full && partial);
    full->getCanvas()->clear(SK_ColorTRANSPARENT);
    partial->getCanvas()->clear(SK_ColorTRANSPARENT);
    tree.paint(*full->getCanvas(),platform);
    { ui::Painter painter{*partial->getCanvas()}; tree.paint_region(painter,platform,{5,5,15,10}); }
    std::array<std::uint32_t,800> full_pixels{}, partial_pixels{};
    NUI_CHECK(full->readPixels(info,full_pixels.data(),40*sizeof(std::uint32_t),0,0));
    NUI_CHECK(partial->readPixels(info,partial_pixels.data(),40*sizeof(std::uint32_t),0,0));
    for (int y=5;y<15;++y) for (int x=5;x<20;++x) {
        const auto index = static_cast<std::size_t>(y*40+x);
        NUI_CHECK(full_pixels[index] == partial_pixels[index]);
        NUI_CHECK(full_pixels[index] == SK_ColorGREEN);
    }
    tree.deactivate_focus(platform);
}
void layout_notifications_publish_only_successful_geometry() {
    auto fault = std::make_shared<bool>(false);
    auto control = std::make_shared<Control>();
    ui::UI tree{overlap(fault,{leaf(control)})};
    (void)tree.measure(); NUI_CHECK(control->commits == 0);
    tree.resize({40,20}); NUI_CHECK(control->commits == 1 && control->committed.w == 40);
    *fault = true;
    bool caught{};
    try { tree.resize({60,30}); } catch (const std::runtime_error&) { caught = true; }
    NUI_CHECK(caught && control->commits == 1 && control->committed.w == 40);
    *fault = false; tree.resize({60,30});
    NUI_CHECK(control->commits == 2 && control->committed.w == 60);
}
void child_factory_unwinds_already_created_children_before_mount() {
    auto control = std::make_shared<Control>();
    auto fault = std::make_shared<bool>(false);
    bool fail = true;
    ui::Spec bad{[&] () -> std::unique_ptr<ui::Component> {
        if (fail) throw std::runtime_error("factory fault");
        return std::make_unique<Root>();
    },{}};
    auto spec = overlap(fault,{leaf(control),bad});
    bool caught{};
    try { ui::UI rejected{ui::Spec{spec}}; } catch (const std::runtime_error&) { caught = true; }
    NUI_CHECK(caught && control->mounts == 0 && control->destructions == 1);
    fail = false; ui::UI recovered{ui::Spec{spec}};
    NUI_CHECK(control->mounts == 1);
}
void interactive_policy_rejects_future_descendants_and_recovers() {
    auto present = ui::State<bool>{false};
    auto control = std::make_shared<Control>();
    auto fault = std::make_shared<bool>(false);
    ui::UI tree{overlap(fault,{ui::make_spec(ui::If{present,leaf(control)})},true)};
    (void)tree.measure(); present.set(true);
    bool caught{};
    try { (void)tree.measure(); } catch (const std::invalid_argument&) { caught = true; }
    NUI_CHECK(caught && control->mounts == 0);
    present.set(false); (void)tree.measure();
    NUI_CHECK(tree.structural_diagnostic().empty());
    ui::UI independent{overlap(fault,{ui::make_spec(ui::Spacer{10,20})},true)};
    NUI_CHECK(independent.measure().preferred.w == 40);
}
void queued_focus_revalidates_availability_identity_and_traps() {
    auto a = std::make_shared<Control>(), b = std::make_shared<Control>();
    ui::State<bool> present{true};
    ui::State<ui::VisibilityMode> visible{ui::VisibilityMode::Visible};
    test::MockPlatform platform;
    ui::UI tree{ui::Row{leaf(a),ui::Visibility{visible,ui::If{present,leaf(b)}}}};
    tree.resize({80,20}); tree.activate(platform);
    auto stale = b->request;
    stale(); visible.set(ui::VisibilityMode::Hidden); tree.refresh_focus(platform);
    NUI_CHECK(b->focus == 0);
    visible.set(ui::VisibilityMode::Visible); present.set(false); (void)tree.measure();
    present.set(true); tree.resize({81,21}); stale(); tree.refresh_focus(platform);
    NUI_CHECK(b->focus == 0);
    b->request(); tree.refresh_focus(platform); NUI_CHECK(b->focused && !a->focused);
    tree.deactivate(platform);
    auto outside = std::make_shared<Control>(), inside = std::make_shared<Control>();
    ui::State<bool> trapped{true};
    ui::UI modal{ui::Row{leaf(outside),ui::FocusScope{trapped,leaf(inside)}}};
    modal.resize({80,20}); modal.activate(platform);
    NUI_CHECK(inside->focus == 1);
    outside->request(); modal.refresh_focus(platform);
    NUI_CHECK(outside->focus == 0);
    modal.deactivate(platform);
}
class GroupLeaf final : public ui::Component, public ui::detail::FocusGroupParticipant {
public:
    GroupLeaf(std::shared_ptr<int> selected, int index, std::shared_ptr<Control> control)
        : selected_(std::move(selected)), index_(index), control_(std::move(control)) {}
    bool focusable() const noexcept override { return true; }
    const void* focus_group_identity() const noexcept override { return selected_.get(); }
    bool focus_group_selected() const override { return *selected_ == index_; }
    void focus_group_select() override { *selected_ = index_; }
    bool focus_group_accepts_navigation_key(ui::Key key) const noexcept override {
        return key == ui::Key::Up || key == ui::Key::Down;
    }
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40,20}; }
    void focus_changed(bool focused, ui::FocusContext&) override { if (focused) ++control_->focus; }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::KeyDown) ++control_->key_events;
        return ui::EventResult::Ignored;
    }
    void paint(ui::PaintContext&) const override {}
private:
    std::shared_ptr<int> selected_;
    int index_{};
    std::shared_ptr<Control> control_;
};
void navigation_filter_keeps_horizontal_keys_in_normal_routing() {
    auto selected = std::make_shared<int>(0);
    std::array<std::shared_ptr<Control>,3> controls{std::make_shared<Control>(),std::make_shared<Control>(),std::make_shared<Control>()};
    std::vector<ui::Spec> children;
    for (int i=0;i<3;++i) children.push_back({[selected,i,control=controls[static_cast<std::size_t>(i)]] {
        return std::make_unique<GroupLeaf>(selected,i,control); },{}});
    auto fault = std::make_shared<bool>(false);
    test::MockPlatform platform;
    ui::UI tree{overlap(fault,std::move(children))};
    tree.resize({40,20}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Right),platform);
    NUI_CHECK(*selected == 0 && controls[0]->key_events == 1);
    tree.dispatch(test::key(ui::Key::Down),platform);
    NUI_CHECK(*selected == 1 && controls[1]->focus == 1);
    tree.dispatch(test::key(ui::Key::Left),platform); NUI_CHECK(*selected == 1);
    tree.dispatch(test::key(ui::Key::Up),platform); NUI_CHECK(*selected == 0);
    tree.deactivate(platform);
}


struct RelayoutState {
    std::function<void()> invalidate;
    int hits{};
    float height{20};
    bool change_once{true};
    ui::Rect committed{};
};
class RelayoutLeaf final : public ui::Component {
public:
    explicit RelayoutLeaf(std::shared_ptr<RelayoutState> state) : state_(std::move(state)) {}
    bool focusable() const noexcept override { return true; }
    ui::EventResult input(const ui::InputEvent& event, ui::InputContext&) override {
        if (event.type == ui::InputType::PointerDown) { ++state_->hits; return ui::EventResult::Handled; }
        return ui::EventResult::Ignored;
    }
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40,state_->height}; }
    void mount(ui::MountContext& context) override { state_->invalidate = context.layout_invalidator(); }
    void layout_children(ui::Rect, const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>&) const override {
        if (std::exchange(state_->change_once,false)) { state_->height = 40; state_->invalidate(); }
    }
    void paint(ui::PaintContext&) const override {}
private:
    void layout_committed(ui::Rect, ui::Rect current) noexcept override { state_->committed = current; }
    std::shared_ptr<RelayoutState> state_;
};
void model_mutations_during_layout_keep_the_next_pass_pending() {
    auto state = std::make_shared<RelayoutState>();
    ui::Spec child{[state] { return std::make_unique<RelayoutLeaf>(state); },{}};
    ui::Tree tree{ui::compile(ui::make_spec(ui::Column{std::move(child)}))};
    test::MockPlatform platform;
    tree.mount(); tree.layout({100,100});
    NUI_CHECK(state->committed.h == 20);
    NUI_CHECK(tree.layout_dirty());
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(100,100));
    NUI_CHECK(surface);
    tree.paint(*surface->getCanvas(),platform);
    NUI_CHECK(!tree.layout_dirty() && state->committed.h == 40);
}


void pointer_hit_testing_uses_newly_requested_layout() {
    auto state = std::make_shared<RelayoutState>(); state->change_once = false;
    auto second = std::make_shared<Control>();
    ui::Spec child{[state] { return std::make_unique<RelayoutLeaf>(state); },{}};
    test::MockPlatform platform;
    ui::UI tree{ui::Column{std::move(child),leaf(second)}.padding(0).gap(0)};
    tree.resize({40,100}); tree.activate(platform);
    state->height = 60; state->invalidate();
    tree.dispatch(test::pointer(ui::InputType::PointerDown,10,30),platform);
    NUI_CHECK(state->hits == 1 && second->hits == 0);
    tree.deactivate(platform);
}

void suite() { pointer_hit_testing_uses_newly_requested_layout(); model_mutations_during_layout_keep_the_next_pass_pending(); foreground_matches_full_partial_hit_drop_and_logical_focus(); layout_notifications_publish_only_successful_geometry(); child_factory_unwinds_already_created_children_before_mount(); interactive_policy_rejects_future_descendants_and_recovers(); queued_focus_revalidates_availability_identity_and_traps(); navigation_filter_keeps_horizontal_keys_in_normal_routing(); children_are_created_per_compilation_and_fail_before_mount(); retained_focus_is_deferred_and_inert_after_destruction(); }
} // namespace
int main() { return test::run("widget_composition_contract", &suite); }
