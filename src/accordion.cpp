#include <nativeui/accordion.hpp>
#include <nativeui/detail/theme_binding.hpp>

#include "detail/disclosure_kernel.hpp"
#include "detail/layout_support.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>

namespace ui {
namespace {
struct AccordionSection {
    std::string key;
    std::string title;
    std::shared_ptr<const Spec> content;
    bool enabled{};
};
struct AccordionController {
    explicit AccordionController(Binding<std::vector<std::string>> value) : source(std::move(value)) {}
    Binding<std::vector<std::string>> source;
    Binding<std::vector<std::string>>::Subscription subscription;
    std::shared_ptr<const std::vector<AccordionSection>> sections;
    std::vector<std::shared_ptr<detail::DisclosureState>> states;
    AccordionMode mode{AccordionMode::Multiple};
    std::function<void(const std::vector<std::string>&)> callback;
    std::size_t selected{};
    bool mounted{};
    bool syncing{};
    std::uint64_t write_serial{};

    std::vector<std::string> canonical(const std::vector<std::string>& external) const {
        std::vector<std::string> result;
        for (const auto& section : *sections) {
            if (std::find(external.begin(),external.end(),section.key) == external.end()) continue;
            result.push_back(section.key);
            if (mode == AccordionMode::Single) break;
        }
        return result;
    }
    void sync() {
        const auto keys = canonical(source.get());
        // Prepare every target and its durable effect journal before crossing
        // any boundary. Reentrant sync may replace a target without flushing.
        for (std::size_t i = 0; i < states.size(); ++i)
            detail::prepare_disclosure_change(states[i],
                std::find(keys.begin(),keys.end(),(*sections)[i].key) != keys.end());
        if (syncing) return;
        syncing = true;
        try {
            for (const auto& state : states) {
                if (!mounted) break;
                detail::flush_disclosure_change(state);
            }
        } catch (...) { syncing = false; throw; }
        syncing = false;
    }
    void write(std::size_t index,bool open) {
        if (!mounted || !source.valid() || index >= states.size() || !(*sections)[index].enabled) return;
        auto keys = canonical(source.get());
        const auto& key = (*sections)[index].key;
        const bool was_open = std::find(keys.begin(),keys.end(),key) != keys.end();
        if (was_open == open) return;
        if (open && mode == AccordionMode::Single) keys.clear();
        if (open) keys.push_back(key); else std::erase(keys,key);
        keys = canonical(keys);
        const auto before = source.get();
        const auto serial = ++write_serial;
        for (const auto& state : states) {
            if (state->invalidate_layout) state->invalidate_layout();
            if (!mounted || !source.valid() || write_serial != serial) return;
        }
        try { source.set(keys); }
        catch (...) {
            const auto failure = std::current_exception();
            try { sync(); } catch (...) {}
            std::rethrow_exception(failure);
        }
        sync();
        if (!mounted || !source.valid() || write_serial != serial) return;
        const auto accepted = source.get();
        if (accepted == before) return;
        const auto notify = callback;
        if (notify && mounted && source.valid() && write_serial == serial &&
            source.get() == accepted) notify(accepted);
    }
    void edge(Key key) {
        if (states.empty()) return;
        for (std::size_t offset = 0; offset < states.size(); ++offset) {
            const auto index = key == Key::End ? states.size()-1-offset : offset;
            if (!(*sections)[index].enabled) continue;
            selected = index;
            if (states[index]->request_focus) states[index]->request_focus();
            return;
        }
    }
};

class AccordionComponent final : public Component, public detail::ThemeBinding {
public:
    AccordionComponent(std::shared_ptr<AccordionController> controller,AccordionStyle style)
        : controller_(std::move(controller)),style_(std::move(style)) {}
    [[nodiscard]] Size measure(const std::vector<ChildMetrics>& children) const override { return size_for(children,false); }
    [[nodiscard]] Size minimum_size(const std::vector<ChildMetrics>& children) const override { return size_for(children,true); }
    [[nodiscard]] Constraints child_constraints(const Constraints& constraints,std::size_t,std::size_t) const override {
        return {{},{constraints.bounded_width() ? detail::saturating_extent(static_cast<double>(constraints.max.w) - 2.0 * style_.padding) :
                                                kUnboundedExtent,kUnboundedExtent}};
    }
    void layout_children(Rect bounds,const std::vector<ChildMetrics>& children,std::vector<ChildPlacement>& placements) const override {
        std::vector<float> separators;
        separators.reserve(children.empty() ? 0 : children.size()-1);
        double y = static_cast<double>(bounds.y) + style_.padding;
        const auto width = detail::saturating_extent(static_cast<double>(bounds.w) - 2.0 * style_.padding);
        const auto count = std::min(children.size(),placements.size());
        for (std::size_t i = 0; i < count; ++i) {
            placements[i].bounds = {detail::saturating_coordinate(static_cast<double>(bounds.x) + style_.padding),
                detail::saturating_coordinate(y),width,children[i].preferred.h};
            y += children[i].preferred.h;
            if (i+1 < count) {
                separators.push_back(detail::saturating_coordinate(y + style_.separator_width * 0.5));
                y += style_.separator_width;
            }
        }
        pending_separators_ = std::move(separators);
    }
    [[nodiscard]] SemanticInfo semantics() const override {
        SemanticInfo info;
        info.role = SemanticRole::Group; info.name = style_.accessible_name;
        info.enabled = effective_enabled(); info.read_only = effective_read_only();
        return info;
    }
    [[nodiscard]] bool uses_retained_checkpoint() const noexcept override { return true; }
    void mount(MountContext&) override {
        controller_->mounted = true;
        const std::weak_ptr<AccordionController> weak = controller_;
        controller_->subscription = controller_->source.observe([weak](const auto&) {
            if (const auto current = weak.lock()) current->sync();
        });
        controller_->sync();
    }
    void unmount(LifecycleContext&) override { controller_->mounted = false; controller_->subscription.reset(); }
    std::vector<Spec> children() const {
        std::vector<Spec> result;
        result.reserve(controller_->states.size());
        for (std::size_t i = 0; i < controller_->states.size(); ++i)
            result.push_back(detail::disclosure_spec(controller_->states[i],(*controller_->sections)[i].content,controller_));
        return result;
    }
    void paint(PaintContext& context) const override {
        const auto bounds = context.bounds();
        auto& painter = context.painter();
        painter.fill_rounded_rect(bounds,style_.corner_radius,style_.background.value_or(current_theme().palette.surface));
        if (style_.separator_width > 0.0f)
            for (const auto y : separators_) painter.line(
                {bounds.x+style_.padding,y},{bounds.x+bounds.w-style_.padding,y},
                style_.separator_width,style_.separator_color.value_or(current_theme().palette.border));
        if (style_.border_width > 0.0f) {
            const auto inset = style_.border_width * 0.5f;
            painter.stroke_rounded_rect({bounds.x+inset,bounds.y+inset,std::max(0.0f,bounds.w-style_.border_width),
                std::max(0.0f,bounds.h-style_.border_width)},style_.corner_radius,style_.border_width,
                style_.border_color.value_or(current_theme().palette.border));
        }
    }
private:
    void retained_checkpoint() override { controller_->sync(); }
    Size size_for(const std::vector<ChildMetrics>& children,bool minimum) const {
        double width = 0.0, height = 2.0 * style_.padding;
        for (const auto& child : children) {
            if (!child.participates_in_layout) continue;
            const auto size = minimum ? child.minimum : child.preferred;
            width = std::max(width,static_cast<double>(size.w)); height += size.h;
        }
        if (!children.empty()) height += static_cast<double>(children.size()-1) * style_.separator_width;
        return {detail::saturating_extent(width + 2.0 * style_.padding),detail::saturating_extent(height)};
    }
    std::shared_ptr<AccordionController> controller_;
    AccordionStyle style_;
    void layout_committed(Rect,Rect) noexcept override { separators_.swap(pending_separators_); }
    mutable std::vector<float> pending_separators_;
    std::vector<float> separators_;
};
} // namespace

Accordion::Accordion(Binding<std::vector<std::string>> source) : open_keys_(std::move(source)) {}
Accordion::Accordion(State<std::vector<std::string>>& source) : Accordion(source.binding()) {}
Accordion&& Accordion::mode(AccordionMode value) && { mode_ = value; return std::move(*this); }
Accordion&& Accordion::content_policy(DisclosureContentPolicy value) && { policy_ = value; return std::move(*this); }
Accordion&& Accordion::on_change(std::function<void(const std::vector<std::string>&)> callback) && {
    on_change_ = std::move(callback); return std::move(*this);
}
Accordion&& Accordion::style(AccordionStyle value) && { style_ = std::move(value); return std::move(*this); }
Spec Accordion::spec() && {
    if (mode_ != AccordionMode::Multiple && mode_ != AccordionMode::Single) throw std::invalid_argument("Accordion mode is invalid");
    for (const auto value : {style_.padding,style_.corner_radius,style_.border_width,style_.separator_width})
        if (!std::isfinite(value) || value < 0.0f) throw std::invalid_argument("Accordion style extents must be finite and nonnegative");
    for (std::size_t i = 0; i < sections_.size(); ++i) {
        if (sections_[i].key.empty()) throw std::invalid_argument("Accordion section key must not be empty");
        detail::validate_disclosure(sections_[i].title,style_.section,policy_);
        for (std::size_t j = 0; j < i; ++j)
            if (sections_[i].key == sections_[j].key) throw std::invalid_argument("Accordion section keys must be unique");
    }
    // Validate the policy even for an empty group.
    detail::validate_disclosure("Accordion",style_.section,policy_);
    std::vector<AccordionSection> models;
    models.reserve(sections_.size());
    for (auto& section : sections_) models.push_back({std::move(section.key),std::move(section.title),
        std::make_shared<const Spec>(std::move(section.content)),section.enabled});
    const auto sections = std::make_shared<const std::vector<AccordionSection>>(std::move(models));
    const auto source = open_keys_;
    const auto mode = mode_;
    const auto policy = policy_;
    const auto callback = std::move(on_change_);
    const auto style = std::move(style_);
    Spec result{[source,sections,mode,policy,callback,style] {
        auto controller = std::make_shared<AccordionController>(source);
        controller->sections = sections; controller->mode = mode; controller->callback = callback;
        controller->selected = 0;
        const auto keys = controller->canonical(source.get());
        const std::weak_ptr<AccordionController> weak = controller;
        for (std::size_t i = 0; i < sections->size(); ++i) {
            auto state = std::make_shared<detail::DisclosureState>();
            state->title = (*sections)[i].title; state->style = style.section; state->policy = policy; state->enabled = (*sections)[i].enabled;
            state->open = std::find(keys.begin(),keys.end(),(*sections)[i].key) != keys.end();
            state->phase = state->open ? 1.0f : 0.0f;
            state->can_write = [weak] { const auto current = weak.lock(); return current && current->mounted && current->source.valid(); };
            state->set_open = [weak,i](bool next) { if (const auto current = weak.lock()) current->write(i,next); };
            state->group_identity = controller.get();
            state->group_selected = [weak,i] { const auto current = weak.lock(); return current && current->selected == i; };
            state->group_select = [weak,i] { if (const auto current = weak.lock()) current->selected = i; };
            state->group_edge = [weak](Key key) { if (const auto current = weak.lock()) current->edge(key); };
            controller->states.push_back(std::move(state));
        }
        if (!sections->empty()) {
            const auto first = std::find_if(sections->begin(),sections->end(),[](const auto& value) { return value.enabled; });
            controller->selected = static_cast<std::size_t>(std::distance(sections->begin(),first));
        }
        return std::make_unique<AccordionComponent>(controller,style);
    },{}};
    result.children_factory = [](Component& component) { return static_cast<AccordionComponent&>(component).children(); };
    return result;
}
} // namespace ui
