    // Keep the existing full retained traversal as the reference path and add
    // one root-logical selective traversal entry point for renderer-owned damage
    // reconstruction. This method never consumes Tree dirty state.
    void paint_region(SkCanvas& canvas,
                      PlatformServices& platform,
                      Rect repaint_region) {
        paint_cull_paint_region(canvas, platform, repaint_region);
    }

    // Keep paint-cull cache invalidation attached to the same public layout/theme
    // publication entry points without coupling those include fragments to the
    // culling implementation.
    void layout(Size viewport) {
        paint_cull_mark_cache_dirty();
        layout_without_paint_cull_cache_invalidation(viewport);
    }

    void set_theme(Theme value) {
        paint_cull_mark_cache_dirty();
        set_theme_without_paint_cull_cache_invalidation(std::move(value));
    }
