#include <nativeui/detail/virtual_list_model.hpp>

#include <cassert>
#include <cstddef>
#include <limits>

namespace {

void range_contract() {
    using ui::detail::VirtualListRange;
    using ui::detail::virtual_list_materialization_range;

    const auto first = virtual_list_materialization_range(100, 20.0f, 0.0f, 100.0f, 2);
    assert(first.has_value());
    assert(*first == VirtualListRange{0, 7});

    const auto fractional = virtual_list_materialization_range(100, 20.0f, 10.0f, 100.0f, 2);
    assert(fractional.has_value());
    assert(*fractional == VirtualListRange{0, 8});

    const auto aligned = virtual_list_materialization_range(100, 20.0f, 40.0f, 100.0f, 2);
    assert(aligned.has_value());
    assert(*aligned == VirtualListRange{0, 9});

    const auto end = virtual_list_materialization_range(100, 20.0f, 1900.0f, 100.0f, 2);
    assert(end.has_value());
    assert(*end == VirtualListRange{93, 100});

    const auto empty = virtual_list_materialization_range(0, 20.0f, 0.0f, 100.0f, 2);
    assert(empty.has_value());
    assert(empty->empty());

    assert(!virtual_list_materialization_range(10, 0.0f, 0.0f, 100.0f, 2));
    assert(!virtual_list_materialization_range(
        10, std::numeric_limits<float>::quiet_NaN(), 0.0f, 100.0f, 2));
}

void content_height_contract() {
    using ui::detail::virtual_list_content_height;

    const auto empty = virtual_list_content_height(0, 20.0f);
    assert(empty.has_value() && *empty == 0.0f);

    const auto normal = virtual_list_content_height(100, 20.0f);
    assert(normal.has_value() && *normal == 2000.0f);

    assert(!virtual_list_content_height(1, 0.0f));
    assert(!virtual_list_content_height(
        1, std::numeric_limits<float>::infinity()));
    assert(!virtual_list_content_height(
        2, std::numeric_limits<float>::max()));
}

} // namespace

int main() {
    range_contract();
    content_height_contract();
    return 0;
}
