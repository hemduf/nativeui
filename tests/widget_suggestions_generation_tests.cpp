#include "../src/detail/widget_suggestions.hpp"
#include "test_support.hpp"
namespace {
void completed_selection_cannot_publish_after_a_new_generation() {
  auto engine = std::make_shared<ui::detail::SuggestionsEngine>();
  engine->mounted = true;
  engine->provider = [] { return std::vector<std::string>{"Paris", "Pau"}; };
  engine->allowed = [] { return true; };
  int published{};
  std::string accepted;
  engine->prepare_choice = [&](std::string value) {
    return [&, value = std::move(value)] {
      ++published;
      accepted = value;
    };
  };
  engine->rebuild("pa");
  (void)engine->take_command();
  engine->choose(0);
  auto old = engine->take_command();
  NUI_CHECK(old && old->after_close);
  engine->rebuild("pau");
  const auto fresh_session = engine->session;
  const auto fresh_generation = engine->generation;
  old->after_close();
  NUI_CHECK(published == 0 && accepted.empty());
  NUI_CHECK(engine->generation == fresh_generation &&
            engine->session == fresh_session && fresh_session->live);
  engine->choose(0);
  auto current = engine->take_command();
  NUI_CHECK(current && current->after_close);
  current->after_close();
  NUI_CHECK(published == 1 && accepted == "Pau");
}
void detached_engine_never_invokes_a_queued_selection() {
  auto engine = std::make_shared<ui::detail::SuggestionsEngine>();
  engine->mounted = true;
  engine->provider = [] { return std::vector<std::string>{"Paris"}; };
  int calls{};
  engine->prepare_choice = [&](std::string) { return [&] { ++calls; }; };
  engine->rebuild("pa");
  (void)engine->take_command();
  engine->choose(0);
  auto old = engine->take_command();
  NUI_CHECK(old && old->after_close);
  engine->detach();
  old->after_close();
  NUI_CHECK(calls == 0);
}
void suite() {
  completed_selection_cannot_publish_after_a_new_generation();
  detached_engine_never_invokes_a_queued_selection();
}
} // namespace
int main() { return test::run("widget_suggestions_generation", &suite); }
