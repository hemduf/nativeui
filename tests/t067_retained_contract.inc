#pragma once

#include <nativeui/detail/virtual_list_retained.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
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

class SpecRoot {
public:
    explicit SpecRoot(ui::Spec value) : value_(std::move(value)) {}

    [[nodiscard]] ui::Spec spec() && { return std::move(value_); }

private:
    ui::Spec value_;
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

    ui::UI tree{SpecRoot{ui::detail::make_virtual_list_retained_spec(runtime)}};
    tree.resize({100.0f, 100.0f});
    ui::HeadlessRenderer renderer{{100.0f, 100.0f}, 1.0f};

    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(runtime->scroll().viewport_size().h == 100.0f);
    NUI_CHECK(runtime->scroll().content_size().h == 2000.0f);
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

    NUI_CHECK(runtime->scroll_to_index(30, ui::ScrollAlignment::Start));
    NUI_CHECK(runtime->scroll().offset().y == 600.0f);
    NUI_CHECK(runtime->scroll_to_key(31, ui::ScrollAlignment::Nearest));
    NUI_CHECK(runtime->scroll().offset().y == 600.0f);

    const auto before_invalid_scroll = runtime->scroll().offset();
    NUI_CHECK(!runtime->scroll_to_index(100, ui::ScrollAlignment::End));
    NUI_CHECK(!runtime->scroll_to_key(10000, ui::ScrollAlignment::Center));
    NUI_CHECK(runtime->scroll().offset().x == before_invalid_scroll.x);
    NUI_CHECK(runtime->scroll().offset().y == before_invalid_scroll.y);

    // T127: exercise the retained VirtualList as the first ScrollState observer,
    // then throw from a later observer after mutating both the dataset and the
    // pending scroll transaction. The throwing pass must keep the committed
    // offset, discard recursive pending work, honor listener removal/addition,
    // and leave materialization coherent for the next scroll mutation.
    {
        LifetimeCounters reentrant_counters;
        auto reentrant_runtime = std::make_shared<Runtime>(
            20.0f,
            [&reentrant_counters](const Runtime::Item&) {
                return ui::Spec{
                    [&reentrant_counters] {
                        return std::make_unique<CountedVirtualRow>(reentrant_counters);
                    },
                    {}};
            });

        std::vector<Runtime::Item> initial;
        initial.reserve(100);
        for (int index = 0; index < 100; ++index) {
            initial.emplace_back(index, "initial " + std::to_string(index));
        }
        NUI_CHECK(reentrant_runtime->replace(std::move(initial)));

        ui::UI reentrant_tree{
            SpecRoot{ui::detail::make_virtual_list_retained_spec(reentrant_runtime)}};
        reentrant_tree.resize({100.0f, 100.0f});
        ui::HeadlessRenderer reentrant_renderer{{100.0f, 100.0f}, 1.0f};
        NUI_CHECK(reentrant_renderer.render(reentrant_tree));

        const auto generation_before = reentrant_runtime->dataset_generation();
        bool throw_once = true;
        bool replacement_succeeded = false;
        int mutator_calls = 0;
        int suffix_calls = 0;
        int added_calls = 0;
        ui::ScrollState::Subscription suffix;
        ui::ScrollState::Subscription added;

        auto mutator = reentrant_runtime->scroll().observe([&](ui::Point) {
            ++mutator_calls;
            if (!throw_once) return;
            throw_once = false;

            // Removal before the suffix turn is immediate and reset remains
            // idempotent even while dispatch is active.
            suffix.reset();
            suffix.reset();

            // A listener inserted mid-pass starts only on a later mutation.
            added = reentrant_runtime->scroll().observe(
                [&](ui::Point) { ++added_calls; });

            std::vector<Runtime::Item> replacement;
            replacement.reserve(100);
            for (int index = 0; index < 100; ++index) {
                replacement.emplace_back(
                    1000 + index,
                    "replacement " + std::to_string(index));
            }
            replacement_succeeded = reentrant_runtime->replace(std::move(replacement));

            // This recursive write is pending only; the exception policy must
            // discard it rather than silently applying/retrying it after unwind.
            reentrant_runtime->scroll().scroll_by({0.0f, 100.0f});
            throw std::runtime_error{"virtual-list scroll observer failure"};
        });
        suffix = reentrant_runtime->scroll().observe(
            [&](ui::Point) { ++suffix_calls; });

        bool threw = false;
        try {
            reentrant_runtime->scroll().set_offset({0.0f, 100.0f});
        } catch (const std::runtime_error&) {
            threw = true;
        }

        NUI_CHECK(threw);
        NUI_CHECK(replacement_succeeded);
        NUI_CHECK(mutator_calls == 1);
        NUI_CHECK(suffix_calls == 0);
        NUI_CHECK(added_calls == 0);
        NUI_CHECK(!suffix.active());
        NUI_CHECK(added.active());
        NUI_CHECK(reentrant_runtime->dataset_generation() == generation_before + 1);
        NUI_CHECK(reentrant_runtime->scroll().offset().y == 100.0f);

        const auto keys_after_throw = reentrant_runtime->desired_keys();
        NUI_CHECK(!keys_after_throw.empty());
        NUI_CHECK(keys_after_throw.front() == "virtual-list:i:1003");
        NUI_CHECK(keys_after_throw.back() == "virtual-list:i:1011");

        // A later mutation remains usable: the former middle observer is not
        // automatically retried by the throw itself, the newly-added listener
        // now participates, and materialization follows the replacement dataset.
        reentrant_runtime->scroll().scroll_by({0.0f, 100.0f});
        NUI_CHECK(mutator_calls == 2);
        NUI_CHECK(suffix_calls == 0);
        NUI_CHECK(added_calls == 1);
        NUI_CHECK(reentrant_runtime->scroll().offset().y == 200.0f);
        NUI_CHECK(reentrant_renderer.render(reentrant_tree));

        const auto keys_after_recovery = reentrant_runtime->desired_keys();
        NUI_CHECK(!keys_after_recovery.empty());
        NUI_CHECK(keys_after_recovery.front() == "virtual-list:i:1008");
        NUI_CHECK(keys_after_recovery.back() == "virtual-list:i:1016");

        auto moved_added = std::move(added);
        NUI_CHECK(!added.active());
        NUI_CHECK(moved_added.active());
        moved_added.reset();
        moved_added.reset();
        NUI_CHECK(!moved_added.active());
        NUI_CHECK(mutator.active());
    }
}

} // namespace t067_retained_contract