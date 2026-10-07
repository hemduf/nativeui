    struct DescendantActionRequest {
        NodeId owner{kInvalidNodeId};
        NodeId target{kInvalidNodeId};
        std::optional<SemanticAction> action;
        bool focus_requested{};
        std::uint64_t identity{};
    };
    std::vector<DescendantActionRequest> pending_descendant_actions_;
    bool draining_descendant_actions_{};
    std::uint64_t next_descendant_action_identity_{1};

    [[nodiscard]] static ComponentAvailability current_local_availability(const Node& node) noexcept {
        const auto inherited = node.parent ? current_local_availability(*node.parent) : ComponentAvailability{};
        return combine_availability(inherited, node.component->local_availability());
    }
    [[nodiscard]] static bool eligible_label_control(const Node& node, const SemanticInfo& info) noexcept {
        if (node.component->focusable()) return true;
        if (info.role == SemanticRole::None || info.role == SemanticRole::Text ||
            info.role == SemanticRole::Image || info.role == SemanticRole::TabPanel) return false;
        return node.component->focusable() || info.focusable ||
            (info.role == SemanticRole::Group &&
             (info.supports(SemanticAction::Activate) || info.supports(SemanticAction::Toggle) || info.supports(SemanticAction::Select)));
    }
    [[nodiscard]] Node* first_scoped_control(Node& node, bool include_self) {
        if (include_self) {
            if (node.component->descendant_semantic_decoration()) return nullptr;
            const auto info = node.component->semantics();
            if (eligible_label_control(node, info)) return &node;
        }
        for (auto& child : node.children) {
            if (auto* result = first_scoped_control(*child, true)) return result;
        }
        return nullptr;
    }
    void find_scoped_key(Node& node, const std::string& key, Node*& result, std::size_t& matches) {
        for (auto& child : node.children) {
            if (child->component->descendant_semantic_decoration()) continue;
            if (child->retained_key == key) { ++matches; result = child.get(); }
            find_scoped_key(*child, key, result, matches);
        }
    }
    [[nodiscard]] Node* resolve_descendant_control(Node& owner, const std::string& key) {
        if (key.empty()) return first_scoped_control(owner, false);
        Node* keyed_node = nullptr; std::size_t matches{};
        find_scoped_key(owner, key, keyed_node, matches);
        return matches == 1 ? first_scoped_control(*keyed_node, true) : nullptr;
    }
    [[nodiscard]] bool descendant_target_available(Node& owner, Node& target) const noexcept {
        if (!active_ || !is_descendant_or_self(&target, &owner) ||
            dynamic_teardown_quarantines(&target) || !current_local_availability(target).interactive()) return false;
        for (auto* current = &target; current; current = current->parent) {
            if (current->component->is_focus_scope() && !current->component->focus_scope_active()) return false;
        }
        if (auto* trap = trapping_scope_for(current_focus_node()); trap &&
            !is_descendant_or_self(&target, trap) && trapping_scope_for(&target) == nullptr) return false;
        return true;
    }
    [[nodiscard]] static std::optional<SemanticAction> label_mutation(const SemanticInfo& info) noexcept {
        if (!info.enabled || info.read_only) return {};
        if (info.role == SemanticRole::Checkbox || info.role == SemanticRole::Toggle) {
            if (info.supports(SemanticAction::Toggle)) return SemanticAction::Toggle;
            if (info.supports(SemanticAction::Activate)) return SemanticAction::Activate;
        }
        if (info.role == SemanticRole::RadioButton) {
            if (info.supports(SemanticAction::Select)) return SemanticAction::Select;
            if (info.supports(SemanticAction::Activate)) return SemanticAction::Activate;
        }
        return {};
    }
    [[nodiscard]] SemanticInfo decorated_component_semantics(Node& node) {
        auto info = node.component->semantics();
        for (auto* ancestor = node.parent; ancestor; ancestor = ancestor->parent) {
            const auto decoration = ancestor->component->descendant_semantic_decoration();
            if (!decoration) continue;
            if (resolve_descendant_control(*ancestor, decoration->target_key) != &node) continue;
            if (decoration->expanded) info.expanded = *decoration->expanded
                ? SemanticExpandedState::Expanded : SemanticExpandedState::Collapsed;
            if (info.name.empty()) info.name = decoration->name;
            if (!decoration->description.empty() &&
                (!decoration->description_if_empty || info.description.empty())) {
                if (!info.description.empty()) info.description += '\n';
                info.description += decoration->description;
            }
        }
        return info;
    }
    void request_descendant_action(NodeId owner_id, const std::string& key) {
        if (!active_) return;
        NodeId target_id{kInvalidNodeId}; std::optional<SemanticAction> action;
        {
            SemanticOperationScope guard{*this};
            auto* owner = retained_invalidation_node(owner_id); if (!owner) return;
            auto* target = resolve_descendant_control(*owner, key);
            if (!target || !descendant_target_available(*owner, *target)) return;
            auto info = target->component->semantics();
            if (!info.enabled) return;
            if (!current_local_availability(*target).read_only) action = label_mutation(info);
            if (!target->component->focusable() && !action) return;
            target_id = target->id;
            if (next_descendant_action_identity_ == std::numeric_limits<std::uint64_t>::max())
                throw std::overflow_error("descendant action identity exhausted");
            pending_descendant_actions_.push_back({owner_id,target_id,action,false,next_descendant_action_identity_});
            ++next_descendant_action_identity_;
        }
        // Request publication is retained before exposure; a throwing observer
        // does not lose the intent. The weak owner is resolved again after reads.
        if (auto* owner = retained_invalidation_node(owner_id)) invalidate_node_paint(*owner);
    }
    static std::function<void()> retained_descendant_action_factory(void* owner, NodeId id, std::string key) {
        const auto weak = static_cast<Tree*>(owner)->retained_invalidation_weak();
        return [weak,id,key=std::move(key)] {
            const auto lifetime = weak.lock(); if (!lifetime || !lifetime->tree) return;
            lifetime->tree->request_descendant_action(id,key);
        };
    }
    [[nodiscard]] detail::InputCapability retained_input_capability(NodeId id,
        PointerId pointer = 0, std::uint64_t contact = 0, std::uint64_t activation = 0) {
        return {this, {}, id, pointer, contact, activation, &Tree::retain_input_owner,
                &Tree::check_input_capability};
    }
    static std::weak_ptr<void> retain_input_owner(void* owner) {
        return static_cast<Tree*>(owner)->retained_invalidation_weak();
    }
    static bool check_input_capability(const detail::InputCapability& capability,
        bool mutation) noexcept {
        const auto lifetime = capability.immediate_owner ? std::shared_ptr<void>{}
                                                        : capability.owner.lock();
        auto* tree = capability.immediate_owner ? static_cast<Tree*>(capability.immediate_owner)
            : lifetime ? static_cast<RetainedInvalidationLifetime*>(lifetime.get())->tree : nullptr;
        if (!tree || !tree->active_) return false;
        if (capability.activation && capability.activation != tree->activation_epoch_) return false;
        auto* node = tree->retained_invalidation_node(capability.node);
        if (!node || tree->dynamic_teardown_quarantines(node)) return false;
        const auto availability = current_local_availability(*node);
        if (!availability.interactive() || (mutation && availability.read_only)) return false;
        for (auto* ancestor = node; ancestor; ancestor = ancestor->parent) {
            if (ancestor->component->is_focus_scope() &&
                !ancestor->component->focus_scope_active()) return false;
        }
        return capability.contact == 0 ||
            tree->pointer_interaction_dispatch_token(capability.pointer) == capability.contact;
    }
    static detail::InputCapability retained_input_capability_factory(void* owner, NodeId id) {
        return static_cast<Tree*>(owner)->retained_input_capability(id);
    }
    // Frame consumption must retain and expose a following frame for an
    // unstarted suffix. Commit is noexcept; notification failure leaves damage
    // durable, and no state is accessed after the application callback.
    void retain_descendant_action_frame() noexcept {
        if (!active_ || pending_descendant_actions_.empty()) return;
        const auto dirty = retain_dirty(viewport_rect());
        if (!dirty) return;
        try {
            auto callback = invalidation_callback_;
            if (callback) callback(*dirty);
        } catch (...) {}
    }
    [[nodiscard]] bool descendant_request_is_current(const DescendantActionRequest& request) const noexcept {
        return active_ && !pending_descendant_actions_.empty() &&
            pending_descendant_actions_.front().identity == request.identity;
    }
    void drain_descendant_actions(PlatformServices& platform) {
        if (pending_descendant_actions_.empty() || draining_descendant_actions_ ||
            semantic_operation_running_ || !active_) return;
        draining_descendant_actions_ = true;
        try {
            const auto count = std::min<std::size_t>(pending_descendant_actions_.size(),32);
            for (std::size_t pass = 0; pass < count && !pending_descendant_actions_.empty(); ++pass) {
                auto request = pending_descendant_actions_.front();
                flush_dynamic_mutations(); sync_availability_structure(platform);
                if (!descendant_request_is_current(request)) continue;
                auto* owner = retained_invalidation_node(request.owner);
                auto* target = retained_invalidation_node(request.target);
                if (!owner || !target || !descendant_target_available(*owner,*target)) {
                    pending_descendant_actions_.erase(pending_descendant_actions_.begin()); continue;
                }
                if (!request.focus_requested) {
                    pending_descendant_actions_.front().focus_requested = true;
                    if (target->component->focusable()) queue_pending_focus(target->id);
                }
                resume_focus_transitions(platform);
                if (!descendant_request_is_current(request)) continue;
                if (pending_focus_request_ || !focus_transition_stack_.empty()) break;
                flush_dynamic_mutations(); sync_availability_structure(platform);
                if (!descendant_request_is_current(request)) continue;
                owner = retained_invalidation_node(request.owner); target = retained_invalidation_node(request.target);
                if (!owner || !target || !descendant_target_available(*owner,*target) ||
                    (target->component->focusable() && current_focus_node() != target) || !request.action) {
                    pending_descendant_actions_.erase(pending_descendant_actions_.begin()); continue;
                }
                const auto epoch = structural_epoch_;
                bool retry{};
                {
                    SemanticOperationScope guard{*this};
                    const auto info = target->component->semantics();
                    if (structural_epoch_ != epoch) retry = true;
                    else if (!descendant_target_available(*owner,*target) || !info.enabled || info.read_only ||
                             current_local_availability(*target).read_only || label_mutation(info) != request.action) {
                        pending_descendant_actions_.erase(pending_descendant_actions_.begin());
                    } else {
                        InputContext context{target->bounds,platform,paint_invalidator(target->id),
                                             layout_invalidator(target->id),{}, {}};
                        const auto weak = retained_invalidation_weak();
                        context.semantic_mutation_allowed_ = [weak,request,epoch] {
                            const auto lifetime = weak.lock(); if (!lifetime || !lifetime->tree) return false;
                            auto* tree = lifetime->tree;
                            auto* live_owner = tree->retained_invalidation_node(request.owner);
                            auto* live = tree->retained_invalidation_node(request.target);
                            return live_owner && live && tree->structural_epoch_ == epoch &&
                                tree->descendant_target_available(*live_owner,*live) &&
                                !current_local_availability(*live).read_only;
                        };
                        // All preparation precedes consumption. A begun action
                        // is never replayed, including a throwing application setter.
                        pending_descendant_actions_.erase(pending_descendant_actions_.begin());
                        (void)target->component->semantic_action(*request.action,context);
                    }
                }
                if (retry) continue;
            }
            draining_descendant_actions_ = false;
        } catch (...) { draining_descendant_actions_ = false; throw; }
    }
