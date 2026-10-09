#include <nativeui/cached_layer.hpp>

#include <memory>
#include <type_traits>
#include <vector>

namespace {

struct ThemeValue {
    int accent{};
    bool operator==(const ThemeValue&) const = default;
};

class HeaderLeafComponent final : public ui::Component {
public:
    [[nodiscard]] ui::Size measure(
        const std::vector<ui::ChildMetrics>&) const override {
        return {20.0f, 10.0f};
    }

    void paint(ui::PaintContext&) const override {}
};

struct HeaderLeaf {
    ui::Spec spec() && {
        return {[] { return std::make_unique<HeaderLeafComponent>(); }, {}};
    }
};

template <class Source>
concept AcceptedDependency = requires(Source& source) {
    ui::CachedLayer{HeaderLeaf{}}.depends(source);
};

static_assert(AcceptedDependency<ui::State<ThemeValue>>);
static_assert(AcceptedDependency<ui::State<int>>);
static_assert(!AcceptedDependency<int>);
static_assert(!AcceptedDependency<ui::Binding<int>>);

[[maybe_unused]] ui::Spec heterogeneous_dependencies() {
    ui::State<ThemeValue> theme{ThemeValue{1}};
    ui::State<int> revision{0};
    return ui::CachedLayer{HeaderLeaf{}}.depends(theme, revision, theme).spec();
}

[[maybe_unused]] ui::Spec zero_dependencies() {
    return ui::CachedLayer{HeaderLeaf{}}.spec();
}

} // namespace
