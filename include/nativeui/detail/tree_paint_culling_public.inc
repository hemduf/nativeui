    /// Paint only content intersecting one root-logical repaint region through
    /// an existing Painter. This never consumes Tree dirty state.
    void paint_region(Painter& painter,
                      PlatformServices& platform,
                      Rect repaint_region) {
        paint_cull_paint_region(painter, platform, repaint_region);
    }


