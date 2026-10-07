    [[nodiscard]] static detail::FocusGroupParticipant* focus_group_participant(
        Node* node) noexcept {
        if (!node) return nullptr;
        if (auto* direct = dynamic_cast<detail::FocusGroupParticipant*>(node->component.get()))
            return direct;
        if (!node->component->roving_focus_target()) return nullptr;
        for (auto* ancestor = node->parent; ancestor; ancestor = ancestor->parent) {
            if (auto* participant = dynamic_cast<detail::FocusGroupParticipant*>(
                    ancestor->component.get())) return participant;
        }
        return nullptr;
    }

    [[nodiscard]] static const detail::FocusGroupParticipant* focus_group_participant(
        const Node* node) noexcept {
        return focus_group_participant(const_cast<Node*>(node));
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
        std::size_t start = current ? focused_index_ : 0;
        // Each group has one Tab stop at its first admitted member, even when
        // independent editors occur between members. The stop enters its cache.
        if (current_group) {
            for (std::size_t i = 0; i < count; ++i) {
                auto* member = focusables_[i];
                if (focus_target_available(member) &&
                    (!trap || is_descendant_or_self(member, trap)) &&
                    focus_group_identity(member) == current_group) { start = i; break; }
            }
        }

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
                bool representative = true;
                for (std::size_t i = 0; i < index; ++i) {
                    auto* member = focusables_[i];
                    if (focus_target_available(member) &&
                        (!trap || is_descendant_or_self(member, trap)) &&
                        focus_group_identity(member) == candidate_group) {
                        representative = false; break;
                    }
                }
                if (!representative) continue;
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
        int direction, PlatformServices& platform, bool boundary = false) {
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
            const auto index = boundary
                ? (direction > 0 ? count - step : step - 1)
                : (direction > 0 ? (start + step) % count
                                 : (start + count - (step % count)) % count);
            auto* candidate = focusables_[index];
            if (!focus_target_available(candidate)) continue;
            if (trap && !is_descendant_or_self(candidate, trap)) continue;
            auto* participant = focus_group_participant(candidate);
            if (!participant || participant->focus_group_identity() != identity) continue;

            const auto target_id = candidate->id;
            const auto input_activation = activation_epoch_;
            const auto selection_revision = participant->focus_group_selection_revision();
            set_focus(candidate, platform);
            // Focus exposure can retire a node or start a new activation.
            // Resolve the selected identity again before any value publication.
            candidate = retained_invalidation_node(target_id);
            if (!active_ || activation_epoch_ != input_activation ||
                !candidate || current_focus_node() != candidate ||
                !focus_target_available(candidate)) return true;
            participant = focus_group_participant(candidate);
            if (!participant || participant->focus_group_identity() != identity ||
                participant->focus_group_selection_revision() != selection_revision) return true;
            if (select_value && !current_local_availability(*candidate).read_only) {
                const auto capability = retained_input_capability(
                    target_id, 0, 0, input_activation).retained();
                participant->focus_group_select_guarded(
                    [capability] { return capability.allowed(true); });
            }
            return true;
        }
        return false;
    }
