#include <nativeui/nativeui.hpp>

#include <concepts>
#include <type_traits>

namespace {

template <class T>
concept HasStandaloneRun = requires(T& value) {
    { value.run() } -> std::same_as<int>;
};

template <class T>
concept HasStandalonePoll = requires(T& value) {
    { value.poll(-1.0) } -> std::same_as<bool>;
};

static_assert(!std::is_constructible_v<ui::StandaloneWindow, ui::UI&, ui::WindowDesc>);
static_assert(!HasStandaloneRun<ui::StandaloneWindow>);
static_assert(!HasStandalonePoll<ui::StandaloneWindow>);

static_assert(std::is_constructible_v<
              ui::StandaloneWindow, ui::Application&, ui::UI&, ui::WindowDesc>);
static_assert(requires(ui::Application& app) {
    { app.run() } -> std::same_as<int>;
    { app.poll(0.0) } -> std::same_as<bool>;
});
static_assert(requires(ui::EmbeddedView& view) {
    { view.poll() } -> std::same_as<bool>;
});

} // namespace

int main() { return 0; }
