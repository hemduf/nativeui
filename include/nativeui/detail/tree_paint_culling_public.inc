    // Keep the existing full retained traversal as the reference path and add
    // one root-logical selective traversal entry point for renderer-owned damage
    // reconstruction. This method never consumes Tree dirty state.
    void paint_region(Painter& painter,
                      PlatformServices& platform,
                      Rect repaint_region) {
        paint_cull_paint_region(painter, platform, repaint_region);
    }


