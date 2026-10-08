#include "test_support.hpp"

#include <nativeui/style_scope.hpp>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace allocation_fault {
bool armed{};
int hits{};
void* allocate(std::size_t size) {
    if (std::exchange(armed,false)) {
        ++hits;
        throw std::bad_alloc{};
    }
    if (void* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc{};
}
}
void* operator new(std::size_t size) { return allocation_fault::allocate(size); }
void* operator new[](std::size_t size) { return allocation_fault::allocate(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value,std::size_t) noexcept { std::free(value); }
void operator delete[](void* value,std::size_t) noexcept { std::free(value); }

namespace {
ui::Theme inherited_theme(char tag) {
    auto value = ui::default_theme();
    value.typography.family = std::string(160,tag) + " inherited family";
    value.typography.fallback_families = {
        std::string(150,tag) + " first inherited fallback",
        std::string(170,tag) + " second inherited fallback"};
    return value;
}
ui::StyleScopeOverrides typography_patch() {
    ui::StyleScopeOverrides patch;
    patch.typography.family = std::string(180,'S') + " scope family";
    patch.typography.fallback_families = std::vector<std::string>{
        std::string(170,'F') + " first scope fallback",
        std::string(190,'G') + " second scope fallback"};
    return patch;
}
void bind_noexcept_retains_last_theme_then_recovers() {
    // A noexcept allocation failure currently terminates. Make that RED a
    // controlled exit, avoiding an abort/core dump while Root qualifies it.
    const auto old_handler = std::set_terminate([] {
        allocation_fault::armed = false;
        std::fputs("FAIL StyleScope noexcept bind terminated after injected allocation failure\n",stderr);
        std::_Exit(EXIT_FAILURE);
    });
    ui::StyleScopeOverrides patch;
    patch.palette.accent = ui::Color{1.0f,0.0f,0.0f,1.0f};
    ui::detail::StyleScopeComponent component{patch};
    auto old_parent = inherited_theme('A');
    auto new_parent = inherited_theme('B');
    new_parent.typography.control_size = 27.0f;
    const auto old_expected = ui::apply_style_scope_overrides(old_parent,patch);
    const auto new_expected = ui::apply_style_scope_overrides(new_parent,patch);
    component.bind_theme(old_parent);
    NUI_CHECK(component.descendant_theme() == old_expected);

    // Long family/fallback strings guarantee a real allocation during bind,
    // and the global fail-once interceptor arms only around that exact call.
    allocation_fault::armed = true;
    component.bind_theme(new_parent);
    NUI_CHECK(!allocation_fault::armed && allocation_fault::hits == 1);
    NUI_CHECK(component.descendant_theme() == old_expected);

    component.bind_theme(new_parent);
    NUI_CHECK(component.descendant_theme() == new_expected);
    component.bind_theme(new_parent);
    NUI_CHECK(component.descendant_theme() == new_expected);
    std::set_terminate(old_handler);
}

void check_recipe_component(ui::Component& component,const ui::Theme& parent,
                            const ui::StyleScopeOverrides& expected) {
    auto* binding = dynamic_cast<ui::detail::ThemeBinding*>(&component);
    NUI_CHECK(binding);
    binding->bind_theme(parent);
    NUI_CHECK(binding->descendant_theme().typography.family == *expected.typography.family);
    NUI_CHECK(binding->descendant_theme().typography.fallback_families == *expected.typography.fallback_families);
}

void copied_compile_preserves_complete_overrides() {
    const auto patch = typography_patch();
    auto parent = inherited_theme('P');
    auto recipe = ui::StyleScope{patch,ui::Spacer{20.0f,20.0f}}.spec();
    auto first = ui::compile(recipe);
    auto second = ui::compile(recipe);
    check_recipe_component(*first->component,parent,patch);
    check_recipe_component(*second->component,parent,patch);
    // compile takes Spec by value: this control case should already be GREEN.
    auto copied = recipe;
    auto third = ui::compile(copied);
    check_recipe_component(*third->component,parent,patch);
}

void factory_reinvocation_and_post_invocation_copy_preserve_overrides() {
    const auto patch = typography_patch();
    auto parent = inherited_theme('P');
    auto recipe = ui::StyleScope{patch,ui::Spacer{20.0f,20.0f}}.spec();
    auto first = recipe.factory();
    check_recipe_component(*first,parent,patch);
    // This copy observes the original factory AFTER it was invoked once.
    auto copied = recipe;
    auto second = recipe.factory();
    check_recipe_component(*second,parent,patch);
    auto third = copied.factory();
    check_recipe_component(*third,parent,patch);
    check_recipe_component(*first,parent,patch);
}
void suite() {
    copied_compile_preserves_complete_overrides();
    factory_reinvocation_and_post_invocation_copy_preserve_overrides();
    bind_noexcept_retains_last_theme_then_recovers();
}
}

int main(int argc,char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "all";
    if (mode == "bind") return test::run("style_scope_bind_fault",&bind_noexcept_retains_last_theme_then_recovers);
    if (mode == "copied_compile") return test::run("style_scope_copied_compile",&copied_compile_preserves_complete_overrides);
    if (mode == "factory_reuse") return test::run("style_scope_factory_reuse",&factory_reinvocation_and_post_invocation_copy_preserve_overrides);
    return test::run("style_scope_contract",&suite);
}
