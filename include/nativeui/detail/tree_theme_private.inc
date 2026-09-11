    // Theme is ordinary per-tree value state. It intentionally precedes the
    // retained root in member declaration order so borrowed component theme
    // views cannot outlive it during Tree destruction.
    Theme theme_{default_theme()};

    void bind_theme(Node& node) noexcept {
        if (auto* binding = dynamic_cast<detail::ThemeBinding*>(node.component.get())) {
            binding->bind_theme(theme_);
        }
        for (auto& child : node.children) bind_theme(*child);
    }
