#include "test_support.hpp"

#include <cmath>
#include <memory>
#include <vector>

namespace {

void check_size(ui::Size actual, ui::Size expected) {
    NUI_CHECK_NEAR(actual.w, expected.w, 0.0001f);
    NUI_CHECK_NEAR(actual.h, expected.h, 0.0001f);
}

void suite() {
    // Constraint sanitization/clamping.
    {
        const ui::Constraints c{{20.0f, 10.0f}, {100.0f, 80.0f}};
        check_size(c.constrain({5.0f, 200.0f}), {20.0f, 80.0f});
        check_size(c.constrain({50.0f, 40.0f}), {50.0f, 40.0f});

        const ui::Constraints bad{{-10.0f, std::nanf("")}, {-4.0f, 30.0f}};
        NUI_CHECK(bad.min.w >= 0.0f && bad.min.h >= 0.0f);
        NUI_CHECK(bad.max.w >= bad.min.w && bad.max.h >= bad.min.h);
        const auto clean = bad.constrain({std::nanf(""), -20.0f});
        NUI_CHECK(std::isfinite(clean.w) && clean.w >= 0.0f);
        NUI_CHECK(std::isfinite(clean.h) && clean.h >= 0.0f);
    }

    // Intrinsic preferred/minimum metrics remain distinguishable.
    {
        ui::SpacerComponent fixed{{80.0f, 30.0f}};
        const auto metrics = fixed.measure_constrained(ui::Constraints::loose({200.0f, 200.0f}), {});
        check_size(metrics.minimum, {80.0f, 30.0f});
        check_size(metrics.preferred, {80.0f, 30.0f});

        const auto clipped = fixed.measure_constrained(ui::Constraints::loose({40.0f, 20.0f}), {});
        check_size(clipped.minimum, {40.0f, 20.0f});
        check_size(clipped.preferred, {40.0f, 20.0f});
    }

    // Nested Row/Column preferred size is deterministically bounded by the
    // supplied constraints while preserving natural size when unbounded.
    {
        auto root = ui::compile(ui::make_spec(
            ui::Column{
                ui::Row{ui::Spacer{80.0f, 20.0f}, ui::Spacer{60.0f, 30.0f}}.gap(10.0f),
                ui::Spacer{30.0f, 15.0f},
            }.padding(5.0f).gap(5.0f)));
        ui::Tree tree{std::move(root)};
        tree.mount();

        const auto natural = tree.measure(ui::Constraints::unbounded());
        check_size(natural.preferred, {160.0f, 60.0f});

        const auto bounded = tree.measure(ui::Constraints::loose({100.0f, 50.0f}));
        check_size(bounded.preferred, {100.0f, 50.0f});
        NUI_CHECK(bounded.minimum.w <= bounded.preferred.w);
        NUI_CHECK(bounded.minimum.h <= bounded.preferred.h);
    }

    // Resize never creates negative/NaN component bounds even below natural size.
    {
        ui::UI tree{
            ui::Column{
                ui::Row{ui::Spacer{80.0f, 20.0f}, ui::Spacer{60.0f, 30.0f}}.gap(10.0f),
                ui::Spacer{30.0f, 15.0f},
            }.padding(5.0f).gap(5.0f)
        };
        tree.resize({37.0f, 29.0f});
        const auto metrics = tree.measure(ui::Constraints::tight({37.0f, 29.0f}));
        check_size(metrics.preferred, {37.0f, 29.0f});

        auto first = std::make_shared<test::ProbeState>();
        auto second = std::make_shared<test::ProbeState>();
        ui::UI tiny{ui::Row{test::Probe{first}, test::Probe{second}}.gap(18.0f)};
        test::MockPlatform platform;
        tiny.resize({5.0f, 5.0f});
        tiny.activate(platform);
        tiny.dispatch(test::key(ui::Key::Tab), platform);
        for (const auto& state : {first, second}) {
            NUI_CHECK(!state->focus_bounds.empty());
            const auto b = state->focus_bounds.back();
            NUI_CHECK(std::isfinite(b.x) && std::isfinite(b.y));
            NUI_CHECK(std::isfinite(b.w) && b.w >= 0.0f);
            NUI_CHECK(std::isfinite(b.h) && b.h >= 0.0f);
        }
    }
}

} // namespace

int main() { return test::run("layout_constraints", &suite); }
