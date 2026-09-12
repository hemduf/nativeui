#include <nativeui/component_tree.hpp>
#include <nativeui/detail/semantic_tree.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string{"line "} + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define T068_CHECK(expr) check(static_cast<bool>(expr), #expr, __LINE__)

class SemanticProbe final : public ui::Component {
public:
    SemanticProbe(ui::SemanticInfo info, ui::ComponentAvailability availability = {})
        : info_(std::move(info)), availability_(availability) {}

    [[nodiscard]] ui::SemanticInfo semantics() const override { return info_; }
    [[nodiscard]] ui::ComponentAvailability local_availability() const noexcept override {
        return availability_;
    }
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {};
    }
    void paint(ui::PaintContext&) const override {}

private:
    ui::SemanticInfo info_;
    ui::ComponentAvailability availability_;
};

ui::SemanticInfo info(ui::SemanticRole role, std::string name = {}) {
    ui::SemanticInfo result;
    result.role = role;
    result.name = std::move(name);
    return result;
}

std::unique_ptr<ui::Node> node(ui::NodeId id,
                               ui::SemanticInfo semantic,
                               ui::ComponentAvailability availability = {}) {
    auto result = std::make_unique<ui::Node>();
    result->id = id;
    result->component = std::make_unique<SemanticProbe>(std::move(semantic), availability);
    return result;
}

void append(ui::Node& parent, std::unique_ptr<ui::Node> child) {
    child->parent = &parent;
    parent.children.push_back(std::move(child));
}

const ui::SemanticNodeSnapshot& require_node(const ui::SemanticTreeSnapshot& snapshot,
                                             ui::SemanticId id) {
    for (const auto& item : snapshot.nodes) {
        if (item.id == id) return item;
    }
    throw std::runtime_error("semantic node not found");
}

void semantic_tree_flattens_none_and_honors_effective_availability() {
    auto root_info = info(ui::SemanticRole::Group, "Root");
    auto root = node(1, std::move(root_info));
    root->bounds = {0.0f, 0.0f, 200.0f, 100.0f};

    auto wrapper_info = info(ui::SemanticRole::None);
    wrapper_info.description = "Inherited help";
    auto wrapper = node(2, std::move(wrapper_info));
    wrapper->bounds = {0.0f, 0.0f, 80.0f, 20.0f};

    auto button_info = info(ui::SemanticRole::Button, "Apply");
    button_info.focusable = true;
    button_info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    auto button = node(3, std::move(button_info));
    button->bounds = {0.0f, 0.0f, 80.0f, 20.0f};
    append(*wrapper, std::move(button));
    append(*root, std::move(wrapper));

    ui::ComponentAvailability hidden;
    hidden.visibility = ui::VisibilityMode::Hidden;
    auto hidden_text = node(4, info(ui::SemanticRole::Text, "Hidden"), hidden);
    hidden_text->bounds = {0.0f, 20.0f, 80.0f, 20.0f};
    append(*root, std::move(hidden_text));

    auto text = node(5, info(ui::SemanticRole::Text, "Visible"));
    text->bounds = {0.0f, 40.0f, 80.0f, 20.0f};
    append(*root, std::move(text));

    ui::ComponentAvailability disabled;
    disabled.enabled = false;
    auto disabled_info = info(ui::SemanticRole::Button, "Disabled");
    disabled_info.focusable = true;
    disabled_info.actions = {ui::SemanticAction::Activate, ui::SemanticAction::Focus};
    auto disabled_button = node(6, std::move(disabled_info), disabled);
    disabled_button->bounds = {0.0f, 60.0f, 80.0f, 20.0f};
    append(*root, std::move(disabled_button));

    ui::Node* root_ptr = root.get();
    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(*root_ptr, nullptr);
    T068_CHECK(snapshot.root == 1);
    T068_CHECK(snapshot.nodes.size() == 4);

    const auto& semantic_root = require_node(snapshot, 1);
    T068_CHECK(semantic_root.parent == ui::kInvalidSemanticId);
    T068_CHECK(semantic_root.children ==
               std::vector<ui::SemanticId>({3, 5, 6}));

    const auto& button_node = require_node(snapshot, 3);
    T068_CHECK(button_node.parent == 1);
    T068_CHECK(button_node.info.description == "Inherited help");
    T068_CHECK(button_node.info.enabled);
    T068_CHECK(button_node.info.actions.size() == 2);

    const auto& text_node = require_node(snapshot, 5);
    T068_CHECK(text_node.parent == 1);
    T068_CHECK(text_node.info.name == "Visible");

    const auto& disabled_node = require_node(snapshot, 6);
    T068_CHECK(!disabled_node.info.enabled);
    T068_CHECK(!disabled_node.info.focusable);
    T068_CHECK(disabled_node.info.actions.empty());

    for (const auto& item : snapshot.nodes) {
        T068_CHECK(item.id != 2);
        T068_CHECK(item.id != 4);
    }
}

} // namespace

int main() {
    try {
        semantic_tree_flattens_none_and_honors_effective_availability();
        std::cout << "PASS t068 semantic tree\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL t068 semantic tree: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
