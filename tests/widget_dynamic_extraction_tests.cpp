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
struct CopyLifetime {
    bool armed{};
    bool copying{};
    bool input_retired{};
    std::function<void()> replace;
};
struct CopyChoice {
    int value;
    std::shared_ptr<CopyLifetime> lifetime;
    bool source_value{};
    CopyChoice(int value,std::shared_ptr<CopyLifetime> lifetime,bool source_value=false)
        : value(value),lifetime(std::move(lifetime)),source_value(source_value) {}
    CopyChoice(const CopyChoice& other)
        : value(other.value),lifetime(other.lifetime) {
        const auto owned=lifetime;
        if (!other.source_value || !owned || !std::exchange(owned->armed,false)) return;
        const auto replace=owned->replace;
        owned->copying=true;
        try {
            if (replace) replace();
            owned->copying=false;
            // Stop the source traversal immediately if the framework retired
            // its input. This oracle never reads the retired element again.
            if (owned->input_retired) throw std::runtime_error("dynamic copy input retired");
        } catch (...) {
            owned->copying=false;
            throw;
        }
    }
    CopyChoice(CopyChoice&&) noexcept = default;
    CopyChoice& operator=(const CopyChoice&) = default;
    CopyChoice& operator=(CopyChoice&& other) noexcept {
        if (source_value && lifetime && lifetime->copying) lifetime->input_retired=true;
        value=other.value;
        lifetime=std::move(other.lifetime);
        source_value=other.source_value;
        return *this;
    }
    ~CopyChoice() {
        if (source_value && lifetime && lifetime->copying) lifetime->input_retired=true;
    }
    friend bool operator==(const CopyChoice& first,const CopyChoice& second) {
        return first.value==second.value;
    }
};
void switch_copy_keeps_its_input_alive_until_the_snapshot_is_owned() {
    const auto lifetime=std::make_shared<CopyLifetime>();
    const auto log=std::make_shared<Observation>();
    ui::State<CopyChoice> source{CopyChoice{1,lifetime,true}};
    auto recipe=ui::make_spec(ui::Switch{source}.when(CopyChoice{1,lifetime},Probe{"first",log})
        .when(CopyChoice{2,lifetime},Probe{"second",log}));
    lifetime->replace=[&] { source.set(CopyChoice{2,lifetime,true}); };
    lifetime->armed=true;
    bool retired{};
    try {
        ui::UI tree{std::move(recipe)};
        tree.resize({120.0f,80.0f});
        NUI_CHECK(source.get().value==2 && log->identities["second"].size()==1);
        source.set(CopyChoice{1,lifetime,true});
        tree.resize({120.0f,80.0f});
        NUI_CHECK(log->identities["first"].size()==2);
    } catch (const std::runtime_error&) { retired=true; }
    NUI_CHECK(!retired && !lifetime->input_retired);
}
void foreach_copy_cannot_retire_the_source_iteration() {
    const auto lifetime=std::make_shared<CopyLifetime>();
    const auto log=std::make_shared<Observation>();
    std::vector<CopyChoice> initial;
    initial.emplace_back(1,lifetime,true);
    initial.emplace_back(2,lifetime,true);
    ui::State<std::vector<CopyChoice>> source{std::move(initial)};
    lifetime->replace=[&] {
        std::vector<CopyChoice> replacement;
        replacement.emplace_back(3,lifetime,true);
        source.set(std::move(replacement));
    };
    lifetime->armed=true;
    bool retired{};
    try {
        ui::UI tree{ui::ForEach{source,[](const CopyChoice& value) { return value.value; },
            [log](const CopyChoice& value) { return Probe{std::to_string(value.value),log}; }}};
        tree.resize({120.0f,80.0f});
        std::vector<CopyChoice> replacement;
        replacement.emplace_back(4,lifetime,true);
        source.set(std::move(replacement));
        tree.resize({120.0f,80.0f});
        NUI_CHECK(log->identities["4"].size()==1);
    } catch (const std::runtime_error&) { retired=true; }
    NUI_CHECK(!retired && !lifetime->input_retired);
}
struct MoveOnlyItem {
    int key;
    std::unique_ptr<int> payload;
    explicit MoveOnlyItem(int value):key(value),payload(std::make_unique<int>(value)) {}
    MoveOnlyItem(MoveOnlyItem&&) noexcept=default;
    MoveOnlyItem& operator=(MoveOnlyItem&&) noexcept=default;
    bool operator==(const MoveOnlyItem& other) const { return key==other.key; }
};
void foreach_move_only_factory_reads_stable_items_during_replacement() {
    const auto log=std::make_shared<Observation>();
    std::vector<MoveOnlyItem> initial;
    initial.emplace_back(1); initial.emplace_back(2);
    ui::State<std::vector<MoveOnlyItem>> source{std::move(initial)};
    bool replace=true;
    ui::UI tree{ui::ForEach{source,[](const MoveOnlyItem& item) { return item.key; },
        [&](const MoveOnlyItem& item) {
            const auto key=item.key;
            log->built.push_back(std::to_string(key));
            if (std::exchange(replace,false)) {
                std::vector<MoveOnlyItem> replacement;
                replacement.emplace_back(3); replacement.emplace_back(4);
                source.set(std::move(replacement));
                NUI_CHECK(source.get().size()==2 && source.get()[0].key==1);
            }
            NUI_CHECK(item.key==key && *item.payload==key);
            return Probe{std::to_string(key),log};
        }}};
    tree.resize({120.0f,80.0f});
    NUI_CHECK(log->built.size()>=2 && log->built[0]=="1" && log->built[1]=="2");
    NUI_CHECK(source.get().size()==2 && source.get()[0].key==3);
    NUI_CHECK(log->identities["3"].size()==1 && log->identities["4"].size()==1);
}
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
    switch_copy_keeps_its_input_alive_until_the_snapshot_is_owned();
    foreach_copy_cannot_retire_the_source_iteration();
    foreach_move_only_factory_reads_stable_items_during_replacement();
    delayed_and_copied_specs_use_live_sources_and_new_instance_identities();
    foreach_snapshot_survives_reentrant_factory_and_preserves_equal_keys();
    enum_integral_and_string_keys_remain_distinct_encodings();
}
}
int main(int argc,char** argv) {
    if (argc>1 && std::string_view{argv[1]}=="switch_copy")
        return test::run("dynamic_switch_copy",&switch_copy_keeps_its_input_alive_until_the_snapshot_is_owned);
    if (argc>1 && std::string_view{argv[1]}=="foreach_copy")
        return test::run("dynamic_foreach_copy",&foreach_copy_cannot_retire_the_source_iteration);
    return test::run("widget_dynamic_extraction",&suite);
}
