#include "test_support.hpp"
#include <nativeui/if.hpp>
#include <nativeui/switch.hpp>
#include <nativeui/for_each.hpp>

#include <map>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace {
struct Observation {
    std::map<std::string,std::vector<ui::NodeId>> identities;
    std::map<std::string,int> unmounts;
    std::vector<std::string> built;
};
class ProbeComponent final : public ui::Component {
public:
    ProbeComponent(std::string key,std::shared_ptr<Observation> log) : key_(std::move(key)),log_(std::move(log)) {}
    [[nodiscard]] ui::Size measure(const std::vector<ui::ChildMetrics>&) const override { return {80.0f,40.0f}; }
    void mount(ui::MountContext& context) override { log_->identities[key_].push_back(context.node_id()); }
    void unmount(ui::LifecycleContext&) override { ++log_->unmounts[key_]; }
    void paint(ui::PaintContext&) const override {}
private:
    std::string key_;
    std::shared_ptr<Observation> log_;
};
class Probe {
public:
    Probe(std::string key,std::shared_ptr<Observation> log) : key_(std::move(key)),log_(std::move(log)) {}
    ui::Spec spec() && {
        const auto key = key_; const auto log = log_;
        return {[key,log] { return std::make_unique<ProbeComponent>(key,log); },{}};
    }
private:
    std::string key_;
    std::shared_ptr<Observation> log_;
};
struct CustomKey {
    int value;
    std::shared_ptr<bool> fail;
    friend bool operator==(const CustomKey& first,const CustomKey& second) {
        if (*first.fail || *second.fail) throw std::runtime_error("custom equality");
        return first.value == second.value;
    }
};
void constructors_ctad_lvalue_branches_first_match_and_last_fallback_are_preserved() {
    auto log = std::make_shared<Observation>();
    auto fault = std::make_shared<bool>(false);
    ui::State<CustomKey> selection{{1,fault}};
    auto builder = ui::Switch{selection};
    static_assert(std::is_same_v<decltype(builder),ui::Switch<CustomKey>>);
    builder.when({1,fault},Probe{"first",log}).when({1,fault},Probe{"duplicate",log});
    builder.otherwise(Probe{"old-fallback",log}).otherwise(Probe{"fallback",log});
    ui::UI tree{std::move(builder)};
    tree.resize({120.0f,80.0f});
    NUI_CHECK(log->identities["first"].size() == 1);
    NUI_CHECK(log->identities["duplicate"].empty());
    selection.set({2,fault}); tree.resize({120.0f,80.0f});
    NUI_CHECK(log->identities["fallback"].size() == 1 && log->identities["old-fallback"].empty());
    ui::UI empty{ui::Switch{selection.binding()}};
    NUI_CHECK_NEAR(empty.measure().preferred.w,0.0f,0.001f);
    NUI_CHECK_NEAR(empty.measure().preferred.h,0.0f,0.001f);
}
void throwing_equality_preserves_structure_and_later_generation_recovers() {
    auto log = std::make_shared<Observation>();
    auto fault = std::make_shared<bool>(false);
    ui::State<CustomKey> selection{{1,fault}};
    ui::UI tree{ui::Switch{selection}.when({1,fault},Probe{"first",log})
        .when({2,fault},Probe{"second",log})};
    tree.resize({120.0f,80.0f});
    selection.set({2,fault});
    *fault = true;
    bool threw = false;
    try { tree.resize({120.0f,80.0f}); }
    catch (const std::runtime_error&) { threw = true; }
    *fault = false;
    NUI_CHECK(threw && log->identities["second"].empty() && log->unmounts["first"] == 0);
    tree.resize({120.0f,80.0f});
    NUI_CHECK(log->identities["second"].size() == 1 && log->unmounts["first"] == 1);
}
void delayed_and_copied_specs_use_live_sources_and_new_instance_identities() {
    auto log = std::make_shared<Observation>();
    ui::State<bool> shown{false};
    auto conditional = ui::make_spec(ui::If{shown,Probe{"child",log}});
    shown.set(true);
    ui::UI first{ui::Spec{conditional}};
    ui::UI second{ui::Spec{conditional}};
    NUI_CHECK(log->identities["child"].size() == 2);
    shown.set(false); first.resize({120.0f,80.0f}); second.resize({120.0f,80.0f});
    NUI_CHECK(log->unmounts["child"] == 2);
    shown.set(true); first.resize({120.0f,80.0f});
    NUI_CHECK(log->identities["child"].size() == 3);
    NUI_CHECK(log->identities["child"][2] != log->identities["child"][0]);
}
struct Item {
    std::string key;
    std::string data;
    bool operator==(const Item&) const = default;
};
void foreach_snapshot_survives_reentrant_factory_and_preserves_equal_keys() {
    auto log = std::make_shared<Observation>();
    ui::State<std::vector<Item>> items{{{"a","A"},{"b","B"}}};
    bool replace = true;
    ui::UI tree{ui::ForEach{items,[](const Item& item) { return item.key; },[&](const Item& item) {
        const auto key = item.key;
        const auto data = item.data;
        log->built.push_back(data);
        if (replace) {
            replace = false;
            items.set({{"c","C"},{"d","D"},{"e","E"}});
        }
        // item still refers to the immutable pass after application replacement.
        NUI_CHECK(item.key == key && item.data == data);
        return Probe{data,log};
    }}};
    NUI_CHECK(log->built.size() >= 2 && log->built[0] == "A" && log->built[1] == "B");
    tree.resize({120.0f,80.0f});
    NUI_CHECK(log->identities["C"].size() == 1 && log->identities["D"].size() == 1 && log->identities["E"].size() == 1);
    const auto c = log->identities["C"].front();
    items.set({{"e","New E"},{"c","New C"},{"d","New D"}}); tree.resize({120.0f,80.0f});
    NUI_CHECK(log->identities["C"].front() == c && log->identities["New C"].empty());
    items.set({{"c","duplicate"},{"c","duplicate"}}); tree.resize({120.0f,80.0f});
    NUI_CHECK(log->unmounts["C"] == 0);
    items.set({{"c","Valid"}}); tree.resize({120.0f,80.0f});
    NUI_CHECK(log->identities["C"].front() == c && log->unmounts["D"] == 1 && log->unmounts["E"] == 1);
}
void enum_integral_and_string_keys_remain_distinct_encodings() {
    enum class Key : unsigned { One=1 };
    NUI_CHECK(ui::detail::encode_dynamic_key(-1) == "i:-1");
    NUI_CHECK(ui::detail::encode_dynamic_key(1U) == "u:1");
    NUI_CHECK(ui::detail::encode_dynamic_key(Key::One) == "u:1");
    NUI_CHECK(ui::detail::encode_dynamic_key(std::string{"1"}) == "s:1");
    ui::State<std::vector<Key>> source{{Key::One}};
    ui::UI tree{ui::ForEach{source.binding(),[](Key key) { return key; },[](Key) { return ui::Label{"Enum"}; }}};
    ui::HeadlessRenderer renderer{{120.0f,80.0f},1.0f}; NUI_CHECK(renderer.render(tree));
}
void suite() {
    constructors_ctad_lvalue_branches_first_match_and_last_fallback_are_preserved();
    throwing_equality_preserves_structure_and_later_generation_recovers();
    delayed_and_copied_specs_use_live_sources_and_new_instance_identities();
    foreach_snapshot_survives_reentrant_factory_and_preserves_equal_keys();
    enum_integral_and_string_keys_remain_distinct_encodings();
}
}
int main() { return test::run("widget_dynamic_extraction",&suite); }
