    [[nodiscard]] const Theme& theme() const noexcept { return theme_; }

    void set_theme(Theme value) {
        const auto invalidation = classify_theme_change(theme_, value);
        if (invalidation == ThemeInvalidation::None) {
            if (root_) bind_theme(*root_);
            return;
        }

        theme_ = std::move(value);
        if (root_) bind_theme(*root_);
        if (!mounted_ || !root_) return;

        if (invalidation == ThemeInvalidation::Layout) {
            invalidate_layout_from(*root_);
        } else {
            invalidate_paint(viewport_rect());
        }
    }
