#include "test_support.hpp"
#include "../src/detail/window_control_state.hpp"

#include <limits>
#include <optional>

namespace {

bool same(ui::Size a, ui::Size b) {
    return a.w == b.w && a.h == b.h;
}

void test_size_constraints_validate_and_clamp_atomically() {
    ui::detail::WindowSizeConstraints constraints;

    NUI_CHECK(constraints.update(ui::Size{100.0f, 80.0f}, ui::Size{400.0f, 300.0f}));
    NUI_CHECK(constraints.min_size().has_value());
    NUI_CHECK(constraints.max_size().has_value());

    const auto low_high = constraints.clamp({50.0f, 500.0f});
    NUI_CHECK(low_high.has_value());
    NUI_CHECK(same(*low_high, {100.0f, 300.0f}));

    const auto inside = constraints.clamp({240.0f, 160.0f});
    NUI_CHECK(inside.has_value());
    NUI_CHECK(same(*inside, {240.0f, 160.0f}));

    const auto previous_min = constraints.min_size();
    const auto previous_max = constraints.max_size();
    NUI_CHECK(!constraints.update(ui::Size{500.0f, 80.0f}, ui::Size{400.0f, 300.0f}));
    NUI_CHECK(constraints.min_size() == previous_min);
    NUI_CHECK(constraints.max_size() == previous_max);
}

void test_size_constraints_reject_non_finite_or_non_positive_values() {
    ui::detail::WindowSizeConstraints constraints;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    for (const ui::Size invalid : {
             ui::Size{0.0f, 10.0f},
             ui::Size{10.0f, 0.0f},
             ui::Size{-1.0f, 10.0f},
             ui::Size{10.0f, -1.0f},
             ui::Size{nan, 10.0f},
             ui::Size{10.0f, inf}}) {
        NUI_CHECK(!constraints.update(invalid, std::nullopt));
        NUI_CHECK(!constraints.update(std::nullopt, invalid));
        NUI_CHECK(!constraints.clamp(invalid).has_value());
    }

    NUI_CHECK(constraints.update(std::nullopt, std::nullopt));
    const auto unconstrained = constraints.clamp({320.0f, 180.0f});
    NUI_CHECK(unconstrained.has_value());
    NUI_CHECK(same(*unconstrained, {320.0f, 180.0f}));
}

void test_runtime_constraint_updates_preserve_old_pair_on_failure() {
    ui::detail::WindowSizeConstraints constraints;
    NUI_CHECK(constraints.update(ui::Size{100.0f, 80.0f}, ui::Size{500.0f, 400.0f}));

    NUI_CHECK(constraints.set_min(ui::Size{200.0f, 120.0f}));
    NUI_CHECK(same(*constraints.min_size(), {200.0f, 120.0f}));
    NUI_CHECK(constraints.set_max(ui::Size{300.0f, 240.0f}));
    NUI_CHECK(same(*constraints.max_size(), {300.0f, 240.0f}));

    NUI_CHECK(!constraints.set_min(ui::Size{301.0f, 120.0f}));
    NUI_CHECK(same(*constraints.min_size(), {200.0f, 120.0f}));
    NUI_CHECK(same(*constraints.max_size(), {300.0f, 240.0f}));

    NUI_CHECK(!constraints.set_max(ui::Size{199.0f, 240.0f}));
    NUI_CHECK(same(*constraints.min_size(), {200.0f, 120.0f}));
    NUI_CHECK(same(*constraints.max_size(), {300.0f, 240.0f}));
}

void test_close_state_accept_cancel_and_exactly_once_completion() {
    ui::detail::WindowCloseState state;
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Open);

    NUI_CHECK(state.begin_user_request());
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Requesting);
    state.finish_user_request(false);
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Open);

    NUI_CHECK(state.begin_user_request());
    state.finish_user_request(true);
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Pending);
    NUI_CHECK(!state.begin_user_request());
    NUI_CHECK(!state.request_programmatic());

    NUI_CHECK(state.complete_accepted_close());
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Closed);
    NUI_CHECK(!state.complete_accepted_close());
    NUI_CHECK(!state.request_programmatic());
}

void test_programmatic_close_inside_veto_wins_over_cancel() {
    ui::detail::WindowCloseState state;

    NUI_CHECK(state.begin_user_request());
    NUI_CHECK(state.request_programmatic());
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Pending);

    state.finish_user_request(false);
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Pending);
    NUI_CHECK(state.complete_accepted_close());
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Closed);
}

void test_destructor_teardown_suppresses_pending_completion() {
    ui::detail::WindowCloseState state;

    NUI_CHECK(state.begin_user_request());
    state.finish_user_request(true);
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Pending);

    state.begin_teardown();
    NUI_CHECK(state.phase() == ui::detail::WindowClosePhase::Teardown);
    NUI_CHECK(!state.complete_accepted_close());
    NUI_CHECK(!state.begin_user_request());
    NUI_CHECK(!state.request_programmatic());
}

void suite() {
    test_size_constraints_validate_and_clamp_atomically();
    test_size_constraints_reject_non_finite_or_non_positive_values();
    test_runtime_constraint_updates_preserve_old_pair_on_failure();
    test_close_state_accept_cancel_and_exactly_once_completion();
    test_programmatic_close_inside_veto_wins_over_cancel();
    test_destructor_teardown_suppresses_pending_completion();
}

} // namespace

int main() {
    return test::run("t066_window_control_state_tests", suite);
}
