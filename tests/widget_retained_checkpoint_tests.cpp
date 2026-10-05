#include "test_support.hpp"
#include <nativeui/component.hpp>
#include <limits>
#include "include/core/SkSurface.h"
#include "include/core/SkImageInfo.h"
#include <memory>
#include <stdexcept>

namespace {
struct Probe { int refresh{}, destroyed{}, fail_at{}; bool focusable{}; ui::Rect bounds{}; std::function<void()> during_paint; float height{20}; bool fail{}; std::function<void()> during; };
class CheckpointLeaf final : public ui::Component {
public:
  explicit CheckpointLeaf(std::shared_ptr<Probe> p) : p_(std::move(p)) {}
  ~CheckpointLeaf() override { ++p_->destroyed; }
  bool focusable() const noexcept override { return p_->focusable; }
  bool uses_retained_checkpoint() const noexcept override { return true; }
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40, height_}; }
  std::optional<float> first_baseline(ui::Size) const override { return 8.0f; }
  void mount(ui::MountContext& c) override { invalidate_ = c.layout_invalidator(); }
  void paint(ui::PaintContext& c) const override {
    p_->bounds = c.bounds();
    if (auto callback = std::exchange(p_->during_paint,{})) callback();
  }
  ui::EventResult input(const ui::InputEvent& e, ui::InputContext& c) override {
    if(e.type == ui::InputType::PointerDown) { c.capture_pointer(); return ui::EventResult::Handled; }
    if(e.type == ui::InputType::PointerUp) { c.release_pointer(); return ui::EventResult::Handled; }
    return ui::EventResult::Ignored;
  }
private:
  void retained_checkpoint() override {
    ++p_->refresh;
    if (std::exchange(p_->fail, false) || p_->refresh == p_->fail_at) throw std::runtime_error("checkpoint fault");
    if (auto callback = std::exchange(p_->during, {})) { callback(); NUI_CHECK(p_->destroyed == 0); }
    if (height_ != p_->height) { height_ = p_->height; invalidate_(); }
  }
  std::shared_ptr<Probe> p_;
  float height_{20};
  std::function<void()> invalidate_;
};
ui::Spec leaf(std::shared_ptr<Probe> p) { return {[p] { return std::make_unique<CheckpointLeaf>(p); }, {}}; }
void refresh_is_outside_measure_and_recovers_after_fault() {
  auto p = std::make_shared<Probe>(); ui::UI tree{leaf(p)};
  auto m = tree.measure(); NUI_CHECK(m.first_baseline == 8 && p->refresh == 0);
  tree.resize({40,60}); NUI_CHECK(p->refresh == 1);
  p->height = 40; p->fail = true;
  bool threw{}; try { tree.resize({40,60}); } catch (const std::runtime_error&) { threw = true; }
  NUI_CHECK(threw && tree.measure().preferred.h == 20);
  tree.resize({40,60}); NUI_CHECK(tree.measure().preferred.h == 40);
}
void refresh_defers_destructive_reentrant_resize_and_removes_identity() {
  auto p = std::make_shared<Probe>(); ui::State<bool> present{true};
  ui::UI tree{ui::If{present,leaf(p)}}; tree.resize({40,60});
  p->during = [&] { present.set(false); tree.resize({41,60}); };
  tree.resize({40,60}); NUI_CHECK(p->destroyed == 1);
  const int count = p->refresh; tree.resize({42,60}); NUI_CHECK(p->refresh == count);
  present.set(true); tree.resize({40,60}); tree.resize({40,60}); NUI_CHECK(p->refresh > count);
}
class BaselineLeaf final : public ui::Component {
public:
  explicit BaselineLeaf(float value) : value_(value) {}
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {20,20}; }
  std::optional<float> first_baseline(ui::Size) const override { return value_; }
  void paint(ui::PaintContext&) const override {}
private: float value_;
};
void baseline_is_local_bounded_and_optional() {
  for (float invalid : {-1.f,21.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
    ui::Spec s{[invalid] { return std::make_unique<BaselineLeaf>(invalid); },{}};
    ui::UI tree{std::move(s)}; NUI_CHECK(!tree.measure().first_baseline);
  }
  ui::UI old{ui::Spacer{20,20}}; NUI_CHECK(!old.measure().first_baseline);
}
struct ScopeRecord { std::vector<int> bound; };
class ScopedLeaf final : public ui::Component {
public:
  std::shared_ptr<ScopeRecord> record;
  explicit ScopedLeaf(std::shared_ptr<ScopeRecord> r) : record(std::move(r)) {}
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {1,1}; }
  void paint(ui::PaintContext&) const override {}
};
class ContextOwner final : public ui::Component {
public:
  explicit ContextOwner(int value) : value_(value) {}
  ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {1,1}; }
  void paint(ui::PaintContext&) const override {}
private:
  void bind_descendant_context(ui::Component& c) const override {
    if (auto* leaf = dynamic_cast<ScopedLeaf*>(&c)) leaf->record->bound.push_back(value_);
  }
  int value_;
};
void contexts_are_outer_to_inner_and_keys_are_owned() {
  auto r = std::make_shared<ScopeRecord>();
  ui::Spec child{[r] { return std::make_unique<ScopedLeaf>(r); },{}};
  child = ui::keyed("control",std::move(child));
  ui::Spec inner{[] { return std::make_unique<ContextOwner>(2); },{std::move(child)}};
  ui::Spec outer{[] { return std::make_unique<ContextOwner>(1); },{std::move(inner)}};
  auto tree = ui::compile(std::move(outer));
  NUI_CHECK((r->bound == std::vector<int>{1,2}));
  NUI_CHECK(tree->children[0]->children[0]->retained_key == "control");
}

