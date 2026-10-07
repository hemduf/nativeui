#include "test_support.hpp"

#include <nativeui/state.hpp>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {
class CopyFault final : public std::runtime_error {
public:
  CopyFault() : std::runtime_error("owned State copy fault") {}
};
struct Script {
  std::function<void()> copy;
  std::function<void(int,int)> equality;
};
struct Key {
  int value{};
  std::shared_ptr<Script> script;
  Key() = default;
  Key(int v, std::shared_ptr<Script> s) : value(v), script(std::move(s)) {}
  Key(const Key& other) : value(other.value), script(other.script) {
    // Copy every field needed below before invoking user code. A copy callback
    // may replace a vector owning `other`; the test itself never reads it again.
    if (script) {
      auto callback = std::exchange(script->copy, {});
      if (callback) callback();
    }
  }
  Key(Key&&) noexcept = default;
  Key& operator=(const Key&) = default;
  Key& operator=(Key&&) noexcept = default;
  bool operator==(const Key& other) const {
    const int left = value;
    const int right = other.value;
    const auto lifetime = script ? script : other.script;
    const auto callback = lifetime ? lifetime->equality : std::function<void(int,int)>{};
    if (callback) callback(left,right);
    return left == right;
  }
};

void failed_copy_preserves_previous_observer_write() {
  auto script = std::make_shared<Script>();
  ui::State<Key> owner{Key{0,script}};
  auto binding = owner.binding();
  std::vector<int> observed;
  int faults{};
  auto subscription = binding.observe([&](const Key& key) {
    const int value = key.value;
    observed.push_back(value);
    if (value != 1) return;
    binding.set(Key{2,script});
    script->copy = [&] { binding.set(Key{3,script}); throw CopyFault{}; };
    try { (void)binding.snapshot(); }
    catch (const CopyFault&) { ++faults; }
    NUI_CHECK(binding.get().value == 1 && binding.revision() == 1);
    NUI_CHECK(observed == std::vector<int>{1});
  });
  owner.set(Key{1,script});
  NUI_CHECK(faults == 1 && binding.get().value == 2);
  NUI_CHECK(binding.revision() == 2 && observed == std::vector<int>({1,2}));
  binding.set(Key{4,script});
  NUI_CHECK(binding.revision() == 3 && observed == std::vector<int>({1,2,4}));
}

void nested_failure_preserves_intent(bool outer_writes) {
  auto script = std::make_shared<Script>();
  ui::State<Key> owner{Key{0,script}};
  auto binding = owner.binding();
  std::vector<int> observed;
  int faults{};
  auto subscription = binding.observe([&](const Key& key) {
    const int value = key.value;
    observed.push_back(value);
    if (value != 1) return;
    binding.set(Key{2,script});
    script->copy = [&] {
      if (outer_writes) binding.set(Key{3,script});
      script->copy = [&] { binding.set(Key{4,script}); throw CopyFault{}; };
      try { (void)binding.snapshot(); }
      catch (const CopyFault&) { ++faults; }
      NUI_CHECK(binding.get().value == 1 && binding.revision() == 1);
      NUI_CHECK(observed == std::vector<int>{1});
    };
    const auto owned = binding.snapshot();
    NUI_CHECK(owned.value == 1 && binding.get().value == 1);
  });
  owner.set(Key{1,script});
  const int expected = outer_writes ? 3 : 2;
  NUI_CHECK(faults == 1 && binding.get().value == expected);
  NUI_CHECK(binding.revision() == 2 && observed == std::vector<int>({1,expected}));
}
void nested_failure_suite() {
  nested_failure_preserves_intent(false);
  nested_failure_preserves_intent(true);
}

void nested_success_drains_only_after_outer_copy() {
  auto script = std::make_shared<Script>();
  ui::State<Key> owner{Key{0,script}};
  auto binding = owner.binding();
  std::vector<int> observed;
  auto subscription = binding.observe([&](const Key& key) { observed.push_back(key.value); });
  script->copy = [&] {
    binding.set(Key{1,script});
    script->copy = [&] { binding.set(Key{2,script}); };
    const auto inner = binding.snapshot();
    NUI_CHECK(inner.value == 0 && binding.get().value == 0);
    NUI_CHECK(binding.revision() == 0 && observed.empty());
  };
  const auto outer = owner.snapshot();
  NUI_CHECK(outer.value == 0 && binding.get().value == 2);
  NUI_CHECK(binding.revision() == 1 && observed == std::vector<int>{2});
  binding.set(Key{3,script});
  NUI_CHECK(binding.revision() == 2 && observed == std::vector<int>({2,3}));
}

