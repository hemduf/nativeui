    // T094 keeps the existing full retained traversal as the reference path and
    // adds one root-logical selective traversal entry point for renderer-owned
    // damage reconstruction. This method never consumes Tree dirty state; T096
    // owns dirty-generation commit semantics.
    void paint_region(SkCanvas& canvas,
                      PlatformServices& platform,
                      Rect repaint_region) {
        t094_paint_region(canvas, platform, repaint_region);
    }

    // Keep T094 cache invalidation attached to the same public layout/theme
    // publication entry points without teaching those earlier include fragments
    // about the later optimization cache.
    void layout(Size viewport) {
        t094_mark_cache_dirty();
        layout_t094_impl(viewport);
    }

    void set_theme(Theme value) {
        t094_mark_cache_dirty();
        set_theme_t094_impl(std::move(value));
    }
