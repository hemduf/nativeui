#include "test_support.hpp"

#include <memory>

namespace {

void suite() {
    ui::State<int> state{1};
    int observed = 0;

    auto subscription = state.observe([&](const int& value) { observed = value; });
    NUI_CHECK(subscription.active());
    state.set(2);
    NUI_CHECK(observed == 2);

    // Re-setting the same value must not emit a duplicate notification.
    state.set(2);
    NUI_CHECK(observed == 2);

    // Move ownership of a subscription without losing/unsubscribing it.
    auto moved = std::move(subscription);
    NUI_CHECK(!subscription.active());
    NUI_CHECK(moved.active());
    state.set(3);
    NUI_CHECK(observed == 3);

    moved.reset();
    NUI_CHECK(!moved.active());
    state.set(4);
    NUI_CHECK(observed == 3);

    // A subscription is allowed to outlive its State. Destruction/reset must
    // not dereference a dead State object (important under ASan/UBSan).
    ui::State<int>::Subscription survivor;
    int temporary_observed = 0;
    {
        auto temporary = std::make_unique<ui::State<int>>(10);
        survivor = temporary->observe([&](const int& value) { temporary_observed = value; });
        temporary->set(11);
        NUI_CHECK(temporary_observed == 11);
        NUI_CHECK(survivor.active());
    }
    NUI_CHECK(!survivor.active());
    survivor.reset();
}

} // namespace

int main() { return test::run("state", &suite); }
