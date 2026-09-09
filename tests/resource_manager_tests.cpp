#include "test_support.hpp"

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace allocation_probe {
std::atomic<std::size_t> allocations{0};
std::atomic<bool> enabled{false};

void note() noexcept {
    if (enabled.load(std::memory_order_relaxed)) {
        allocations.fetch_add(1, std::memory_order_relaxed);
    }
}

void begin() noexcept {
    allocations.store(0, std::memory_order_relaxed);
    enabled.store(true, std::memory_order_release);
}

std::size_t end() noexcept {
    enabled.store(false, std::memory_order_release);
    return allocations.load(std::memory_order_relaxed);
}
} // namespace allocation_probe

void* operator new(std::size_t size) {
    allocation_probe::note();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
    allocation_probe::note();
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

constexpr std::array<std::byte, 3> kAlpha{
    std::byte{0x10}, std::byte{0x00}, std::byte{0x20}};
constexpr std::array<std::byte, 2> kBeta{
    std::byte{0x30}, std::byte{0x40}};
constexpr std::array<std::byte, 1> kOther{std::byte{0x7f}};

constexpr std::array<std::byte, 75> kTinyRgbaPng{
    std::byte{137}, std::byte{80}, std::byte{78}, std::byte{71}, std::byte{13}, std::byte{10},
    std::byte{26}, std::byte{10}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{13},
    std::byte{73}, std::byte{72}, std::byte{68}, std::byte{82}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2},
    std::byte{8}, std::byte{6}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{114},
    std::byte{182}, std::byte{13}, std::byte{36}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{18}, std::byte{73}, std::byte{68}, std::byte{65}, std::byte{84}, std::byte{120},
    std::byte{218}, std::byte{99}, std::byte{248}, std::byte{207}, std::byte{192}, std::byte{240},
    std::byte{31}, std::byte{12}, std::byte{129}, std::byte{52}, std::byte{24}, std::byte{0},
    std::byte{0}, std::byte{73}, std::byte{200}, std::byte{9}, std::byte{247}, std::byte{3},
    std::byte{217}, std::byte{100}, std::byte{241}, std::byte{0}, std::byte{0}, std::byte{0},
    std::byte{0}, std::byte{73}, std::byte{69}, std::byte{78}, std::byte{68}, std::byte{174},
    std::byte{66}, std::byte{96}, std::byte{130},
};

constexpr std::string_view kSvg =
    R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="2" height="2" viewBox="0 0 2 2"><rect width="2" height="2" fill="#ffffff"/></svg>)svg";
constexpr char kUtf8EAcute[] = "\xC3\xA9";

std::span<const std::byte> svg_bytes() noexcept {
    return {reinterpret_cast<const std::byte*>(kSvg.data()), kSvg.size()};
}

void validation_and_zero_copy_lookup() {
    const std::array<ui::EmbeddedResourceEntry, 3> entries{{
        {"alpha", kAlpha},
        {"beta", kBeta},
        {"empty", {}},
    }};

    ui::ResourceManager manager{entries};
    NUI_CHECK(manager.valid());
    NUI_CHECK(manager.validation_error().empty());
    NUI_CHECK(manager.resources().data() == entries.data());
    NUI_CHECK(manager.resources().size() == entries.size());

    const auto found = manager.find("alpha");
    NUI_CHECK(found.has_value());
    NUI_CHECK(found->id == "alpha");
    NUI_CHECK(found->id.data() == entries[0].id.data());
    NUI_CHECK(found->bytes.data() == kAlpha.data());
    NUI_CHECK(found->bytes.size() == kAlpha.size());
    NUI_CHECK(found->bytes[1] == std::byte{0x00});
    NUI_CHECK(manager.contains("beta"));
    NUI_CHECK(!manager.contains("Alpha"));
    NUI_CHECK(!manager.find("missing").has_value());

    const auto empty = manager.find("empty");
    NUI_CHECK(empty.has_value());
    NUI_CHECK(empty->bytes.empty());
}

void generated_table_byte_order_is_accepted() {
    const std::array<ui::EmbeddedResourceEntry, 4> entries{{
        {";semi", kAlpha},
        {"A", kBeta},
        {"a", kOther},
        {std::string_view{kUtf8EAcute, 2}, kAlpha},
    }};
    const ui::ResourceManager manager{entries};
    NUI_CHECK(manager.valid());
    NUI_CHECK(manager.find(";semi").has_value());
    NUI_CHECK(manager.find("A").has_value());
    NUI_CHECK(manager.find("a").has_value());
    NUI_CHECK(manager.find(std::string_view{kUtf8EAcute, 2}).has_value());
}

