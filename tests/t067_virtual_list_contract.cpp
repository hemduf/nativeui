#include <nativeui/detail/virtual_list_model.hpp>

#include <cstddef>
#include <limits>

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

} // namespace

int main() {
    range_contract();
    content_height_contract();
    return failures == 0 ? 0 : 1;
}
