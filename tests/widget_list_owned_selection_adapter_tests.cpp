#include "test_support.hpp"
#include <nativeui/detail/list_view_kernel.hpp>
#include <functional>
#include <memory>

namespace {
struct Hooks {
    bool copying{}, assigned_inside_copy{}, copy_armed{}, retire_armed{};
    int activation_calls{};
    std::function<void()> replace, retire;
};
struct Key {
    std::shared_ptr<const int> value;
    std::shared_ptr<Hooks> hooks;
    bool source_storage{};
    Key(int number, std::shared_ptr<Hooks> state, bool source = false)
        : value(std::make_shared<const int>(number)), hooks(std::move(state)),
          source_storage(source) {}
    Key(const Key &other) : value(other.value), hooks(other.hooks), source_storage(false) {
        // Pin every observed payload before invoking the hook. No access to
        // other or the State's borrowed storage occurs after replacement.
        const auto retained = hooks;
        const bool trigger =
            other.source_storage && *value == 1 && std::exchange(retained->copy_armed, false);
        if (trigger) {
            const auto replace = retained->replace;
            retained->copying = true;
            try {
                if (replace)
                    replace();
            } catch (...) {
                retained->copying = false;
                throw;
            }
            retained->copying = false;
        }
    }
    Key(Key &&) = default;
    Key &operator=(const Key &other) {
        Key owned{other};
        return *this = std::move(owned);
    }
    Key &operator=(Key &&other) noexcept {
        if (source_storage && hooks && hooks->copying)
            hooks->assigned_inside_copy = true;
        value = std::move(other.value);
        hooks = std::move(other.hooks);
        source_storage = other.source_storage;
        return *this;
    }
    bool operator==(const Key &other) const {
        const auto first = value, second = other.value;
        const auto state = hooks;
        if (*first != *second && std::exchange(state->retire_armed, false)) {
            const auto retire = state->retire;
            if (retire)
                retire();
        }
        return *first == *second;
    }
};
void copied_snapshot_stabilizes_after_reentrant_assignment() {
    const auto hooks = std::make_shared<Hooks>();
    ui::State<std::optional<Key>> source{Key{1, hooks, true}};
    auto keys =
        std::make_shared<const std::vector<Key>>(std::vector<Key>{Key{1, hooks}, Key{2, hooks}});
    ui::detail::TypedListSelection<Key> adapter{
        source.binding(), keys, [hooks](const Key &) { ++hooks->activation_calls; }};
    hooks->replace = [&] { source.set(Key{2, hooks, true}); };
    hooks->copy_armed = true;
    const auto mask = adapter.snapshot();
    NUI_CHECK(!hooks->assigned_inside_copy);
    NUI_CHECK(*source.get()->value == 2 && mask == std::vector<bool>({false, true}));
}
void equality_that_retires_the_recipient_cannot_commit_or_activate() {
    const auto hooks = std::make_shared<Hooks>();
    ui::State<std::optional<Key>> source{Key{1, hooks, true}};
    auto keys =
        std::make_shared<const std::vector<Key>>(std::vector<Key>{Key{1, hooks}, Key{2, hooks}});
    ui::detail::TypedListSelection<Key> adapter{
        source.binding(), keys, [hooks](const Key &) { ++hooks->activation_calls; }};
    bool mounted = true;
    hooks->retire = [&] { mounted = false; };
    hooks->retire_armed = true;
    adapter.publish(1, true, [&](bool) { return mounted; });
    NUI_CHECK(!mounted && *source.get()->value == 1 && source.revision() == 0 &&
              hooks->activation_calls == 0);
    mounted = true;
    adapter.publish(1, true, [&](bool) { return mounted; });
    NUI_CHECK(*source.get()->value == 2 && source.revision() == 1 && hooks->activation_calls == 1);
}
void arbitrary_key_copy_can_expire_the_source_owner_without_unsafe_access() {
    const auto hooks = std::make_shared<Hooks>();
    auto source = std::make_unique<ui::State<std::optional<Key>>>(Key{1, hooks, true});
    auto binding = source->binding();
    auto keys =
        std::make_shared<const std::vector<Key>>(std::vector<Key>{Key{1, hooks}, Key{2, hooks}});
    ui::detail::TypedListSelection<Key> adapter{
        binding, keys, [hooks](const Key &) { ++hooks->activation_calls; }};
    hooks->replace = [&] { source.reset(); };
    hooks->copy_armed = true;
    const auto mask = adapter.snapshot();
    NUI_CHECK(!binding.valid() && mask == std::vector<bool>({true, false}));
    adapter.publish(1, true, [](bool) { return true; });
    NUI_CHECK(*binding.get()->value == 1 && hooks->activation_calls == 0);
}
void suite() {
    copied_snapshot_stabilizes_after_reentrant_assignment();
    equality_that_retires_the_recipient_cannot_commit_or_activate();
    arbitrary_key_copy_can_expire_the_source_owner_without_unsafe_access();
}
} // namespace
int main(int argc, char **argv) {
    if (argc > 1 && std::string_view{argv[1]} == "copy")
        return test::run("list_owned_selection_adapter_copy",
                         &copied_snapshot_stabilizes_after_reentrant_assignment);
    if (argc > 1 && std::string_view{argv[1]} == "retire")
        return test::run("list_owned_selection_adapter_retire",
                         &equality_that_retires_the_recipient_cannot_commit_or_activate);
    if (argc > 1 && std::string_view{argv[1]} == "owner")
        return test::run("list_owned_selection_adapter_owner",
                         &arbitrary_key_copy_can_expire_the_source_owner_without_unsafe_access);
    return test::run("list_owned_selection_adapter", &suite);
}
