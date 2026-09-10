#include <nativeui/detail/virtual_list_window.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition) {
    if (!condition) ++failures;
}

void viewport_bounded_factory_contract() {
    using Model = ui::detail::VirtualListDatasetModel<int>;
    using Window = ui::detail::VirtualListMaterializationWindow<int, int>;

    std::vector<Model::Item> items;
    for (int index = 0; index < 100; ++index) {
        items.emplace_back(index, "row " + std::to_string(index));
    }

    Model model;
    check(model.replace(std::move(items)));

    std::size_t factory_calls = 0;
    Window window{model, 20.0f, [&](const Model::Item& item) {
        ++factory_calls;
        return item.key;
    }};

    check(window.update(0.0f, 100.0f));
    check(window.indices().size() == 7);
    check(window.indices().front() == 0 && window.indices().back() == 6);
    check(window.keys().size() == 7);
    check(window.items().size() == 7);
    check(*window.items().front().payload == 0);
    check(*window.items().back().payload == 6);
    check(factory_calls == 7);

    check(window.update(400.0f, 100.0f));
    check(window.indices().size() == 9);
    check(window.indices().front() == 18 && window.indices().back() == 26);
    check(factory_calls == 16);

    check(window.update(420.0f, 100.0f));
    check(window.indices().size() == 9);
    check(window.indices().front() == 19 && window.indices().back() == 27);
    check(factory_calls == 17);

    check(window.update(420.0f, 100.0f));
    check(factory_calls == 17);

    check(window.update(420.0f, 100.0f, 2, 0, 99));
    check(window.indices().size() == 11);
    check(window.indices().front() == 0 && window.indices().back() == 99);
    check(factory_calls == 19);
}

void keyed_reorder_reuses_materialized_payloads() {
    using Model = ui::detail::VirtualListDatasetModel<int>;
    using Window = ui::detail::VirtualListMaterializationWindow<int, int>;

    Model model;
    std::vector<Model::Item> items;
    for (int index = 0; index < 12; ++index) {
        items.emplace_back(index, "row " + std::to_string(index));
    }
    check(model.replace(items));

    std::size_t factory_calls = 0;
    Window window{model, 20.0f, [&](const Model::Item& item) {
        ++factory_calls;
        return item.key;
    }};

    check(window.update(40.0f, 100.0f));
    const auto before_keys = window.keys();
    const auto before_calls = factory_calls;

    std::swap(items[3], items[4]);
    check(model.replace(items));
    check(window.update(40.0f, 100.0f));
    check(window.keys() != before_keys);
    check(factory_calls == before_calls);

    items[4] = Model::Item{100, "replacement"};
    check(model.replace(items));
    check(window.update(40.0f, 100.0f));
    check(factory_calls == before_calls + 1);
}

void invalid_geometry_is_atomic() {
    using Model = ui::detail::VirtualListDatasetModel<int>;
    using Window = ui::detail::VirtualListMaterializationWindow<int, int>;

    Model model;
    check(model.replace({Model::Item{1, "one"}, Model::Item{2, "two"}}));
    Window window{model, 20.0f, [](const Model::Item& item) { return item.key; }};

    check(window.update(0.0f, 20.0f));
    const auto keys = window.keys();
    const auto indices = window.indices();
    check(!window.update(-1.0f, 20.0f));
    check(window.keys() == keys);
    check(window.indices() == indices);
}

} // namespace

int main() {
    viewport_bounded_factory_contract();
    keyed_reorder_reuses_materialized_payloads();
    invalid_geometry_is_atomic();
    return failures == 0 ? 0 : 1;
}
