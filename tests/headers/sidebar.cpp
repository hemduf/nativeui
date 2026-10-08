#include <nativeui/sidebar.hpp>
namespace {
[[maybe_unused]] void probe() {
    ui::State<std::optional<int>> value{std::nullopt};
    ui::State<bool> open{true};
    auto spec =
        std::move(ui::Sidebar<int>{value}.section("one", "One", open).item(1, "Item")).spec();
    (void)spec;
}
} // namespace
