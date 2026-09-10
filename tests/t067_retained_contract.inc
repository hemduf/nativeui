#pragma once

#include <nativeui/detail/virtual_list_retained.hpp>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace t067_retained_contract {

struct LifetimeCounters {
    std::size_t constructed{};
    std::size_t destroyed{};
};

class CountedVirtualRow final : public ui::Component {
public:
    explicit CountedVirtualRow(LifetimeCounters& counters) : counters_(&counters) {
        ++counters_->constructed;
    }

    ~CountedVirtualRow() override { ++counters_->destroyed; }

    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override {
        return {100.0f, 20.0f};
    }

    void paint(ui::PaintContext&) const override {}

private:
    LifetimeCounters* counters_{};
};

void behavior_contract() {
    using Runtime = ui::detail::VirtualListRetainedRuntime<int>;

    LifetimeCounters counters;
    auto runtime = std::make_shared<Runtime>(
        20.0f,
        [&counters](const Runtime::Item&) {
            return ui::Spec{
                [&counters] { return std::make_unique<CountedVirtualRow>(counters); },
                {}};
        });

    std::vector<Runtime::Item> items;
    items.reserve(100);
    for (int index = 0; index < 100; ++index) {
        items.emplace_back(index, "row " + std::to_string(index));
    }
    NUI_CHECK(runtime->replace(items));

    ui::UI tree{ui::detail::make_virtual_list_retained_spec(runtime)};
    tree.resize({100.0f, 100.0f});
    ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};

    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(counters.constructed == 7);
    NUI_CHECK(counters.destroyed == 0);

    runtime->scroll().set_offset({0.0f, 400.0f});
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(counters.constructed == 16);
    NUI_CHECK(counters.destroyed == 7);

    runtime->scroll().set_offset({0.0f, 420.0f});
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(counters.constructed == 17);
    NUI_CHECK(counters.destroyed == 8);

    std::swap(items[20], items[21]);
    NUI_CHECK(runtime->replace(items));
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(counters.constructed == 17);
    NUI_CHECK(counters.destroyed == 8);

    items[21] = Runtime::Item{1000, "replacement"};
    NUI_CHECK(runtime->replace(items));
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(counters.constructed == 18);
    NUI_CHECK(counters.destroyed == 9);

    auto invalid = items;
    invalid[22].key = invalid[21].key;
    NUI_CHECK(!runtime->replace(std::move(invalid)));
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(counters.constructed == 18);
    NUI_CHECK(counters.destroyed == 9);
}

} // namespace t067_retained_contract
