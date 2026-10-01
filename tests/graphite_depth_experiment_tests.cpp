#include "src/detail/graphite_depth_experiment.hpp"

#include <cstdio>

namespace {

bool require(bool condition, const char* message) {
    if (condition) {
        return true;
    }
    std::fprintf(stderr, "%s\n", message);
    return false;
}

bool default_mode_preserves_graphite_behavior() {
    skgpu::graphite::ContextOptions options;
    ui::detail::GraphiteDepthExperiment experiment;

    experiment.configure(options);

    return require(!options.fAvoidDepthMode,
                   "default experiment mode must preserve Graphite depth usage") &&
           require(!experiment.avoids_depth(),
                   "default experiment mode must report depth usage");
}

bool avoid_depth_mode_is_explicit_opt_in() {
    skgpu::graphite::ContextOptions options;
    ui::detail::GraphiteDepthExperiment experiment{
        ui::detail::GraphiteDepthMode::AvoidDepth};

    experiment.configure(options);

    return require(options.fAvoidDepthMode,
                   "avoid-depth mode must set the pinned Graphite option") &&
           require(experiment.avoids_depth(),
                   "avoid-depth mode must report its active policy");
}

bool fallback_restores_default_mode() {
    skgpu::graphite::ContextOptions options;
    ui::detail::GraphiteDepthExperiment avoid_depth{
        ui::detail::GraphiteDepthMode::AvoidDepth};
    ui::detail::GraphiteDepthExperiment fallback;

    avoid_depth.configure(options);
    if (!require(options.fAvoidDepthMode,
                 "precondition: avoid-depth mode must be active")) {
        return false;
    }

    fallback.configure(options);
    return require(!options.fAvoidDepthMode,
                   "fallback must restore the current Graphite behavior");
}

bool independent_options_do_not_share_state() {
    skgpu::graphite::ContextOptions first;
    skgpu::graphite::ContextOptions second;

    ui::detail::GraphiteDepthExperiment avoid_depth{
        ui::detail::GraphiteDepthMode::AvoidDepth};
    ui::detail::GraphiteDepthExperiment default_mode;

    avoid_depth.configure(first);
    default_mode.configure(second);

    if (!require(first.fAvoidDepthMode,
                 "first context must retain its avoid-depth policy")) {
        return false;
    }
    if (!require(!second.fAvoidDepthMode,
                 "second context must retain the default policy")) {
        return false;
    }

    default_mode.configure(first);
    return require(!first.fAvoidDepthMode && !second.fAvoidDepthMode,
                   "changing one option set must not mutate another");
}

} // namespace

int main() {
    bool ok = true;
    ok &= default_mode_preserves_graphite_behavior();
    ok &= avoid_depth_mode_is_explicit_opt_in();
    ok &= fallback_restores_default_mode();
    ok &= independent_options_do_not_share_state();
    return ok ? 0 : 1;
}