void destroyed_owner_leaves_copy_storage_pinned() {
  auto script = std::make_shared<Script>();
  auto owner = std::make_unique<ui::State<std::vector<Key>>>(
      std::vector<Key>{Key{1,script},Key{2,script}});
  auto binding = owner->binding();
  int notifications{};
  auto subscription = binding.observe([&](const std::vector<Key>&) { ++notifications; });
  script->copy = [&] {
    owner.reset();
    binding.set(std::vector<Key>{Key{3,script}});
  };
  const auto owned = binding.snapshot();
  NUI_CHECK(!owner && !binding.valid() && !subscription.active());
  NUI_CHECK(owned.size() == 2 && owned[0].value == 1 && owned[1].value == 2);
  NUI_CHECK(binding.get().size() == 2 && binding.revision() == 0 && notifications == 0);
  const auto expired_owned = binding.snapshot();
  NUI_CHECK(expired_owned.size() == 2 && expired_owned[1].value == 2);
}

void vector_copy_replacement_waits_until_all_keys_are_owned() {
  auto script = std::make_shared<Script>();
  ui::State<std::vector<Key>> owner{
      std::vector<Key>{Key{1,script},Key{2,script},Key{3,script}}};
  auto binding = owner.binding();
  int notifications{};
  auto subscription = binding.observe([&](const std::vector<Key>& keys) {
    ++notifications;
    NUI_CHECK(keys.size() == 1 && keys[0].value == 9);
  });
  script->copy = [&] {
    binding.set(std::vector<Key>{Key{9,script}});
    NUI_CHECK(binding.get().size() == 3 && binding.revision() == 0);
    NUI_CHECK(notifications == 0);
  };
  const auto owned = binding.snapshot();
  NUI_CHECK(owned.size() == 3 && owned[0].value == 1 && owned[1].value == 2 && owned[2].value == 3);
  NUI_CHECK(binding.get().size() == 1 && binding.get()[0].value == 9);
  NUI_CHECK(binding.revision() == 1 && notifications == 1);
}

void final_predicate_rejects_after_equality_changes_permission() {
  auto script = std::make_shared<Script>();
  ui::State<Key> owner{Key{0,script}};
  auto binding = owner.binding();
  bool permitted = true;
  int equalities{};
  int predicates{};
  int notifications{};
  auto subscription = binding.observe([&](const Key&) { ++notifications; });
  script->equality = [&](int left,int right) {
    if (left == 1 && right == 0 && ++equalities == 2) {
      permitted = false;
      script->equality = {};
    }
  };
  binding.set_if(Key{1,script},[&] { ++predicates;return permitted; });
  NUI_CHECK(equalities == 2 && predicates == 1 && !permitted);
  NUI_CHECK(binding.get().value == 0 && binding.revision() == 0 && notifications == 0);
  permitted = true;
  owner.set_if(Key{1,script},[&] { ++predicates;return permitted; });
  NUI_CHECK(binding.get().value == 1 && binding.revision() == 1 && notifications == 1);
  NUI_CHECK(predicates == 2);
}

void queued_predicate_declines_without_commit_or_replay() {
  ui::State<int> owner{0};
  auto binding = owner.binding();
  bool permitted = true;
  int predicates{};
  std::vector<int> observed;
  auto subscription = binding.observe([&](int value) {
    observed.push_back(value);
    if (value != 1) return;
    binding.set_if(2,[&] { ++predicates;return permitted; });
    permitted = false;
  });
  owner.set(1);
  NUI_CHECK(binding.get() == 1 && binding.revision() == 1);
  NUI_CHECK(predicates == 1 && observed == std::vector<int>{1});
  NUI_CHECK(binding.snapshot() == 1 && predicates == 1);
  permitted = true;
  binding.set_if(2,[&] { ++predicates;return permitted; });
  NUI_CHECK(binding.get() == 2 && binding.revision() == 2);
  NUI_CHECK(predicates == 2 && observed == std::vector<int>({1,2}));
}

void successful_copy_current_intent_cancels_previous_pending() {
  auto script = std::make_shared<Script>();
  ui::State<Key> owner{Key{0,script}};
  auto binding = owner.binding();
  std::vector<int> observed;
  auto subscription = binding.observe([&](const Key& key) {
    observed.push_back(key.value);
    if (key.value != 1) return;
    binding.set(Key{2,script});
    script->copy = [&] { binding.set(Key{1,script}); };
    const auto owned = binding.snapshot();
    NUI_CHECK(owned.value == 1);
  });
  owner.set(Key{1,script});
  NUI_CHECK(binding.get().value == 1 && binding.revision() == 1);
  NUI_CHECK(observed == std::vector<int>{1});
}

