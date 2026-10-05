#include "test_support.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/toast.hpp>

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

// Core-only renderer fixture: exercise the production UI scene preparation,
// transaction and partial-paint helpers without a GL context or GPU readback.
namespace ui::detail {
class SkiaGlRenderer final {
  public:
    bool partial(UI& view, SkCanvas& canvas, PlatformServices& platform) {
        bool requires_full = false;
        if (!view.prepare_scene_paint(platform, requires_full)) return false;
        prepared_full = requires_full;
        auto transaction = view.begin_scene_paint_transaction();
        if (!view.scene_paint_transaction_valid(transaction) ||
            !view.scene_paint_transaction_has_damage(transaction) ||
            !view.scene_paint_transaction_damage_valid(transaction)) return false;
        bool used_effects = false;
        const auto damage = view.scene_paint_transaction_damage(transaction);
        canvas.save();
        bool painted = false;
        try {
            if (requires_full) {
                ++full_calls;
                canvas.clear(SK_ColorBLACK);
                painted = view.paint_full_scene_prepared(transaction,canvas,platform,used_effects);
            } else {
                ++partial_calls;
                canvas.clipRect(SkRect::MakeXYWH(damage.x,damage.y,damage.w,damage.h));
                canvas.clear(SK_ColorBLACK);
                painted = view.paint_partial_scene_prepared(
                    transaction,canvas,platform,damage,used_effects);
            }
        } catch (...) { canvas.restore(); throw; }
        canvas.restore();
        if (!painted) return false;
        view.commit_scene_paint(transaction);
        return true;
    }
    bool prepared_full{};
    unsigned full_calls{}, partial_calls{};
};
} // namespace ui::detail

