#include "test_support.hpp"

#include <nativeui/component_state.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {

struct NoEquality {};

template <class T>
concept SupportsState = requires { typename ui::State<T>; };

static_assert(SupportsState<int>);
static_assert(!SupportsState<NoEquality>);

struct CopyTrackedCallback {
    explicit CopyTrackedCallback(std::shared_ptr<int> copies)
        : copies(std::move(copies)) {}

    CopyTrackedCallback(const CopyTrackedCallback& other)
        : copies(other.copies) {
        ++*copies;
    }

    CopyTrackedCallback(CopyTrackedCallback&&) noexcept = default;
    CopyTrackedCallback& operator=(const CopyTrackedCallback&) = default;
    CopyTrackedCallback& operator=(CopyTrackedCallback&&) noexcept = default;

    void operator()(const int&) const {}

    std::shared_ptr<int> copies;
};

void suite() {
    ui::State<int> state{1};
    int observed = 0;
    int callback_count = 0;

    auto subscription = state.observe([&](const int& value) {
        observed = value;
        ++callback_count;
    });
    NUI_CHECK(subscription.active());
    state.set(2);
    NUI_CHECK(observed == 2);
    NUI_CHECK(callback_count == 1);

    // Re-setting the same value is a strict no-op.
    state.set(2);
    NUI_CHECK(observed == 2);
    NUI_CHECK(callback_count == 1);

    // Move ownership of a subscription without losing/unsubscribing it.
    auto moved = std::move(subscription);
    NUI_CHECK(!subscription.active());
    NUI_CHECK(moved.active());
    state.set(3);
    NUI_CHECK(observed == 3);
    NUI_CHECK(callback_count == 2);

    moved.reset();
    moved.reset();
    NUI_CHECK(!moved.active());
    state.set(4);
    NUI_CHECK(observed == 3);
    NUI_CHECK(callback_count == 2);

    // One pass exposes one stable value. Recursive writes do not mutate the
    // current pass and coalesce to the latest value for the next pass.
    ui::State<int> reentrant{0};
    std::vector<int> pass_trace;
    bool stable_value = true;
    auto first = reentrant.observe([&](const int& value) {
        pass_trace.push_back(100 + value);
        stable_value = stable_value && reentrant.get() == value;
        if (value == 1) {
            reentrant.set(2);
            reentrant.set(3);
        }
    });
    auto second = reentrant.observe([&](const int& value) {
        pass_trace.push_back(200 + value);
        stable_value = stable_value && reentrant.get() == value;
    });
    NUI_CHECK(first.active());
    NUI_CHECK(second.active());
    reentrant.set(1);
    NUI_CHECK(stable_value);
    NUI_CHECK(reentrant.get() == 3);
    NUI_CHECK(pass_trace == std::vector<int>({101, 201, 103, 203}));

    // Removing a later observer during dispatch deactivates it immediately;
    // no copied stale callback is allowed to run later in the same pass.
    ui::State<int> removal{0};
    ui::State<int>::Subscription later;
    int later_calls = 0;
    auto remover = removal.observe([&](const int&) { later.reset(); });
    later = removal.observe([&](const int&) { ++later_calls; });
    NUI_CHECK(remover.active());
    NUI_CHECK(later.active());
    removal.set(1);
    NUI_CHECK(later_calls == 0);
    NUI_CHECK(!later.active());

    // Observers added during a pass begin on the next mutation/pass, not in the
    // pass whose listener boundary has already been established.
    ui::State<int> addition{0};
    ui::State<int>::Subscription added;
    int added_calls = 0;
    auto adder = addition.observe([&](const int& value) {
        if (value == 1 && !added.active()) {
            added = addition.observe([&](const int&) { ++added_calls; });
        }
    });
    NUI_CHECK(adder.active());
    addition.set(1);
    NUI_CHECK(added.active());
    NUI_CHECK(added_calls == 0);
    addition.set(2);
    NUI_CHECK(added_calls == 1);

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
    survivor.reset();

    // Stronger teardown regression: destroying the State from its first
    // callback invalidates all remaining observers without freeing the active
    // callback underneath its own stack frame.
    auto destroyable = std::make_unique<ui::State<int>>(0);
    ui::State<int>::Subscription destroyer;
    ui::State<int>::Subscription stale;
    int stale_calls = 0;
    destroyer = destroyable->observe([&](const int&) { destroyable.reset(); });
    stale = destroyable->observe([&](const int&) { ++stale_calls; });
    ui::State<int>* destroyable_raw = destroyable.get();
    destroyable_raw->set(1);
    NUI_CHECK(!destroyable);
    NUI_CHECK(stale_calls == 0);
    NUI_CHECK(!destroyer.active());
    NUI_CHECK(!stale.active());

    // Stable-observer set() must not copy the complete callback list. Callback
    // copies made by registration are allowed; steady-state mutations are not.
    auto callback_copies = std::make_shared<int>(0);
    ui::State<int> copy_probe_state{0};
    auto copy_probe = copy_probe_state.observe(CopyTrackedCallback{callback_copies});
    NUI_CHECK(copy_probe.active());
    const int registration_copies = *callback_copies;
    copy_probe_state.set(1);
    copy_probe_state.set(2);
    copy_probe_state.set(3);
    NUI_CHECK(*callback_copies == registration_copies);

    // T059: generic component availability remains a public, platform-neutral
    // state vocabulary under the stronger State notification contract.
    ui::State<ui::VisibilityMode> visibility{ui::VisibilityMode::Visible};
    NUI_CHECK(visibility.get() == ui::VisibilityMode::Visible);
}

} // namespace

int main() { return test::run("state", &suite); }