void stable_read_prepares_owned_results_without_copying_move_only_values() {
  std::vector<std::unique_ptr<int>> initial;
  initial.push_back(std::make_unique<int>(1));
  initial.push_back(std::make_unique<int>(2));
  ui::State<std::vector<std::unique_ptr<int>>> owner{std::move(initial)};
  const auto binding=owner.binding();
  int notifications{};
  auto subscription=owner.observe([&](const auto&) { ++notifications; });
  const auto result=ui::detail::StateReadAccess::read(binding,[&](const auto& values) {
    std::vector<std::unique_ptr<int>> replacement;
    replacement.push_back(std::make_unique<int>(3));
    owner.set(std::move(replacement));
    NUI_CHECK(binding.revision()==0 && notifications==0);
    std::vector<int> keys;
    for (const auto& value:values) keys.push_back(*value);
    return keys;
  });
  NUI_CHECK(result==std::vector<int>({1,2}));
  NUI_CHECK(binding.get().size()==1 && *binding.get()[0]==3);
  NUI_CHECK(binding.revision()==1 && notifications==1);
}

void failed_stable_read_preserves_earlier_pending_writes_and_recovers() {
  ui::State<int> owner{0};
  auto binding=owner.binding();
  std::vector<int> observed;
  int failures{};
  auto subscription=owner.observe([&](int value) {
    observed.push_back(value);
    if (value!=1) return;
    binding.set(2);
    try {
      (void)ui::detail::StateReadAccess::read(binding,[&](int current) -> int {
        binding.set(3);
        const auto nested=ui::detail::StateReadAccess::read(binding,[&](int inner) {
          binding.set(4);
          return inner;
        });
        NUI_CHECK(current==1 && nested==1 && binding.get()==1);
        throw CopyFault{};
      });
    } catch (const CopyFault&) { ++failures; }
    NUI_CHECK(binding.get()==1 && binding.revision()==1);
  });
  owner.set(1);
  NUI_CHECK(failures==1 && binding.get()==2 && binding.revision()==2);
  NUI_CHECK(observed==std::vector<int>({1,2}));
  owner.set(5);
  NUI_CHECK(binding.get()==5 && binding.revision()==3);
  NUI_CHECK(observed==std::vector<int>({1,2,5}));
}

void suite() {
  failed_copy_preserves_previous_observer_write();
  nested_failure_suite();
  nested_success_drains_only_after_outer_copy();
  destroyed_owner_leaves_copy_storage_pinned();
  vector_copy_replacement_waits_until_all_keys_are_owned();
  final_predicate_rejects_after_equality_changes_permission();
  queued_predicate_declines_without_commit_or_replay();
  successful_copy_current_intent_cancels_previous_pending();
  stable_read_prepares_owned_results_without_copying_move_only_values();
  failed_stable_read_preserves_earlier_pending_writes_and_recovers();
}
} // namespace

int main(int argc,char** argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  if (mode == "prior_pending") return test::run("state_owned_prior_pending",&failed_copy_preserves_previous_observer_write);
  if (mode == "nested_failure") return test::run("state_owned_nested_failure",&nested_failure_suite);
  if (mode == "nested_success") return test::run("state_owned_nested_success",&nested_success_drains_only_after_outer_copy);
  if (mode == "owner_destroy") return test::run("state_owned_owner_destroy",&destroyed_owner_leaves_copy_storage_pinned);
  if (mode == "vector_replace") return test::run("state_owned_vector_replace",&vector_copy_replacement_waits_until_all_keys_are_owned);
  if (mode == "final_guard") return test::run("state_owned_final_guard",&final_predicate_rejects_after_equality_changes_permission);
  if (mode == "queued_guard") return test::run("state_owned_queued_guard",&queued_predicate_declines_without_commit_or_replay);
  if (mode == "cancel_pending") return test::run("state_owned_cancel_pending",&successful_copy_current_intent_cancels_previous_pending);
  if (mode == "stable_read") return test::run("state_stable_read",&stable_read_prepares_owned_results_without_copying_move_only_values);
  if (mode == "stable_failure") return test::run("state_stable_read_failure",&failed_stable_read_preserves_earlier_pending_writes_and_recovers);
  return test::run("state_owned_transactions",&suite);
}