void invalid_tables_are_atomic_and_diagnostic() {
    const std::array<ui::EmbeddedResourceEntry, 0> none{};
    const ui::ResourceManager empty{none};
    NUI_CHECK(empty.valid());
    NUI_CHECK(empty.validation_error().empty());
    NUI_CHECK(empty.resources().empty());

    const std::array<ui::EmbeddedResourceEntry, 2> empty_id{{
        {"", kAlpha},
        {"beta", kBeta},
    }};
    const ui::ResourceManager invalid_empty{empty_id};
    NUI_CHECK(!invalid_empty.valid());
    NUI_CHECK(invalid_empty.validation_error() == "resource id is empty");
    NUI_CHECK(invalid_empty.resources().empty());
    NUI_CHECK(!invalid_empty.find("beta").has_value());
    NUI_CHECK(!invalid_empty.contains("beta"));

    const std::array<ui::EmbeddedResourceEntry, 2> duplicate{{
        {"alpha", kAlpha},
        {"alpha", kBeta},
    }};
    const ui::ResourceManager invalid_duplicate{duplicate};
    NUI_CHECK(!invalid_duplicate.valid());
    NUI_CHECK(invalid_duplicate.validation_error() ==
              "resource ids must be strictly ascending and unique");
    NUI_CHECK(invalid_duplicate.resources().empty());

    const std::array<ui::EmbeddedResourceEntry, 2> descending{{
        {"beta", kBeta},
        {"alpha", kAlpha},
    }};
    const ui::ResourceManager invalid_order{descending};
    NUI_CHECK(!invalid_order.valid());
    NUI_CHECK(invalid_order.validation_error() ==
              "resource ids must be strictly ascending and unique");
    NUI_CHECK(invalid_order.resources().empty());
}

void direct_api_is_allocation_free() {
    const std::array<ui::EmbeddedResourceEntry, 3> entries{{
        {"alpha", kAlpha},
        {"beta", kBeta},
        {"empty", {}},
    }};

    allocation_probe::begin();
    ui::ResourceManager manager{entries};
    const auto found = manager.find("beta");
    const bool present = manager.contains("alpha");
    const auto all = manager.resources();
    const bool valid = manager.valid();
    const auto error = manager.validation_error();
    const auto direct_allocations = allocation_probe::end();

    NUI_CHECK(direct_allocations == 0U);
    NUI_CHECK(found.has_value());
    NUI_CHECK(present);
    NUI_CHECK(all.data() == entries.data());
    NUI_CHECK(valid);
    NUI_CHECK(error.empty());

    const std::array<ui::EmbeddedResourceEntry, 2> duplicate{{
        {"alpha", kAlpha},
        {"alpha", kBeta},
    }};
    allocation_probe::begin();
    const ui::ResourceManager invalid{duplicate};
    const auto invalid_allocations = allocation_probe::end();
    NUI_CHECK(invalid_allocations == 0U);
    NUI_CHECK(!invalid.valid());
}

void large_table_lookup_sanity() {
    std::array<std::array<char, 4>, 256> ids{};
    std::array<ui::EmbeddedResourceEntry, 256> entries{};
    for (std::size_t index = 0; index < entries.size(); ++index) {
        ids[index][0] = 'r';
        ids[index][1] = static_cast<char>('0' + ((index / 100) % 10));
        ids[index][2] = static_cast<char>('0' + ((index / 10) % 10));
        ids[index][3] = static_cast<char>('0' + (index % 10));
        entries[index] = {std::string_view{ids[index].data(), ids[index].size()}, {}};
    }
    const ui::ResourceManager manager{entries};
    NUI_CHECK(manager.valid());

    allocation_probe::begin();
    const auto first = manager.find("r000");
    const auto middle = manager.find("r127");
    const auto last = manager.find("r255");
    const auto absent = manager.find("r999");
    const auto allocations = allocation_probe::end();
    NUI_CHECK(allocations == 0U);
    NUI_CHECK(first && first->id == "r000");
    NUI_CHECK(middle && middle->id == "r127");
    NUI_CHECK(last && last->id == "r255");
    NUI_CHECK(!absent);
}

