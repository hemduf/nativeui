#include "test_support.hpp"
#include <nativeui/state.hpp>

#include <functional>
#include <memory>
#include <vector>

namespace {
struct Probe {
    bool armed{};
    bool copying{};
    bool destroyed_during_copy{};
    int copy_callbacks{};
    std::function<void()> replace;
};
struct Key {
    int value{};
    std::shared_ptr<Probe> probe;
    bool source_element{};
    Key(int value,std::shared_ptr<Probe> probe,bool source_element=true)
        :value(value),probe(std::move(probe)),source_element(source_element) {}
    Key(const Key& other):value(other.value),probe(other.probe),source_element(false) {
        // All fields and the callable are owned before the mutation. Nothing
        // reads other after replace(), so the regression oracle itself does
        // not dereference a freed element or continue a vector iterator.
        const auto retained=probe;
        if (retained && std::exchange(retained->armed,false)) {
            const auto replace=retained->replace;
            retained->copying=true;
            try { ++retained->copy_callbacks; if (replace) replace(); }
            catch (...) { retained->copying=false; throw; }
            retained->copying=false;
        }
    }
    Key(Key&&)=default;
    Key& operator=(const Key&)=default;
    Key& operator=(Key&&)=default;
    ~Key() { if (source_element && probe && probe->copying) probe->destroyed_during_copy=true; }
    friend bool operator==(const Key& first,const Key& second) { return first.value==second.value; }
};
void key_copy_needs_an_owned_value_pin_before_reentrant_dataset_replacement() {
    const auto probe=std::make_shared<Probe>();
    std::vector<Key> initial; initial.emplace_back(1,probe);
    ui::State<std::vector<Key>> model{std::move(initial)};
    auto binding=model.binding();
    probe->replace=[&] { std::vector<Key> replacement; replacement.emplace_back(2,probe); binding.set(std::move(replacement)); };
    const auto before_revision=binding.revision();
    probe->armed=true;
    // The copied-read frame preserves borrowed storage until the Key copy
    // completes, then drains the queued replacement. This snapshot owns Key1.
    const auto snapshot=binding.snapshot();
    const Key& copied=snapshot.front();
    NUI_CHECK(copied.value==1 && binding.get().front().value==2);
    NUI_CHECK(binding.revision()==before_revision+1 && probe->copy_callbacks==1);
    NUI_CHECK(!probe->destroyed_during_copy);
}
}
int main() { return test::run("state_snapshot_copy_retirement",&key_copy_needs_an_owned_value_pin_before_reentrant_dataset_replacement); }
