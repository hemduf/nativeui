#pragma once

#include <nativeui/collapsible.hpp>

#include <memory>
#include <vector>

namespace ui {

enum class AccordionMode { Multiple, Single };

struct AccordionStyle {
    CollapsibleStyle section{};
    float padding{};
    float corner_radius{4.0f};
    float border_width{1.0f};
    float separator_width{1.0f};
    std::optional<Color> background{};
    std::optional<Color> border_color{};
    std::optional<Color> separator_color{};
    std::string accessible_name{};
};

class Accordion {
public:
    explicit Accordion(Binding<std::vector<std::string>> open_keys);
    explicit Accordion(State<std::vector<std::string>>& open_keys);
    template <class Child>
    Accordion&& section(std::string key, std::string title, Child&& child, bool enabled = true) && {
        sections_.push_back({std::move(key),std::move(title),make_spec(std::forward<Child>(child)),enabled});
        return std::move(*this);
    }
    Accordion&& mode(AccordionMode value) &&;
    Accordion&& content_policy(DisclosureContentPolicy value) &&;
    Accordion&& on_change(std::function<void(const std::vector<std::string>&)> callback) &&;
    Accordion&& style(AccordionStyle value) &&;
    Spec spec() &&;

private:
    struct Section {
        std::string key;
        std::string title;
        Spec content;
        bool enabled{true};
    };
    Binding<std::vector<std::string>> open_keys_;
    std::vector<Section> sections_;
    AccordionMode mode_{AccordionMode::Multiple};
    DisclosureContentPolicy policy_{DisclosureContentPolicy::Retain};
    std::function<void(const std::vector<std::string>&)> on_change_;
    AccordionStyle style_;
};

} // namespace ui