namespace {
struct Fixture {
    std::shared_ptr<ui::detail::ManualDispatcherClock> clock{
        std::make_shared<ui::detail::ManualDispatcherClock>()};
    ui::detail::DispatcherOwner dispatcher{{},clock};
    test::MockPlatform platform;
    sk_sp<SkSurface> surface{SkSurfaces::Raster(SkImageInfo::MakeN32Premul(320,240))};
    Fixture() {
        platform.dispatcher_value = dispatcher.dispatcher();
        NUI_CHECK(surface);
    }
    void paint(ui::UI& view) {
        surface->getCanvas()->clear(SK_ColorBLACK);
        view.paint(*surface->getCanvas(),platform);
    }
};
std::optional<ui::NodeId> named(const ui::UI& view,std::string_view name) {
    for (ui::NodeId id=1;id<500;++id) {
        const auto info=view.component_semantics(id);
        if (info && info->name==name) return id;
    }
    return std::nullopt;
}
std::unique_ptr<ui::Toast> mounted_toast(ui::UI& view,Fixture& fixture,int& actions) {
    view.resize({320,240});
    view.activate(fixture.platform);
    auto notices=std::make_unique<ui::Toast>(view,fixture.dispatcher.dispatcher());
    const auto shown=notices->show({.message="Old message",.action_label="Old action",
                                   .action=[&actions] { ++actions; }});
    NUI_CHECK(shown.status==ui::ToastShowStatus::Shown && shown.handle.valid());
    fixture.paint(view);
    fixture.paint(view);
    NUI_CHECK(named(view,"Old message") && named(view,"Old action"));
    NUI_CHECK(!view.dirty() && fixture.dispatcher.active_timer_count()==1);
    return notices;
}
void require_retired(const ui::UI& view,const Fixture& fixture,int actions) {
    NUI_CHECK(view.overlay_entries().empty());
    NUI_CHECK(!named(view,"Old message") && !named(view,"Old action"));
    NUI_CHECK(named(view,"Root"));
    NUI_CHECK(fixture.dispatcher.active_timer_count()==0 && actions==0);
}
void direct_paint_flushes_a_quiet_last_overlay_without_resize_or_dispatch() {
    Fixture fixture;
    int actions=0, invalidations=0;
    ui::UI view{ui::Button{"Root",[] {}}};
    auto notices=mounted_toast(view,fixture,actions);
    view.set_invalidation_callback([&](ui::Rect) { ++invalidations; });
    const int before=invalidations;
    notices.reset();
    NUI_CHECK(invalidations==before && view.overlay_entries().empty());
    NUI_CHECK(view.dirty() && view.layout_dirty() && view.paint_dirty());
    // No UI.resize, HeadlessRenderer.render, dispatch or activate after reset.
    fixture.paint(view);
    require_retired(view,fixture,actions);
    NUI_CHECK(!view.dirty() && invalidations>before);
}
void partial_renderer_preparation_flushes_a_quiet_last_overlay() {
    Fixture fixture;
    int actions=0;
    ui::UI view{ui::Button{"Root",[] {}}};
    auto notices=mounted_toast(view,fixture,actions);
    notices.reset();
    NUI_CHECK(view.overlay_entries().empty() && view.dirty() && view.layout_dirty() &&
              view.paint_dirty());
    ui::detail::SkiaGlRenderer renderer;
    NUI_CHECK(renderer.partial(view,*fixture.surface->getCanvas(),fixture.platform));
    // Structural removal must conservatively rebuild the retained scene before
    // a localized partial update becomes admissible again.
    NUI_CHECK(renderer.prepared_full && renderer.full_calls==1 && renderer.partial_calls==0);
    require_retired(view,fixture,actions);
    NUI_CHECK(!view.dirty());
    view.invalidate({0,0,8,8});
    NUI_CHECK(renderer.partial(view,*fixture.surface->getCanvas(),fixture.platform));
    NUI_CHECK(!renderer.prepared_full && renderer.full_calls==1 && renderer.partial_calls==1);
    require_retired(view,fixture,actions);
    NUI_CHECK(!view.dirty());
}
enum class Entry { Paint, Partial, Activate, ResizeThrow };
void quiet_flush_may_destroy_its_ui(Entry entry) {
    Fixture fixture;
    int actions=0, invalidations=0;
    auto view=std::make_unique<ui::UI>(ui::Button{"Root",[] {}});
    auto notices=mounted_toast(*view,fixture,actions);
    bool armed=false;
    view->set_invalidation_callback([&](ui::Rect) {
        if (!armed) return;
        armed=false;
        ++invalidations;
        const bool throw_after_destroy=entry==Entry::ResizeThrow;
        view.reset();
        if (throw_after_destroy) throw std::runtime_error("quiet flush destroyed UI");
    });
    const int before=invalidations;
    notices.reset();
    // The destructor itself must not run the invalidator. Only the new quiet
    // flush owns this callback, so no prior State/application callback is used.
    NUI_CHECK(view && invalidations==before && view->overlay_entries().empty() && view->dirty());
    armed=true;
    auto* const raw=view.get();
    if (entry==Entry::Paint) {
        raw->paint(*fixture.surface->getCanvas(),fixture.platform);
    } else if (entry==Entry::Partial) {
        ui::detail::SkiaGlRenderer renderer;
        NUI_CHECK(!renderer.partial(*raw,*fixture.surface->getCanvas(),fixture.platform));
        NUI_CHECK(renderer.full_calls==0 && renderer.partial_calls==0);
    } else if (entry==Entry::Activate) {
        raw->activate(fixture.platform);
    } else {
        bool threw=false;
        try { raw->resize({300,220}); }
        catch (const std::runtime_error& error) {
            threw=std::string_view{error.what()}=="quiet flush destroyed UI";
        }
        NUI_CHECK(threw);
    }
    NUI_CHECK(!view && invalidations==before+1 && actions==0 &&
              fixture.dispatcher.active_timer_count()==0);
}
void destroy_paint() { quiet_flush_may_destroy_its_ui(Entry::Paint); }
void destroy_partial() { quiet_flush_may_destroy_its_ui(Entry::Partial); }
void destroy_activate() { quiet_flush_may_destroy_its_ui(Entry::Activate); }
void destroy_resize_throw() { quiet_flush_may_destroy_its_ui(Entry::ResizeThrow); }
void suite() {
    direct_paint_flushes_a_quiet_last_overlay_without_resize_or_dispatch();
    partial_renderer_preparation_flushes_a_quiet_last_overlay();
    destroy_paint(); destroy_partial(); destroy_activate(); destroy_resize_throw();
}
} // namespace
int main(int argc,char** argv) {
    const auto mode=argc>1?std::string_view{argv[1]}:std::string_view{};
    if (mode=="direct") return test::run("toast_quiet_direct",&direct_paint_flushes_a_quiet_last_overlay_without_resize_or_dispatch);
    if (mode=="partial") return test::run("toast_quiet_partial",&partial_renderer_preparation_flushes_a_quiet_last_overlay);
    if (mode=="destroy_paint") return test::run("toast_quiet_destroy_paint",&destroy_paint);
    if (mode=="destroy_partial") return test::run("toast_quiet_destroy_partial",&destroy_partial);
    if (mode=="destroy_activate") return test::run("toast_quiet_destroy_activate",&destroy_activate);
    if (mode=="destroy_resize_throw") return test::run("toast_quiet_destroy_resize_throw",&destroy_resize_throw);
    return test::run("toast_quiet_paint",&suite);
}
