#include "test_support.hpp"
#include <nativeui/column.hpp>
#include <nativeui/enabled.hpp>
#include <nativeui/focus_scope.hpp>

#include <memory>
#include <optional>

namespace {
struct Observation {
    ui::NodeId id{};
    ui::Rect bounds{};
    bool focused{};
};
class ProbeComponent final : public ui::Component, public ui::detail::OverlayAnchorPolicy {
public:
    ProbeComponent(std::shared_ptr<Observation> state, ui::Size size)
        : state_(std::move(state)), size_(size) {}
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return size_; }
    bool focusable() const noexcept override { return true; }
    void mount(ui::MountContext& context) override { state_->id = context.node_id(); }
    void focus_changed(bool value, ui::FocusContext&) override { state_->focused = value; }
    ui::SemanticInfo semantics() const override {
        ui::SemanticInfo value; value.role=ui::SemanticRole::Button; value.name="Anchor";
        return value;
    }
    void paint(ui::PaintContext& context) const override { state_->bounds=context.bounds(); }
private:
    std::shared_ptr<Observation> state_;
    ui::Size size_;
};
ui::Spec probe(std::shared_ptr<Observation> state, ui::Size size={120.0f,40.0f}) {
    return {[state=std::move(state),size] { return std::make_unique<ProbeComponent>(state,size); },{}};
}
ui::OverlaySpec panel(ui::NodeId anchor) {
    ui::OverlaySpec value;
    value.anchor=anchor;
    value.content=ui::Spacer{20.0f,20.0f}.spec();
    value.dismiss_on_outside_pointer_down=true;
    value.dismiss_on_escape=true;
    return value;
}
void terminal_reasons_survive_exact_handle_expiration() {
    auto anchor=std::make_shared<Observation>();
    ui::State<bool> enabled{true};
    auto tree=std::make_unique<ui::UI>(ui::Enabled{enabled,probe(anchor)});
    test::MockPlatform platform;
    tree->resize({200.0f,180.0f}); tree->activate(platform);
    auto first=tree->show_overlay(panel(anchor->id)); tree->resize({200.0f,180.0f});
    NUI_CHECK(!first.close_reason());
    NUI_CHECK(tree->dispatch(test::pointer(ui::InputType::PointerDown,199.0f,179.0f),platform)==ui::EventResult::Handled);
    NUI_CHECK(!first.valid() && first.close_reason()==ui::OverlayCloseReason::UserOutside);
    auto second=tree->show_overlay(panel(anchor->id)); tree->resize({200.0f,180.0f});
    tree->dispatch(test::key(ui::Key::Escape),platform);
    NUI_CHECK(!second.valid() && second.close_reason()==ui::OverlayCloseReason::UserEscape);
    auto third=tree->show_overlay(panel(anchor->id));
    NUI_CHECK(tree->close_overlay(third));
    NUI_CHECK(third.close_reason()==ui::OverlayCloseReason::Explicit);
    auto fourth=tree->show_overlay(panel(anchor->id)); tree->resize({200.0f,180.0f});
    enabled.set(false); tree->resize({200.0f,180.0f});
    NUI_CHECK(!fourth.valid() && fourth.close_reason()==ui::OverlayCloseReason::AnchorUnavailable);
    enabled.set(true); tree->resize({200.0f,180.0f});
    auto fifth=tree->show_overlay(panel(anchor->id));
    tree.reset();
    NUI_CHECK(!fifth.valid() && fifth.close_reason()==ui::OverlayCloseReason::OwnerTeardown);
    NUI_CHECK(first.close_reason()==ui::OverlayCloseReason::UserOutside);
}
void anchor_width_is_a_minimum_bound_by_viewport() {
    auto anchor=std::make_shared<Observation>();
    ui::UI tree{ui::Column{probe(anchor),ui::Spacer{0.0f,100.0f}}.padding(0.0f).gap(0.0f).align(ui::Align::Stretch)};
    test::MockPlatform platform; tree.resize({200.0f,180.0f}); tree.activate(platform);
    auto value=panel(anchor->id); value.match_anchor_width=true;
    auto handle=tree.show_overlay(std::move(value)); tree.resize({200.0f,180.0f});
    auto entries=tree.overlay_entries(); NUI_CHECK(entries.size()==1);
    NUI_CHECK_NEAR(entries.front().bounds.w,200.0f,0.01f);
    tree.resize({80.0f,180.0f}); entries=tree.overlay_entries();
    NUI_CHECK_NEAR(entries.front().bounds.w,80.0f,0.01f);
    NUI_CHECK(tree.close_overlay(handle));
    value=panel(anchor->id); value.match_anchor_width=false;
    handle=tree.show_overlay(std::move(value)); tree.resize({200.0f,180.0f});
    entries=tree.overlay_entries(); NUI_CHECK_NEAR(entries.front().bounds.w,20.0f,0.01f);
}
class ExpandedDecorator final : public ui::Component {
public:
    ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override {
        return children.empty()?ui::Size{}:children.front().preferred;
    }
    void layout_children(ui::Rect bounds,const std::vector<ui::ChildMetrics>&,
                         std::vector<ui::ChildPlacement>& placements) const override {
        if (!placements.empty()) placements.front().bounds=bounds;
    }
    void paint(ui::PaintContext&) const override {}
private:
    std::optional<ui::detail::DescendantSemanticDecoration> descendant_semantic_decoration() const override {
        ui::detail::DescendantSemanticDecoration value; value.expanded=true; return value;
    }
};
void expanded_decoration_preserves_anchor_role_and_name() {
    auto anchor=std::make_shared<Observation>();
    ui::Spec root{[] { return std::make_unique<ExpandedDecorator>(); },{probe(anchor)}};
    ui::UI tree{std::move(root)};
    auto info=tree.component_semantics(anchor->id);
    NUI_CHECK(info && info->role==ui::SemanticRole::Button && info->name=="Anchor");
    NUI_CHECK(info->expanded==ui::SemanticExpandedState::Expanded);
}
void nonmodal_scope_restores_previous_focus_when_removed() {
    auto anchor=std::make_shared<Observation>(), outside=std::make_shared<Observation>(), body=std::make_shared<Observation>();
    ui::State<bool> active{true};
    ui::UI tree{ui::Column{probe(anchor),probe(outside)}.padding(0.0f).gap(0.0f)};
    test::MockPlatform platform; tree.resize({200.0f,180.0f}); tree.activate(platform);
    tree.dispatch(test::key(ui::Key::Tab),platform); NUI_CHECK(outside->focused);
    ui::OverlaySpec value=panel(anchor->id);
    value.content=ui::FocusScope{active,probe(body,{20.0f,20.0f})}.trap(false).spec();
    auto handle=tree.show_overlay(std::move(value)); tree.resize({200.0f,180.0f}); tree.refresh_focus(platform);
    NUI_CHECK(body->focused && !outside->focused);
    NUI_CHECK(tree.close_overlay(handle)); tree.resize({200.0f,180.0f}); tree.refresh_focus(platform);
    NUI_CHECK(outside->focused && !anchor->focused);
}
void suite() {
    terminal_reasons_survive_exact_handle_expiration();
    anchor_width_is_a_minimum_bound_by_viewport();
    expanded_decoration_preserves_anchor_role_and_name();
    nonmodal_scope_restores_previous_focus_when_removed();
}
}
int main() { return test::run("popover_overlay_seams",&suite); }
