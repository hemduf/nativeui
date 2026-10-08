    /// Paint retained content intersecting one root-logical repaint rectangle.
    ///
    /// `painter` and `platform` are borrowed for this synchronous renderer
    /// operation. Traversal may prepare layout/structure and invoke application
    /// paint callbacks. It never consumes/clears Tree dirty regions, so it is not
    /// an acknowledgement that pending damage was presented.
    void paint_region(Painter& painter,
                      PlatformServices& platform,
                      Rect repaint_region) {
        paint_cull_paint_region(painter, platform, repaint_region);
    }


