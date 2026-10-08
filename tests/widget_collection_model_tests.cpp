#include "test_support.hpp"
#include <nativeui/collection_model.hpp>

#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>

namespace {
struct Probe {
    int comparisons{};
    bool fail{};
    std::function<void()> nested;
};
struct EqualityKey {
    int value{};
    std::shared_ptr<Probe> probe;
    bool operator==(const EqualityKey &other) const {
        const auto state = probe;
        if (state) {
            ++state->comparisons;
            if (state->fail)
                throw std::runtime_error("key equality fault");
            auto callback = std::exchange(state->nested, {});
            if (callback)
                callback();
        }
        return value == other.value;
    }
};
template <class Key> auto keys(std::vector<Key> values) {
    return std::make_shared<const ui::detail::TypedCollectionKeys<Key>>(std::move(values));
}
void selection_normalizes_once_and_keeps_cursor_keys() {
    ui::State<ui::SelectionSnapshot<int>> source{{}};
    ui::Selection<int> selection{source};
    int calls{};
    const auto subscription = source.observe([&](const auto &) { ++calls; });
    NUI_CHECK(selection.set({{3, 1, 3, 2, 1}, 99, 100}));
    NUI_CHECK(selection.snapshot() == (ui::SelectionSnapshot<int>{{3, 1, 2}, 99, 100}));
    NUI_CHECK(calls == 1 && !selection.set({{3, 1, 2}, 99, 100}));
    NUI_CHECK(calls == 1);
    auto owner = std::make_unique<ui::State<ui::SelectionSnapshot<int>>>(
        ui::SelectionSnapshot<int>{{7}, 7, 7});
    ui::Selection<int> expired{owner->binding()};
    owner.reset();
    NUI_CHECK(!expired.valid() && !expired.set({{8}, 8, 8}));
    NUI_CHECK(expired.snapshot().selected == std::vector<int>{7});
}
void selection_faults_do_not_replay_or_publish_partial_canonicalization() {
    auto probe = std::make_shared<Probe>();
    const EqualityKey first{1, probe}, second{2, probe};
    ui::State<ui::SelectionSnapshot<EqualityKey>> source{{{first}, first, first}};
    ui::Selection<EqualityKey> selection{source};
    probe->fail = true;
    bool caught{};
    try {
        (void)selection.set({{second, second}, second, second});
    } catch (const std::runtime_error &) {
        caught = true;
    }
    probe->fail = false;
    NUI_CHECK(caught && source.get().selected.front().value == 1);
    int notifications{};
    bool throw_once = true;
    const auto subscription = source.observe([&](const auto &) {
        ++notifications;
        if (std::exchange(throw_once, false))
            throw std::runtime_error("observer fault");
    });
    caught = false;
    try {
        (void)selection.set({{second, second}, second, second});
    } catch (const std::runtime_error &) {
        caught = true;
    }
    NUI_CHECK(caught && source.get().selected.size() == 1 &&
              source.get().selected.front().value == 2);
    NUI_CHECK(!selection.set({{second}, second, second}) && notifications == 1);
}
void tokens_preserve_retained_keys_and_never_resurrect_removed_identity() {
    ui::detail::CollectionTokens tokens;
    NUI_CHECK(tokens.commit(tokens.prepare(keys<int>({10, 20, 30}))));
    const auto first = tokens.snapshot();
    NUI_CHECK(first->generation == 1 && first->encoded && first->tokens.size() == 3);
    NUI_CHECK(tokens.commit(tokens.prepare(keys<int>({30, 10}))));
    const auto second = tokens.snapshot();
    NUI_CHECK(second->tokens[0] == first->tokens[2] && second->tokens[1] == first->tokens[0]);
    NUI_CHECK(tokens.commit(tokens.prepare(keys<int>({30, 20, 10}))));
    const auto third = tokens.snapshot();
    NUI_CHECK(third->tokens[1] != first->tokens[1]);
    NUI_CHECK(ui::detail::CollectionTokens::index_for_token(*third, first->tokens[0]) == 2);
    NUI_CHECK(!ui::detail::CollectionTokens::index_for_token(*third, first->tokens[1]));
    NUI_CHECK(first->tokens.size() == 3 && first->generation == 1);
    NUI_CHECK(!ui::detail::CollectionTokens::index_for_token(*third,
                                                             ui::detail::kInvalidCollectionToken));
}
void duplicate_and_equality_faults_leave_the_authoritative_generation_intact() {
    ui::detail::CollectionTokens tokens;
    NUI_CHECK(tokens.commit(tokens.prepare(keys<std::string>({"a", "b"}))));
    const auto previous = tokens.snapshot();
    bool caught{};
    try {
        (void)tokens.prepare(keys<std::string>({"a", "a"}));
    } catch (const std::invalid_argument &) {
        caught = true;
    }
    NUI_CHECK(caught && tokens.snapshot() == previous);
    auto probe = std::make_shared<Probe>();
    NUI_CHECK(tokens.commit(tokens.prepare(keys<EqualityKey>({{1, probe}, {2, probe}}))));
    const auto generic = tokens.snapshot();
    NUI_CHECK(!generic->encoded);
    probe->fail = true;
    caught = false;
    try {
        (void)tokens.prepare(keys<EqualityKey>({{2, probe}, {3, probe}}));
    } catch (const std::runtime_error &) {
        caught = true;
    }
    probe->fail = false;
    NUI_CHECK(caught && tokens.snapshot() == generic);
    NUI_CHECK(tokens.commit(tokens.prepare(keys<EqualityKey>({{2, probe}, {3, probe}}))));
    NUI_CHECK(tokens.snapshot()->tokens[0] == generic->tokens[1]);
}
void reentrant_preparation_is_rejected_at_commit() {
    ui::detail::CollectionTokens tokens;
    auto probe = std::make_shared<Probe>();
    NUI_CHECK(tokens.commit(tokens.prepare(keys<EqualityKey>({{1, probe}, {2, probe}}))));
    probe->nested = [&] {
        NUI_CHECK(tokens.commit(tokens.prepare(keys<EqualityKey>({{9, probe}}))));
    };
    auto stale = tokens.prepare(keys<EqualityKey>({{2, probe}, {3, probe}}));
    NUI_CHECK(!tokens.commit(std::move(stale)));
    const auto committed = tokens.snapshot();
    NUI_CHECK(committed->generation == 2 && committed->tokens.size() == 1);
    NUI_CHECK(tokens.commit(tokens.prepare(keys<EqualityKey>({{9, probe}, {3, probe}}))));
    NUI_CHECK(tokens.snapshot()->tokens[0] == committed->tokens[0]);
}
void independent_registries_do_not_share_generations_or_mutable_snapshots() {
    ui::detail::CollectionTokens first, second;
    NUI_CHECK(first.commit(first.prepare(keys<int>({1, 2}))));
    NUI_CHECK(second.snapshot()->generation == 0 && second.snapshot()->tokens.empty());
    NUI_CHECK(second.commit(second.prepare(keys<int>({2, 1}))));
    NUI_CHECK(first.snapshot() != second.snapshot());
    NUI_CHECK(first.snapshot()->tokens[0] == 1 && second.snapshot()->tokens[1] == 2);
}
void key_lookup_pins_the_old_generation_across_reentrant_publication() {
    ui::detail::CollectionTokens tokens;
    NUI_CHECK(tokens.commit(tokens.prepare(keys<int>({10, 20, 30}))));
    NUI_CHECK(ui::detail::collection_index_of_key(tokens.snapshot(), 20) ==
              std::optional<std::size_t>{1});
    NUI_CHECK(!ui::detail::collection_index_of_key(tokens.snapshot(), 99));
    const auto probe = std::make_shared<Probe>();
    NUI_CHECK(
        tokens.commit(tokens.prepare(keys<EqualityKey>({{1, probe}, {2, probe}, {3, probe}}))));
    const auto old = tokens.snapshot();
    probe->nested = [&] {
        NUI_CHECK(tokens.commit(tokens.prepare(keys<EqualityKey>({{9, probe}}))));
    };
    NUI_CHECK(ui::detail::collection_index_of_key(tokens.snapshot(), EqualityKey{3, probe}) ==
              std::optional<std::size_t>{2});
    NUI_CHECK(tokens.snapshot()->generation == old->generation + 1 &&
              tokens.snapshot()->tokens.size() == 1);
    NUI_CHECK(old->tokens.size() == 3);
}
void variable_height_prefix_queries_and_corrections_keep_the_anchor() {
    ui::detail::CollectionHeightIndex heights;
    heights.replace({10.0, 20.0, 30.0, 40.0});
    NUI_CHECK(heights.index_at(0.0) == 0 && heights.index_at(10.0) == 1 &&
              heights.index_at(29.5) == 1);
    NUI_CHECK(heights.index_at(100.0) == 3 && heights.index_at(-1.0) == 0);
    NUI_CHECK(heights.range(10.0, 20.0, 0) == (ui::detail::CollectionRange{1, 2}));
    NUI_CHECK(heights.range(10.0, 20.0, 1) == (ui::detail::CollectionRange{0, 3}));
    const std::size_t anchor = 2;
    const double inset = 7.0;
    const auto previous_offset = heights.prefix_sum(anchor) + inset;
    NUI_CHECK(heights.set_height(0, 15.0));
    const auto next_offset = heights.prefix_sum(anchor) + inset;
    NUI_CHECK(next_offset == previous_offset + 5.0 && heights.index_at(next_offset) == anchor);
    NUI_CHECK(heights.total_height() == 105.0 && heights.prefix_sum(4) == 105.0);
    NUI_CHECK(!heights.set_height(1, 0.0) &&
              !heights.set_height(1, std::numeric_limits<double>::infinity()));
    NUI_CHECK(heights.total_height() == 105.0);
    bool caught{};
    try {
        heights.replace({20.0, -1.0});
    } catch (const std::invalid_argument &) {
        caught = true;
    }
    NUI_CHECK(caught && heights.total_height() == 105.0 && heights.height_at(0) == 15.0);
    heights.replace({std::numeric_limits<double>::max(), 1.0});
    NUI_CHECK(heights.set_height(0, 1.0) && heights.total_height() == 2.0 &&
              heights.prefix_sum(1) == 1.0);
    heights.replace({});
    NUI_CHECK(!heights.index_at(0.0) &&
              heights.range(0.0, 100.0, 2) == ui::detail::CollectionRange{});
}
void immutable_height_snapshots_keep_old_geometry_after_corrections_and_replacements() {
    ui::detail::CollectionHeightIndex heights;
    heights.replace({10.0, 20.0, 30.0, 40.0, 50.0});
    const auto old = heights.snapshot();
    NUI_CHECK(heights.set_height(1, 50.0));
    const auto corrected = heights.snapshot();
    NUI_CHECK(old.total_height() == 150.0 && old.height_at(1) == 20.0 && old.prefix_sum(2) == 30.0);
    NUI_CHECK(corrected.total_height() == 180.0 && corrected.height_at(1) == 50.0 &&
              corrected.prefix_sum(2) == 60.0);
    NUI_CHECK(old.index_at(31.0) == std::optional<std::size_t>{2});
    NUI_CHECK(corrected.index_at(31.0) == std::optional<std::size_t>{1});
    heights.replace({});
    NUI_CHECK(old.size() == 5 && old.range(0.0, 30.0, 0) == (ui::detail::CollectionRange{0, 2}));
    NUI_CHECK(corrected.size() == 5 &&
              corrected.range(0.0, 30.0, 0) == (ui::detail::CollectionRange{0, 2}));
    heights.replace(std::vector<double>(1000000, 24.0));
    const auto uniform = heights.snapshot();
    NUI_CHECK(uniform.size() == 1000000 && uniform.total_height() == 24000000.0);
    NUI_CHECK(uniform.index_at(12000000.0) == std::optional<std::size_t>{500000});
    NUI_CHECK(heights.set_height(3, 48.0));
    const auto updated = heights.snapshot();
    NUI_CHECK(updated.prefix_sum(4) == 120.0 && uniform.prefix_sum(4) == 96.0);
    NUI_CHECK(heights.set_height(3, 24.0));
    NUI_CHECK(heights.snapshot().prefix_sum(4) == 96.0);
}
void committed_typeahead_folds_ascii_and_keeps_utf8_exact() {
    ui::detail::CollectionTypeahead search;
    using Time = ui::detail::CollectionTypeahead::Time;
    const std::vector<ui::detail::CollectionSearchItem> rows{{"Alpha", true},  {"Alpine", true},
                                                             {"Arc", false},   {"Bravo", true},
                                                             {"Éclair", true}, {"école", true}};
    auto active = search.feed("a", Time{0}, rows, {});
    NUI_CHECK(active == 0 && search.buffer() == "a");
    active = search.feed("A", Time{20}, rows, active);
    NUI_CHECK(active == 1 && search.buffer() == "A");
    active = search.feed("a", Time{30}, rows, active);
    NUI_CHECK(active == 0);
    active = search.feed("l", Time{40}, rows, active);
    NUI_CHECK(active == 0 && search.buffer() == "al");
    active = search.feed("p", Time{740}, rows, active);
    NUI_CHECK(!active && search.buffer() == "p");
    search.clear();
    active = search.feed("É", Time{900}, rows, {});
    NUI_CHECK(active == 4);
    search.clear();
    active = search.feed("é", Time{901}, rows, {});
    NUI_CHECK(active == 5);
    const auto previous = search.buffer();
    NUI_CHECK(!search.feed(std::string_view{"\xc0\xaf", 2}, Time{902}, rows, active) &&
              search.buffer() == previous);
    search.clear();
    NUI_CHECK(!search.feed("", Time{0}, rows, {}));
    NUI_CHECK(search.feed("br", Time::min(), rows, {}) == 3);
    NUI_CHECK(search.feed("A", Time::max(), rows, {}) == 0 && search.buffer() == "A");
    search.clear();
    NUI_CHECK(!search.feed("a", Time{0}, {}, {}));
}
void multiple_selection_ranges_preserve_active_and_skip_ineligible_rows() {
    using Gesture = ui::detail::CollectionSelectionGesture;
    const std::vector<ui::detail::CollectionToken> order{10, 20, 30, 40, 50};
    const std::vector<bool> eligible{true, false, true, true, true};
    const ui::detail::CollectionSelection current{{10}, 10, 10};
    auto selected = ui::detail::resolve_collection_selection(order, eligible, current, true, 40,
                                                             Gesture::Extend);
    NUI_CHECK(selected == (ui::detail::CollectionSelection{{10, 30, 40}, 40, 10}));
    selected = ui::detail::resolve_collection_selection(order, eligible, selected, true, 50,
                                                        Gesture::MoveActive);
    NUI_CHECK(selected == (ui::detail::CollectionSelection{{10, 30, 40}, 50, 10}));
    selected = ui::detail::resolve_collection_selection(order, eligible, selected, true, 30,
                                                        Gesture::Toggle);
    NUI_CHECK(selected == (ui::detail::CollectionSelection{{10, 40}, 30, 30}));
    selected = ui::detail::resolve_collection_selection(order, eligible, selected, true, 50,
                                                        Gesture::AddRange);
    NUI_CHECK(selected == (ui::detail::CollectionSelection{{10, 30, 40, 50}, 50, 30}));
    const auto single = ui::detail::resolve_collection_selection(
        order, eligible, {{50, 30, 10, 99}, 99, 99}, false, 40, Gesture::MoveActive);
    NUI_CHECK(single == (ui::detail::CollectionSelection{{10}, 40, {}}));
    const auto all =
        ui::detail::resolve_collection_selection(order, eligible, {}, true, {}, Gesture::SelectAll);
    NUI_CHECK(all == (ui::detail::CollectionSelection{{10, 30, 40, 50}, 10, 10}));
}
void suite() {
    selection_normalizes_once_and_keeps_cursor_keys();
    selection_faults_do_not_replay_or_publish_partial_canonicalization();
    tokens_preserve_retained_keys_and_never_resurrect_removed_identity();
    duplicate_and_equality_faults_leave_the_authoritative_generation_intact();
    reentrant_preparation_is_rejected_at_commit();
    independent_registries_do_not_share_generations_or_mutable_snapshots();
    key_lookup_pins_the_old_generation_across_reentrant_publication();
    variable_height_prefix_queries_and_corrections_keep_the_anchor();
    immutable_height_snapshots_keep_old_geometry_after_corrections_and_replacements();
    committed_typeahead_folds_ascii_and_keeps_utf8_exact();
    multiple_selection_ranges_preserve_active_and_skip_ineligible_rows();
}
} // namespace
int main() { return test::run("widget_collection_model", &suite); }
