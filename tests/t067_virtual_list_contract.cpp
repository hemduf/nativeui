#include <nativeui/detail/virtual_list_model.hpp>

#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition) {
    if (!condition) ++failures;
}

void range_contract() {
    using ui::detail::VirtualListRange;
    using ui::detail::virtual_list_materialization_range;

    const auto first = virtual_list_materialization_range(100, 20.0f, 0.0f, 100.0f, 2);
    check(first.has_value());
    check(first && *first == VirtualListRange{0, 7});

    const auto fractional = virtual_list_materialization_range(100, 20.0f, 10.0f, 100.0f, 2);
    check(fractional.has_value());
    check(fractional && *fractional == VirtualListRange{0, 8});

    const auto aligned = virtual_list_materialization_range(100, 20.0f, 40.0f, 100.0f, 2);
    check(aligned.has_value());
    check(aligned && *aligned == VirtualListRange{0, 9});

    const auto end = virtual_list_materialization_range(100, 20.0f, 1900.0f, 100.0f, 2);
    check(end.has_value());
    check(end && *end == VirtualListRange{93, 100});

    const auto empty = virtual_list_materialization_range(0, 20.0f, 0.0f, 100.0f, 2);
    check(empty.has_value());
    check(empty && empty->empty());

    check(!virtual_list_materialization_range(10, 0.0f, 0.0f, 100.0f, 2));
    check(!virtual_list_materialization_range(
        10, std::numeric_limits<float>::quiet_NaN(), 0.0f, 100.0f, 2));
}

void content_height_contract() {
    using ui::detail::virtual_list_content_height;

    const auto empty = virtual_list_content_height(0, 20.0f);
    check(empty.has_value() && *empty == 0.0f);

    const auto normal = virtual_list_content_height(100, 20.0f);
    check(normal.has_value() && *normal == 2000.0f);

    check(!virtual_list_content_height(1, 0.0f));
    check(!virtual_list_content_height(
        1, std::numeric_limits<float>::infinity()));
    check(!virtual_list_content_height(
        2, std::numeric_limits<float>::max()));
}

void dataset_identity_contract() {
    using Model = ui::detail::VirtualListDatasetModel<int>;
    using Input = Model::Item;

    Model model;
    check(model.generation() == 0);
    check(model.size() == 0);

    check(model.replace({
        Input{10, "ten", true},
        Input{20, "twenty", true},
        Input{30, "thirty", false},
    }));
    check(model.generation() == 1);
    check(model.size() == 3);

    const auto token10 = model.token_for_key(10);
    const auto token20 = model.token_for_key(20);
    const auto token30 = model.token_for_key(30);
    check(token10.has_value() && *token10 != ui::kInvalidVirtualSemanticItemToken);
    check(token20.has_value() && *token20 != ui::kInvalidVirtualSemanticItemToken);
    check(token30.has_value() && *token30 != ui::kInvalidVirtualSemanticItemToken);
    check(token10 != token20 && token10 != token30 && token20 != token30);

    const auto metadata1 = model.metadata_snapshot();
    check(metadata1 && metadata1->size() == 3);
    const auto selected20 = model.semantic_children(20, {0.0f, 0.0f, 100.0f, 60.0f}, 20.0f, 0.0f);
    const auto selected10 = model.semantic_children(10, {0.0f, 0.0f, 100.0f, 60.0f}, 20.0f, 20.0f);
    check(selected20.metadata_snapshot().get() == metadata1.get());
    check(selected10.metadata_snapshot().get() == metadata1.get());
    check(selected20.index_of_selected_item() == 1);
    check(selected10.index_of_selected_item() == 0);

    check(model.replace({
        Input{30, "thirty", false},
        Input{10, "ten", true},
        Input{20, "twenty", true},
    }));
    check(model.generation() == 2);
    check(model.token_for_key(10) == token10);
    check(model.token_for_key(20) == token20);
    check(model.token_for_key(30) == token30);
    const auto metadata2 = model.metadata_snapshot();
    check(metadata2.get() != metadata1.get());

    const auto generation_before_duplicate = model.generation();
    const auto metadata_before_duplicate = model.metadata_snapshot();
    check(!model.replace({Input{10, "a", true}, Input{10, "b", true}}));
    check(model.generation() == generation_before_duplicate);
    check(model.metadata_snapshot().get() == metadata_before_duplicate.get());
    check(model.size() == 3);

    check(model.replace({
        Input{30, "thirty", false},
        Input{10, "ten", true},
        Input{20, "twenty", true},
    }));
    check(model.generation() == generation_before_duplicate);
    check(model.metadata_snapshot().get() == metadata_before_duplicate.get());
}

void large_dataset_contract() {
    using Model = ui::detail::VirtualListDatasetModel<std::size_t>;
    std::vector<Model::Item> items;
    items.reserve(100000);
    for (std::size_t i = 0; i < 100000; ++i) {
        items.push_back({i, "row", true});
    }

    Model model;
    check(model.replace(std::move(items)));
    check(model.size() == 100000);
    check(model.metadata_snapshot()->size() == 100000);
}

} // namespace

int main() {
    range_contract();
    content_height_contract();
    dataset_identity_contract();
    large_dataset_contract();
    return failures == 0 ? 0 : 1;
}
