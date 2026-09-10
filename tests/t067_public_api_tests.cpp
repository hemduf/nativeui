#include "test_support.hpp"

#include <nativeui/nativeui.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace {

struct RowLifetime {
    std::set<int> live;
};

class TrackedRowComponent final : public ui::Component {
public:
    TrackedRowComponent(int key, std::shared_ptr<RowLifetime> lifetime)
        : key_(key), lifetime_(std::move(lifetime)) {
        lifetime_->live.insert(key_);
    }

    ~TrackedRowComponent() override { lifetime_->live.erase(key_); }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 20.0f};
    }

    void paint(ui::PaintContext&) const override {}

private:
    int key_{};
    std::shared_ptr<RowLifetime> lifetime_;
};

class TrackedRow final {
public:
    TrackedRow(int key, std::shared_ptr<RowLifetime> lifetime)
        : key_(key), lifetime_(std::move(lifetime)) {}

    ui::Spec spec() && {
        const int key = key_;
        auto lifetime = std::move(lifetime_);
        return ui::Spec{
            [key, lifetime = std::move(lifetime)] {
                return std::make_unique<TrackedRowComponent>(key, lifetime);
            },
            {}};
    }

private:
    int key_{};
    std::shared_ptr<RowLifetime> lifetime_;
};

void suite() {
    using VirtualState = ui::VirtualListState<int>;

    // Public controller + exact scroll API remain usable after the ListView
    // builder has been consumed into a retained UI tree.
    {
        ui::State<std::optional<int>> selected{std::nullopt};
        std::size_t row_factory_calls = 0;
        VirtualState state{
            selected,
            20.0f,
            [&row_factory_calls](const VirtualState::Item&) {
                ++row_factory_calls;
                return ui::Spacer{100.0f, 20.0f};
            }};

        std::vector<VirtualState::Item> items;
        items.reserve(100);
        for (int index = 0; index < 100; ++index) {
            items.emplace_back(index, "row " + std::to_string(index));
        }
        NUI_CHECK(state.replace(items));

        ui::UI tree{ui::ListView<int>{state}};
        tree.resize({100.0f, 100.0f});
        ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};

        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(row_factory_calls == 7);

        NUI_CHECK(state.scroll_to_index(20, ui::ScrollAlignment::Start));
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(state.offset().y == 400.0f);

        NUI_CHECK(state.scroll_to_key(21, ui::ScrollAlignment::Center));
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(state.offset().y == 380.0f);

        const auto before_invalid = state.offset();
        NUI_CHECK(!state.scroll_to_index(100));
        NUI_CHECK(!state.scroll_to_key(10000));
        NUI_CHECK(state.offset().x == before_invalid.x);
        NUI_CHECK(state.offset().y == before_invalid.y);
    }

    // T036's one-composite-Tab-stop logical selection/activation contract must
    // survive virtualization. Keyboard navigation may target an unmaterialized
    // key, scroll it into view, and materialize it before the next paint.
    {
        ui::State<std::optional<int>> selected{std::nullopt};
        int activated = -1;
        VirtualState state{
            selected,
            20.0f,
            [](const VirtualState::Item&) { return ui::Spacer{100.0f, 20.0f}; }};

        std::vector<VirtualState::Item> items;
        items.reserve(100);
        for (int index = 0; index < 100; ++index) {
            items.emplace_back(
                index,
                "row " + std::to_string(index),
                index != 1);
        }
        NUI_CHECK(state.replace(items));

        ui::UI tree{std::move(ui::ListView<int>{state})
                        .on_activate([&activated](const int& key) { activated = key; })};
        test::MockPlatform platform;
        tree.resize({100.0f, 100.0f});
        tree.activate(platform);
        ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));

        NUI_CHECK(tree.dispatch(test::key(ui::Key::End), platform) == ui::EventResult::Handled);
        NUI_CHECK(selected.get() && *selected.get() == 99);
        NUI_CHECK(state.offset().y == 1900.0f);
        NUI_CHECK(renderer.render(tree));

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Home), platform) == ui::EventResult::Handled);
        NUI_CHECK(selected.get() && *selected.get() == 0);
        NUI_CHECK(state.offset().y == 0.0f);

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Down), platform) == ui::EventResult::Handled);
        NUI_CHECK(selected.get() && *selected.get() == 2);

        // Application-originated selection retains the T036 reveal behavior.
        selected.set(50);
        NUI_CHECK(state.offset().y == 920.0f);
        NUI_CHECK(renderer.render(tree));

        NUI_CHECK(tree.dispatch(test::key(ui::Key::Enter), platform) == ui::EventResult::Handled);
        NUI_CHECK(activated == 50);
    }

    // Pointer capture belongs to the materialized logical row, not the stable
    // ListView shell. That keeps one off-range captured row alive, and lets T058
    // deliver PointerCancel before destroying a captured row removed by a data
    // replacement.
    {
        ui::State<std::optional<int>> selected{std::nullopt};
        auto lifetime = std::make_shared<RowLifetime>();
        VirtualState state{
            selected,
            20.0f,
            [lifetime](const VirtualState::Item& item) {
                return TrackedRow{item.key, lifetime};
            }};

        std::vector<VirtualState::Item> items;
        items.reserve(100);
        for (int index = 0; index < 100; ++index) {
            items.emplace_back(index, "row " + std::to_string(index));
        }
        NUI_CHECK(state.replace(items));

        ui::UI tree{ui::ListView<int>{state}};
        test::MockPlatform platform;
        tree.resize({100.0f, 100.0f});
        tree.activate(platform);
        ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(lifetime->live.size() == 7);
        NUI_CHECK(lifetime->live.contains(0));

        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 10.0f), platform) ==
                  ui::EventResult::Handled);
        NUI_CHECK(!selected.get());

        NUI_CHECK(state.scroll_to_index(50, ui::ScrollAlignment::Start));
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(lifetime->live.contains(0));
        NUI_CHECK(lifetime->live.size() <= 10);

        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Handled);
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(!lifetime->live.contains(0));
        NUI_CHECK(!selected.get());

        NUI_CHECK(state.scroll_to_index(0, ui::ScrollAlignment::Start));
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(lifetime->live.contains(0));
        NUI_CHECK(tree.dispatch(
                      test::pointer(ui::InputType::PointerDown, 20.0f, 10.0f), platform) ==
                  ui::EventResult::Handled);

        std::vector<VirtualState::Item> without_zero;
        without_zero.reserve(99);
        for (int index = 1; index < 100; ++index) {
            without_zero.emplace_back(index, "row " + std::to_string(index));
        }
        NUI_CHECK(state.replace(std::move(without_zero)));
        NUI_CHECK(renderer.render(tree));
        NUI_CHECK(!lifetime->live.contains(0));
        NUI_CHECK(tree.cancel_pointer(platform) == ui::EventResult::Ignored);
    }
}

} // namespace

int main() { return test::run("t067_public_api", &suite); }
