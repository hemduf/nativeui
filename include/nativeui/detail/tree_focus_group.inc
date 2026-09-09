    [[nodiscard]] static detail::FocusGroupParticipant* focus_group_participant(
        Node* node) noexcept {
        return node
            ? dynamic_cast<detail::FocusGroupParticipant*>(node->component.get())
            : nullptr;
    }

    [[nodiscard]] static const detail::FocusGroupParticipant* focus_group_participant(
        const Node* node) noexcept {
        return node
            ? dynamic_cast<const detail::FocusGroupParticipant*>(node->component.get())
            : nullptr;
    }

    [[nodiscard]] const void* focus_group_identity(const Node* node) const noexcept {
        const auto* participant = focus_group_participant(node);
        return participant ? participant->focus_group_identity() : nullptr;
    }

    [[nodiscard]] Node* focus_group_entry(
        const void* identity, Node* trap = nullptr) const {
        if (!identity) return nullptr;

        Node* first = nullptr;
        for (auto* candidate : focusables_) {
            if (!focus_target_available(candidate)) continue;
            if (trap && !is_descendant_or_self(candidate, trap)) continue;
            auto* participant = focus_group_participant(candidate);
            if (!participant || participant->focus_group_identity() != identity) continue;
            if (!first) first = candidate;
            if (participant->focus_group_selected()) return candidate;
        }
        return first;
    }

    void move_focus_group_aware(int direction, PlatformServices& platform) {
        if (focusables_.empty()) return;
        auto* current = current_focus_node();
        auto* trap = trapping_scope_for(current);
        const void* current_group = focus_group_identity(current);
        const auto count = focusables_.size();
        const std::size_t start = current ? focused_index_ : 0;

        for (std::size_t step = 1; step <= count; ++step) {
            const auto index = direction > 0
                ? (start + step) % count
                : (start + count - (step % count)) % count;
            auto* candidate = focusables_[index];
            if (!focus_target_available(candidate)) continue;
            if (trap && !is_descendant_or_self(candidate, trap)) continue;

            const void* candidate_group = focus_group_identity(candidate);
            if (current_group && candidate_group == current_group) continue;

            if (candidate_group) {
                if (auto* entry = focus_group_entry(candidate_group, trap)) {
                    set_focus(entry, platform);
                    return;
                }
                continue;
            }

            set_focus(candidate, platform);
            return;
        }
    }

    [[nodiscard]] bool move_focus_in_current_group(
        int direction, PlatformServices& platform) {
        auto* current = current_focus_node();
        const void* identity = focus_group_identity(current);
        if (!current || !identity || focusables_.empty()) return false;

        // ReadOnly keeps the roving focus behavior but suppresses the value
        // write that normally accompanies radio arrow navigation.
        const bool select_value = !current->component->effective_read_only();
        auto* trap = trapping_scope_for(current);
        const auto count = focusables_.size();
        const auto current_it = std::find(focusables_.begin(), focusables_.end(), current);
        if (current_it == focusables_.end()) return false;
        const auto start = static_cast<std::size_t>(std::distance(focusables_.begin(), current_it));

        for (std::size_t step = 1; step <= count; ++step) {
            const auto index = direction > 0
                ? (start + step) % count
                : (start + count - (step % count)) % count;
            auto* candidate = focusables_[index];
            if (!focus_target_available(candidate)) continue;
            if (trap && !is_descendant_or_self(candidate, trap)) continue;
            auto* participant = focus_group_participant(candidate);
            if (!participant || participant->focus_group_identity() != identity) continue;

            set_focus(candidate, platform);
            // State observers may synchronously mutate UI state. Availability
            // reconciliation remains deferred until dispatch completion, and no
            // candidate/tree node is touched after this call.
            if (select_value) participant->focus_group_select();
            return true;
        }
        return false;
    }
