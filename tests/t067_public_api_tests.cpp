#include "test_support.hpp"

#include <nativeui/nativeui.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace {

void suite() {
    ui::State<std::optional<int>> selected{std::nullopt};
    using VirtualState = ui::VirtualListState<int>;

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

} // namespace

int main() { return test::run("t067_public_api", &suite); }