void pending_resize_preserves_the_newer_reentrant_request_after_fault() {
  auto p = std::make_shared<Probe>(); ui::UI tree{leaf(p)}; tree.resize({100,60});
  test::MockPlatform platform;
  auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(140,60)); NUI_CHECK(surface);
  p->during_paint = [&] { tree.resize({80,60}); };
  tree.paint(*surface->getCanvas(),platform);
  p->during = [&] { tree.resize({120,60}); throw std::runtime_error("newer resize then failure"); };
  bool threw{}; try { tree.paint(*surface->getCanvas(),platform); } catch(const std::runtime_error&) { threw=true; }
  NUI_CHECK(threw);
  tree.paint(*surface->getCanvas(),platform);
  NUI_CHECK(p->bounds.w == 120);
}
void availability_drain_survives_removal_of_the_last_participant() {
  auto p = std::make_shared<Probe>(); ui::State<bool> present{true};
  ui::State<ui::VisibilityMode> mode{ui::VisibilityMode::Visible};
  ui::Tree tree{ui::compile(ui::make_spec(ui::Row{ui::If{present,leaf(p)},ui::Visibility{mode,ui::Spacer{20,20}}}.gap(0)))};
  tree.mount(); tree.layout({100,60});
  p->during = [&] { mode.set(ui::VisibilityMode::Collapsed); present.set(false); throw std::runtime_error("last participant removed"); };
  bool threw{}; try { tree.layout({100,60}); } catch(const std::runtime_error&) { threw=true; }
  NUI_CHECK(threw); (void)tree.measure(ui::Constraints::unbounded()); NUI_CHECK(p->destroyed==1);
  tree.layout({100,60}); NUI_CHECK(tree.measure(ui::Constraints::unbounded()).preferred.w==0);
}
struct HoverObservation { bool saw{}, held{}; };
class HoverOwner final : public ui::Component, public ui::detail::RetainedInteractionObserver {
public:
  explicit HoverOwner(std::shared_ptr<HoverObservation> s) : state_(std::move(s)) {}
  ui::Size measure(const std::vector<ui::ChildMetrics>& children) const override { return children.front().preferred; }
  void layout_children(ui::Rect b,const std::vector<ui::ChildMetrics>&,std::vector<ui::ChildPlacement>& p) const override { p[0].bounds=b; }
  void paint(ui::PaintContext&) const override {}
  void retained_pointer_hover_changed(bool hovered,bool held,ui::Dispatcher) override { if(hovered) { state_->saw=true;state_->held=held; } }
private: std::shared_ptr<HoverObservation> state_;
};
void failing_input_checkpoint_does_not_start_a_pointer_contact() {
  auto p=std::make_shared<Probe>(); p->focusable=true; auto hover=std::make_shared<HoverObservation>();
  ui::Spec root{[hover]{return std::make_unique<HoverOwner>(hover);},{leaf(p)}};
  ui::UI tree{std::move(root)}; test::MockPlatform platform; tree.resize({40,60}); tree.activate(platform);
  p->fail=true; auto down=test::pointer(ui::InputType::PointerDown,5,5); down.pointer.id=17;down.pointer.type=ui::PointerType::Mouse;
  bool threw{};try { tree.dispatch(down,platform); } catch(const std::runtime_error&) { threw=true; } NUI_CHECK(threw);
  auto move=down;move.type=ui::InputType::PointerMove; tree.dispatch(move,platform);
  NUI_CHECK(hover->saw && !hover->held);
  tree.dispatch(down,platform);auto up=down;up.type=ui::InputType::PointerUp;tree.dispatch(up,platform);tree.deactivate(platform);
}
void overlay_dispatch_runs_one_source_checkpoint() {
  auto p=std::make_shared<Probe>();p->focusable=true; ui::UI tree{leaf(p)};test::MockPlatform platform;
  tree.resize({100,60});tree.activate(platform);
  ui::OverlaySpec overlay;overlay.content=ui::make_spec(ui::Spacer{10,10});overlay.placement=ui::OverlayPlacement::Center;
  auto handle=tree.show_overlay(std::move(overlay));NUI_CHECK(handle.valid());
  const auto before=p->refresh;p->fail_at=before+2;
  tree.dispatch(test::pointer(ui::InputType::PointerMove,5,5),platform);
  NUI_CHECK(p->refresh==before+1);tree.deactivate(platform);
}
void suite() { pending_resize_preserves_the_newer_reentrant_request_after_fault(); availability_drain_survives_removal_of_the_last_participant(); failing_input_checkpoint_does_not_start_a_pointer_contact(); overlay_dispatch_runs_one_source_checkpoint();  refresh_is_outside_measure_and_recovers_after_fault(); refresh_defers_destructive_reentrant_resize_and_removes_identity(); baseline_is_local_bounded_and_optional(); contexts_are_outer_to_inner_and_keys_are_owned(); }
}
int main(int argc,char** argv) {
  if(argc==2 && std::string_view{argv[1]}=="resize")return test::run("checkpoint_resize",&pending_resize_preserves_the_newer_reentrant_request_after_fault);
  if(argc==2 && std::string_view{argv[1]}=="drain")return test::run("checkpoint_drain",&availability_drain_survives_removal_of_the_last_participant);
  if(argc==2 && std::string_view{argv[1]}=="input")return test::run("checkpoint_input",&failing_input_checkpoint_does_not_start_a_pointer_contact);
  if(argc==2 && std::string_view{argv[1]}=="overlay")return test::run("checkpoint_overlay",&overlay_dispatch_runs_one_source_checkpoint);
  return test::run("widget_retained_checkpoint",&suite);
}
