#include <nativeui/checkbox_group.hpp>
#include <nativeui/detail/theme_binding.hpp>
#include "detail/layout_support.hpp"
#include <unordered_set>
#include <stdexcept>

namespace ui {
namespace {
struct GroupModel {
  std::vector<CheckboxGroupItem> items;
  std::vector<Binding<bool>::Subscription> subscriptions;
  std::function<void()> invalidate;
  std::function<bool()> permission;
  SemanticCheckedState aggregate{SemanticCheckedState::Unchecked};
  bool mounted{};
  std::uint64_t generation{};
  bool mutable_child() const noexcept {
    for (const auto& item:items) if(item.enabled && !item.read_only && item.checked.valid()) return true;
    return false;
  }
  SemanticCheckedState read_aggregate() const noexcept {
    std::size_t checked{};
    for(const auto& item:items) checked+=item.checked.get()?1U:0U;
    return checked==0?SemanticCheckedState::Unchecked:checked==items.size()?SemanticCheckedState::Checked:SemanticCheckedState::Mixed;
  }
  void refresh() {
    const auto next=read_aggregate(); if(next==aggregate)return;aggregate=next;
    auto notify=invalidate;if(mounted && notify)notify();
  }
  void publish(bool value,std::uint64_t expected,const std::function<bool()>& context_guard) {
    const auto snapshot=items;
    for(const auto& item:snapshot) {
      if(!mounted || generation!=expected || (permission && !permission()) || (context_guard && !context_guard()))return;
      if(item.enabled && !item.read_only && item.checked.valid()) {
        auto checked = item.checked;
        checked.set(value);
      }
    }
  }
};
struct Contact {
  detail::PressActivationState interaction;
  std::function<void()> release;
  std::uint64_t generation{};
  bool focused{};
  void reset() noexcept {
    ++generation; interaction={}; auto callback=std::exchange(release,{});
    try{if(callback)callback();}catch(...){}
  }
};
class Parent final:public Component,public detail::ThemeBinding {
public:
  Parent(std::string label,std::shared_ptr<GroupModel> model,CheckboxGroupStyle style):label_(std::move(label)),model_(std::move(model)),style_(std::move(style)){}
  bool focusable() const noexcept override{return model_->mutable_child();}
  bool cancel_capture_on_read_only() const noexcept override{return true;}
  Size measure(const std::vector<ChildMetrics>&) const override {
    const auto resolved=resolve(false);const auto text=TextService::measure(label_,text_style(resolved));
    return {resolved.leading_padding+resolved.box_size+resolved.label_gap+text.width+resolved.leading_padding,std::max(resolved.control_height,text.height)};
  }
  SemanticInfo semantics() const override {
    SemanticInfo info;info.role=SemanticRole::Checkbox;info.name=label_;info.checked=model_->read_aggregate();info.focusable=focusable();info.focused=contact_->focused;
    info.enabled=effective_enabled();info.read_only=effective_read_only() || !model_->mutable_child();
    if(info.enabled && info.focusable) {info.actions.push_back(SemanticAction::Focus);if(!info.read_only)info.actions.push_back(SemanticAction::Toggle);}return info;
  }
  void focus_changed(bool focused,FocusContext& context) override {
    contact_->focused=focused;if(!focused)contact_->reset();context.invalidate();
  }
  void unmount(LifecycleContext&) override{contact_->reset();}
  void deactivate(LifecycleContext&) override{contact_->reset();contact_->focused=false;}
  EventResult semantic_action(SemanticAction action,InputContext& context) override {
    if(action!=SemanticAction::Toggle && action!=SemanticAction::Activate)return EventResult::Ignored;
    const auto model=model_;const auto expected=model->generation;const auto guard=detail::InputMutationAccess::guard(context);
    const bool value=model->read_aggregate()!=SemanticCheckedState::Checked;
    if(!effective_enabled() || effective_read_only() || !model->mutable_child())return EventResult::Ignored;
    model->publish(value,expected,guard);return EventResult::Handled;
  }
  EventResult input(const InputEvent& event,InputContext& context) override {
    const auto model=model_;const auto contact=contact_;const auto owner_generation=model->generation;
    const auto guard=detail::InputMutationAccess::guard(context);
    if(effective_read_only() || !model->mutable_child()) {contact->reset();return EventResult::Handled;}
    if(event.type==InputType::PointerDown) {contact->reset();contact->release=context.pointer_releaser();}
    if(event.type==InputType::KeyDown && (event.key==Key::Space || event.key==Key::Enter))++contact->generation;
    const auto generation=contact->generation;
    const auto outcome=contact->interaction.input(event,context,false);
    if(event.type==InputType::PointerUp)contact->release={};
    else if(event.type==InputType::PointerCancel)contact->reset();
    if(outcome.activate && contact->generation==generation && model->mounted && model->generation==owner_generation) {
      const bool value=model->read_aggregate()!=SemanticCheckedState::Checked;
      model->publish(value,owner_generation,guard);
    }
    return outcome.result;
  }
  void paint(PaintContext& context) const override {
    const auto bounds=context.bounds();const auto resolved=resolve(context.focused());auto& painter=context.painter();
    const Rect box{bounds.x+resolved.leading_padding,bounds.y+(bounds.h-resolved.box_size)/2,resolved.box_size,resolved.box_size};
    auto clip=painter.scoped_clip(bounds);painter.fill_rounded_rect(box,resolved.box_corner_radius,resolved.box_fill);
    painter.stroke_rounded_rect(box,resolved.box_corner_radius,resolved.box_border_width,resolved.box_border);
    const auto color=style_.mixed_color.value_or(resolved.checkmark);
    if(model_->aggregate==SemanticCheckedState::Mixed)painter.line({box.x+box.w*.2f,box.y+box.h*.5f},{box.x+box.w*.8f,box.y+box.h*.5f},resolved.checkmark_width,color);
    else if(model_->aggregate==SemanticCheckedState::Checked) {
      painter.line({box.x+box.w*.2f,box.y+box.h*.5f},{box.x+box.w*.44f,box.y+box.h*.72f},resolved.checkmark_width,resolved.checkmark);
      painter.line({box.x+box.w*.44f,box.y+box.h*.72f},{box.x+box.w*.84f,box.y+box.h*.25f},resolved.checkmark_width,resolved.checkmark);
    }
    painter.text({box.x+box.w+resolved.label_gap,bounds.y+bounds.h/2},label_,text_style(resolved));
  }
private:
  ResolvedCheckboxStyle resolve(bool focus) const {
    const VisualState state{.enabled=effective_enabled(),.read_only=effective_read_only(),.hovered=contact_->interaction.hovered(),.pressed=contact_->interaction.pressed(),.focused=focus,.checked=model_->aggregate!=SemanticCheckedState::Unchecked};
    return resolve_checkbox_style(default_checkbox_style(current_theme()),style_.parent,state);
  }
  static TextStyle text_style(const ResolvedCheckboxStyle& resolved) {
    TextStyle style;style.size=resolved.text_size;style.color=resolved.text;style.family=resolved.font_family;style.fallback_families=resolved.fallback_families;style.weight=resolved.text_weight;style.slant=resolved.text_slant;return style;
  }
  std::string label_;std::shared_ptr<GroupModel> model_;CheckboxGroupStyle style_;std::shared_ptr<Contact> contact_{std::make_shared<Contact>()};
};
class ItemAvailability final:public Component {
public:
  ItemAvailability(bool enabled,bool read_only):enabled_(enabled),read_only_(read_only){}
  ComponentAvailability local_availability() const noexcept override{return {VisibilityMode::Visible,enabled_,read_only_};}
  Size measure(const std::vector<ChildMetrics>& children) const override{return children.empty()?Size{}:children.front().preferred;}
  Size minimum_size(const std::vector<ChildMetrics>& children) const override{return children.empty()?Size{}:children.front().minimum;}
  Constraints child_constraints(const Constraints& constraints,std::size_t,std::size_t)const override{return constraints;}
  void layout_children(Rect bounds,const std::vector<ChildMetrics>&,std::vector<ChildPlacement>& placements)const override{if(!placements.empty())placements.front().bounds=bounds;}
  void paint(PaintContext&)const override{}
private:bool enabled_,read_only_;
};
class Group final:public Component {
public:
  Group(std::string label,std::vector<CheckboxGroupItem> items,CheckboxGroupStyle style):label_(std::move(label)),style_(std::move(style)),model_(std::make_shared<GroupModel>()) {model_->items=std::move(items);model_->aggregate=model_->read_aggregate();}
  std::vector<Spec> children() const {
    std::vector<Spec> children;const auto model=model_;const auto style=style_;
    children.push_back({[model,style,label=label_]{return std::make_unique<Parent>(label,model,style);},{}});
    for(const auto& item:model->items) {
      auto child=ui::Checkbox{item.checked,item.label}.style(style.child).spec();
      children.push_back(Spec{[enabled=item.enabled,read_only=item.read_only]{return std::make_unique<ItemAvailability>(enabled,read_only);},{std::move(child)}, {}, item.key});
    }
    return children;
  }
  bool uses_retained_checkpoint()const noexcept override{return true;}
  Size measure(const std::vector<ChildMetrics>& children)const override{return extent(children,false);}
  Size minimum_size(const std::vector<ChildMetrics>& children)const override{return extent(children,true);}
  Constraints child_constraints(const Constraints& constraints,std::size_t index,std::size_t)const override{return Constraints::loose({constraints.bounded_width()?std::max(0.0f,constraints.max.w-(index?style_.indentation:0)):kUnboundedExtent,kUnboundedExtent});}
  void layout_children(Rect bounds,const std::vector<ChildMetrics>& children,std::vector<ChildPlacement>& placements)const override {
    float y=bounds.y;
    for(std::size_t i=0;i<children.size() && i<placements.size();++i) {const auto inset=i?style_.indentation:0;const auto height=std::min(children[i].preferred.h,std::max(0.0f,bounds.y+bounds.h-y));placements[i].bounds={bounds.x+inset,y,std::max(0.0f,bounds.w-inset),height};y+=height+style_.gap;}
  }
  void mount(MountContext& context)override {
    model_->mounted=true;++model_->generation;model_->permission=detail::InputMutationAccess::guard(context);model_->invalidate=context.invalidator();
    const std::weak_ptr<GroupModel> weak=model_;
    for(const auto& item:model_->items) {
      auto checked = item.checked;
      model_->subscriptions.push_back(checked.observe([weak](bool){
        if(const auto model=weak.lock();model && model->mounted)model->refresh();
      }));
    }
  }
  void unmount(LifecycleContext&)override{model_->mounted=false;++model_->generation;model_->subscriptions.clear();model_->permission={};model_->invalidate={};}
  SemanticInfo semantics()const override{SemanticInfo info;info.role=SemanticRole::Group;info.name=label_;info.enabled=effective_enabled();info.read_only=effective_read_only();return info;}
  void paint(PaintContext&)const override{}
private:
  void retained_checkpoint()override{model_->refresh();}
  Size extent(const std::vector<ChildMetrics>& children,bool minimum)const {
    float width{},height{};std::size_t count{};
    for(std::size_t i=0;i<children.size();++i) {if(!children[i].participates_in_layout)continue;const auto size=minimum?children[i].minimum:children[i].preferred;width=std::max(width,size.w+(i?style_.indentation:0));height=detail::saturating_extent(static_cast<double>(height)+size.h+(count++?style_.gap:0));}return {width,height};
  }
  std::string label_;CheckboxGroupStyle style_;std::shared_ptr<GroupModel> model_;
};
}
CheckboxGroup::CheckboxGroup(std::string label,std::vector<CheckboxGroupItem> items):label_(std::move(label)),items_(std::move(items)) {
  std::unordered_set<std::string> keys;for(const auto& item:items_)if(item.key.empty() || !keys.insert(item.key).second)throw std::invalid_argument("CheckboxGroup requires unique non-empty keys");
}
CheckboxGroup&& CheckboxGroup::style(CheckboxGroupStyle value)&& {value.indentation=detail::saturating_extent(value.indentation);value.gap=detail::saturating_extent(value.gap);style_=std::move(value);return std::move(*this);}
Spec CheckboxGroup::spec()&& {
  Spec spec{[label=std::move(label_),items=std::move(items_),style=std::move(style_)] {return std::make_unique<Group>(label,items,style);},{}};
  spec.children_factory=[](Component& component){return static_cast<Group&>(component).children();};return spec;
}
} // namespace ui
