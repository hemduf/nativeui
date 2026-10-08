#include "test_support.hpp"
#include "detail/platform/scene_damage_state.hpp"

#include <limits>

namespace {

void conservative_union() {
    ui::detail::PendingSceneDamage damage;
    NUI_CHECK(!damage.valid());
    NUI_CHECK(!damage.retain({0.0f, 0.0f, 0.0f, 5.0f}));
    NUI_CHECK(!damage.covers({0.0f, 0.0f, 1.0f, 1.0f}));
    NUI_CHECK(damage.retain({10.0f, 10.0f, 20.0f, 20.0f}));
    NUI_CHECK(damage.retain({-5.0f, 5.0f, 10.0f, 10.0f}));
    const auto r = damage.rect();
    NUI_CHECK(r.x == -5.0f && r.y == 5.0f);
    NUI_CHECK(r.w == 35.0f && r.h == 25.0f);
    NUI_CHECK(damage.covers({0.0f, 10.0f, 30.0f, 20.0f}));
    NUI_CHECK(!damage.covers({0.0f, 10.0f, 40.0f, 20.0f}));
}

void invalid_damage_and_recovery() {
    ui::detail::PendingSceneDamage damage;
    const float limit = std::numeric_limits<float>::max();
    NUI_CHECK(damage.retain({-limit, 0.0f, limit, 10.0f}));
    const auto r = damage.rect();
    NUI_CHECK(!damage.retain({limit, 0.0f, 1.0f, 1.0f}));
    NUI_CHECK(damage.rect().x == r.x && damage.rect().w == r.w);
    NUI_CHECK(!damage.retain({0.0f, 0.0f,
                              std::numeric_limits<float>::infinity(), 1.0f}));
    damage.clear();
    NUI_CHECK(!damage.valid());
    NUI_CHECK(damage.retain({3.0f, 4.0f, 5.0f, 6.0f}));
    NUI_CHECK(damage.covers({3.0f, 4.0f, 5.0f, 6.0f}));
}

void independent_owners() {
    ui::detail::PendingSceneDamage left, right;
    NUI_CHECK(left.retain({1.0f, 1.0f, 10.0f, 10.0f}));
    NUI_CHECK(!right.valid());
    NUI_CHECK(right.retain({100.0f, 100.0f, 10.0f, 10.0f}));
    left.clear();
    NUI_CHECK(!left.valid());
    NUI_CHECK(right.covers({100.0f, 100.0f, 10.0f, 10.0f}));
}

} // namespace

int main() {
    return test::run("Pugl scene damage state", [] {
        conservative_union();
        invalid_damage_and_recovery();
        independent_owners();
    });
}
