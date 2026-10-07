#include "test_support.hpp"

#include <functional>
#include <memory>

namespace {
struct Comparison {
    int count{};
    int retire_at{2};
    std::function<void()> retire;
};
struct Key {
    int value{};
    std::shared_ptr<Comparison> comparison;
    bool operator==(const Key& other) const {
        const auto probe = comparison;
        if (probe && ++probe->count == probe->retire_at) {
            const auto retire = probe->retire;
            if (retire) retire();
        }
        return value == other.value;
    }
};
void equality_cannot_publish_after_retiring_the_source() {
    for (const int retire_at : {1,2}) {
        auto probe = std::make_shared<Comparison>();
        probe->retire_at = retire_at;
        auto owner = std::make_unique<ui::State<Key>>(Key{1,probe});
        auto binding = owner->binding();
        int calls{};
        const auto subscription = binding.observe([&](const Key&) { ++calls; });
        probe->retire = [&] { owner.reset(); };
        binding.set(Key{2,probe});
        NUI_CHECK(!binding.valid());
        NUI_CHECK(binding.get().value == 1);
        NUI_CHECK(calls == 0 && !subscription.active());
        binding.set(Key{3,probe});
        NUI_CHECK(binding.get().value == 1 && calls == 0);
    }
}
}
int main() { return test::run("widget_state_key_retirement",&equality_cannot_publish_after_retiring_the_source); }
