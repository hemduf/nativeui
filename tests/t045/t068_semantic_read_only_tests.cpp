#include "test_support.hpp"

#include <nativeui/detail/semantic_tree.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

class ReadOnlySemanticProbe final : public ui::Component {
public:
    [[nodiscard]] ui::ComponentAvailability local_availability() const noexcept override {
        ui::ComponentAvailability availability;
        availability.read_only = true;
        return availability;
    }

    [[nodiscard]] ui::SemanticInfo semantics() const override {
        ui::SemanticInfo info;
        info.role = ui::SemanticRole::Slider;
        info.name = "Read-only value";
        info.focusable = true;
        info.actions = {
            ui::SemanticAction::Increment,
            ui::SemanticAction::Decrement,
            ui::SemanticAction::SetValue,
            ui::SemanticAction::Focus,
        };
        return info;
    }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {};
    }

    void paint(ui::PaintContext&) const override {}
};

void read_only_snapshot_does_not_advertise_mutation_actions() {
    auto root = std::make_unique<ui::Node>();
    root->id = 1;
    root->component = std::make_unique<ReadOnlySemanticProbe>();

    auto* root_ptr = root.get();
    ui::Tree tree{std::move(root)};
    tree.mount();

    const auto snapshot = ui::detail::build_semantic_tree_snapshot(*root_ptr);
    NUI_CHECK(snapshot.nodes.size() == 1);
    const auto& info = snapshot.nodes.front().info;
    NUI_CHECK(info.read_only);
    NUI_CHECK(info.focusable);
    NUI_CHECK(info.actions == std::vector<ui::SemanticAction>{ui::SemanticAction::Focus});
}

void suite() {
    read_only_snapshot_does_not_advertise_mutation_actions();
}

} // namespace

int main() { return test::run("t068_semantic_read_only", &suite); }
