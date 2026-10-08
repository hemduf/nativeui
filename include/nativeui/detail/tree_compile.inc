namespace detail {
inline void bind_compiled_ancestry(Node* parent, Component& descendant) {
    if (!parent) return;
    bind_compiled_ancestry(parent->parent, descendant);
    ComponentContextAccess::bind(*parent->component, descendant);
}


inline std::unique_ptr<Node> compile_node_with_policy(
    Spec spec, NodeId& next_id, Node* parent, bool interaction_allowed) {
    auto node = std::make_unique<Node>();
    node->id = next_id++;
    node->parent = parent;
    node->retained_key = std::move(spec.retained_key);
    node->component = spec.factory();
    if (!node->component) throw std::invalid_argument("Component factory returned null");
    bind_compiled_ancestry(parent, *node->component);
    if (!interaction_allowed &&
        (node->component->focusable() || node->component->pointer_targetable())) {
        throw std::invalid_argument("Decorative content contains an interactive descendant");
    }
    auto children = spec.children_factory ? spec.children_factory(*node->component)
                                          : std::move(spec.children);
    const bool child_interaction = interaction_allowed && node->component->allows_child_interaction();
    node->children.reserve(children.size());
    for (auto& child : children) {
        node->children.push_back(compile_node_with_policy(std::move(child), next_id, node.get(), child_interaction));
    }
    return node;
}

inline std::unique_ptr<Node> compile_node(Spec spec, NodeId& next_id, Node* parent = nullptr) {
    bool interaction_allowed = true;
    for (auto* ancestor = parent; ancestor; ancestor = ancestor->parent) {
        if (ancestor->component && !ancestor->component->allows_child_interaction()) {
            interaction_allowed = false;
            break;
        }
    }
    return compile_node_with_policy(std::move(spec), next_id, parent, interaction_allowed);
}

} // namespace detail

inline std::unique_ptr<Node> compile(Spec spec) {
    NodeId next_id = 1;
    return detail::compile_node(std::move(spec), next_id);
}