void copy_move_and_instance_isolation() {
    const std::array<ui::EmbeddedResourceEntry, 1> first_entries{{{"same", kAlpha}}};
    const std::array<ui::EmbeddedResourceEntry, 1> second_entries{{{"same", kOther}}};

    ui::ResourceManager first{first_entries};
    ui::ResourceManager copy = first;
    ui::ResourceManager moved = std::move(copy);
    ui::ResourceManager second{second_entries};

    NUI_CHECK(first.resources().data() == first_entries.data());
    NUI_CHECK(moved.resources().data() == first_entries.data());
    NUI_CHECK(second.resources().data() == second_entries.data());
    NUI_CHECK(first.find("same")->bytes.data() == kAlpha.data());
    NUI_CHECK(second.find("same")->bytes.data() == kOther.data());
}

void concurrent_read_only_lookup() {
    const std::array<ui::EmbeddedResourceEntry, 3> entries{{
        {"alpha", kAlpha},
        {"beta", kBeta},
        {"empty", {}},
    }};
    const ui::ResourceManager manager{entries};
    std::atomic<bool> failed{false};
    std::array<std::thread, 4> workers;
    for (auto& worker : workers) {
        worker = std::thread([manager, &failed] {
            for (int i = 0; i < 20000; ++i) {
                const auto alpha = manager.find("alpha");
                const auto beta = manager.find("beta");
                if (!alpha || alpha->bytes.data() != kAlpha.data() ||
                    !beta || beta->bytes.data() != kBeta.data() ||
                    manager.contains("missing")) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
            }
        });
    }
    for (auto& worker : workers) worker.join();
    NUI_CHECK(!failed.load(std::memory_order_relaxed));
}

void provider_adapter_has_explicit_copy_boundary() {
    const std::array<ui::EmbeddedResourceEntry, 2> entries{{
        {"alpha", kAlpha},
        {"empty", {}},
    }};
    ui::ResourceManagerProvider provider{ui::ResourceManager{entries}};

    allocation_probe::begin();
    const auto loaded = provider.load("alpha");
    const auto success_allocations = allocation_probe::end();
    NUI_CHECK(loaded.has_value());
    NUI_CHECK(loaded->size() == kAlpha.size());
    NUI_CHECK(loaded->data() != kAlpha.data());
    NUI_CHECK((*loaded)[0] == kAlpha[0]);
    NUI_CHECK((*loaded)[1] == kAlpha[1]);
    NUI_CHECK(success_allocations > 0U);

    allocation_probe::begin();
    const auto missing = provider.load("missing");
    const auto missing_allocations = allocation_probe::end();
    NUI_CHECK(!missing.has_value());
    NUI_CHECK(missing_allocations == 0U);

    allocation_probe::begin();
    const auto empty = provider.load("empty");
    const auto empty_allocations = allocation_probe::end();
    NUI_CHECK(empty.has_value());
    NUI_CHECK(empty->empty());
    NUI_CHECK(empty_allocations == 0U);

    const std::array<ui::EmbeddedResourceEntry, 2> invalid_entries{{
        {"z", kAlpha},
        {"a", kBeta},
    }};
    ui::ResourceManagerProvider invalid_provider{ui::ResourceManager{invalid_entries}};
    allocation_probe::begin();
    const auto invalid = invalid_provider.load("a");
    const auto invalid_allocations = allocation_probe::end();
    NUI_CHECK(!invalid.has_value());
    NUI_CHECK(invalid_allocations == 0U);
}

void image_and_svg_caches_use_the_adapter() {
    const std::array<ui::EmbeddedResourceEntry, 2> entries{{
        {"icon.svg", svg_bytes()},
        {"tiny.png", kTinyRgbaPng},
    }};
    ui::ResourceManagerProvider provider{ui::ResourceManager{entries}};

    ui::ImageCache image_cache{provider};
    const auto image = image_cache.load("tiny.png");
    NUI_CHECK(image);
    NUI_CHECK(image.image.size().w == 2.0f);
    NUI_CHECK(image.image.size().h == 2.0f);

    ui::SvgCache svg_cache{provider};
    const auto icon = svg_cache.load("icon.svg");
    NUI_CHECK(icon);
    NUI_CHECK(icon.icon.intrinsic_size().w == 2.0f);
    NUI_CHECK(icon.icon.intrinsic_size().h == 2.0f);
}

void suite() {
    validation_and_zero_copy_lookup();
    generated_table_byte_order_is_accepted();
    invalid_tables_are_atomic_and_diagnostic();
    direct_api_is_allocation_free();
    large_table_lookup_sanity();
    copy_move_and_instance_isolation();
    concurrent_read_only_lookup();
    provider_adapter_has_explicit_copy_boundary();
    image_and_svg_caches_use_the_adapter();
}

} // namespace

int main() { return test::run("resource-manager", &suite); }
