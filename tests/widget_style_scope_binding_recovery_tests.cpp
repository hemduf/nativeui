#include "test_support.hpp"

#include <nativeui/detail/theme_binding.hpp>
#include <nativeui/style_scope.hpp>

#include <cstdlib>
#include <cstdio>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace style_allocation_fault {
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
void* operator new(std::size_t size) { return style_allocation_fault::allocate(size); }
void* operator new[](std::size_t size) { return style_allocation_fault::allocate(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value,std::size_t) noexcept { std::free(value); }
void operator delete[](void* value,std::size_t) noexcept { std::free(value); }

namespace {
struct Observation { ui::Color accent{}; std::string family; };
class ProbeComponent final : public ui::Component,public ui::detail::ThemeBinding {
public:
    explicit ProbeComponent(std::shared_ptr<Observation> value) : value_(std::move(value)) {}
    ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {40.0f,40.0f}; }
    void paint(ui::PaintContext& context) const override {
        value_->accent = current_theme().palette.accent;
        value_->family = current_theme().typography.family;
        context.painter().fill_rounded_rect(context.bounds(),0.0f,value_->accent);
    }
private:
    std::shared_ptr<Observation> value_;
};
class Probe {
public:
    explicit Probe(std::shared_ptr<Observation> value) : value_(std::move(value)) {}
    ui::Spec spec() && { const auto value=value_;return {[value] { return std::make_unique<ProbeComponent>(value); },{}}; }
private:
    std::shared_ptr<Observation> value_;
};
bool equal(ui::Color first,ui::Color second) {
    return first.r==second.r && first.g==second.g && first.b==second.b && first.a==second.a;
}

void committed_binding_patch_recovers_without_source_rewrite(bool prior_observer_throws) {
    style_allocation_fault::armed = false;
    style_allocation_fault::hits = 0;
    ui::StyleScopeOverrides initial;
    initial.palette.accent = ui::Color{0.0f,0.0f,0.0f,1.0f};
    ui::State<ui::StyleScopeOverrides> source{initial};
    const auto binding = source.binding();
    bool armed{};
    int earlier_calls{};
    auto earlier = source.observe([&](const ui::StyleScopeOverrides&) {
        ++earlier_calls;
        if (!std::exchange(armed,false)) return;
        if (prior_observer_throws) throw std::runtime_error("observer before StyleScope");
        // State has committed its new value/revision. The next allocation is
        // the long inherited Theme copy in StyleScope::replace_overrides.
        style_allocation_fault::armed = true;
    });
    auto inherited = ui::default_theme();
    inherited.typography.family = std::string(180,'P') + " inherited family";
    inherited.typography.fallback_families = {std::string(170,'F'),std::string(190,'G')};
    auto observation = std::make_shared<Observation>();
    ui::UI tree{ui::StyleScope{binding,Probe{observation}},inherited};
    test::MockPlatform platform;
    tree.resize({40.0f,40.0f});
    tree.activate(platform);
    ui::HeadlessRenderer renderer{{40.0f,40.0f},1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(equal(observation->accent,*initial.palette.accent));
    NUI_CHECK(binding.revision()==0 && earlier_calls==0);

    auto next = initial;
    next.palette.accent = ui::Color{1.0f,0.0f,0.0f,1.0f};
    armed = true;
    bool caught{};
    try { source.set(next); }
    catch (const std::bad_alloc&) { caught = !prior_observer_throws; }
    catch (const std::runtime_error&) { caught = prior_observer_throws; }
    // Never leave an unconsumed injected fault armed around NUI_CHECK's
    // diagnostic allocation, UI teardown, or the test runner's error output.
    const bool fault_was_consumed = !std::exchange(style_allocation_fault::armed,false);
    NUI_CHECK(caught);
    NUI_CHECK(fault_was_consumed);
    NUI_CHECK(style_allocation_fault::hits == (prior_observer_throws ? 0 : 1));
    NUI_CHECK(binding.get()==next && binding.revision()==1);
    NUI_CHECK(earlier_calls==1);
    NUI_CHECK(equal(observation->accent,*initial.palette.accent));

    tree.resize({40.0f,40.0f});
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(binding.get()==next && binding.revision()==1);
    NUI_CHECK(equal(observation->accent,*next.palette.accent));
    NUI_CHECK(observation->family==inherited.typography.family);
    NUI_CHECK(earlier_calls==1);
    tree.resize({40.0f,40.0f});
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(equal(observation->accent,*next.palette.accent));
    NUI_CHECK(binding.revision()==1 && earlier_calls==1);
    tree.deactivate(platform);
}
void previous_observer_suite() { committed_binding_patch_recovers_without_source_rewrite(true); }
void allocation_suite() { committed_binding_patch_recovers_without_source_rewrite(false); }
void suite() { previous_observer_suite();allocation_suite(); }
}
int main(int argc,char** argv) {
    std::set_terminate([] {
        style_allocation_fault::armed = false;
        std::fprintf(stderr,"FAIL StyleScope binding recovery terminated; injected fault hits=%d\n",
                     style_allocation_fault::hits);
        std::_Exit(EXIT_FAILURE);
    });
    const std::string_view mode=argc>1?argv[1]:"all";
    if(mode=="observer_previous_throw")return test::run("style_scope_prior_observer_recovery",&previous_observer_suite);
    if(mode=="callback_allocation")return test::run("style_scope_callback_allocation_recovery",&allocation_suite);
    return test::run("style_scope_binding_recovery",&suite);
}
