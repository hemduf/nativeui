namespace detail {

inline std::unique_ptr<Node> compile_node(Spec spec, NodeId& next_id, Node* parent = nullptr) {
    auto node = std::make_unique<Node>();
    node->id = next_id++;
    node->parent = parent;
    node->component = spec.factory();
    node->children.reserve(spec.children.size());
    for (auto& child : spec.children) {
        node->children.push_back(compile_node(std::move(child), next_id, node.get()));
    }
    return node;
}

} // namespace detail

inline std::unique_ptr<Node> compile(Spec spec) {
    NodeId next_id = 1;
    return detail::compile_node(std::move(spec), next_id);
}


