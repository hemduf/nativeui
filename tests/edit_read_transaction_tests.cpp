#include <nativeui/edit.hpp>

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
void check(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
ui::EditCallbacks<int> callbacks(std::string &trace) {
  return {[&](ui::EditSource) { trace += 'B'; },
          [&](const int &, ui::EditSource) { trace += 'C'; },
          [&](ui::EditSource) { trace += 'E'; },
          [&](ui::EditSource) { trace += 'X'; }};
}
void set_rejected_during_read_recovers() {
  ui::State<int> value{0};
  std::string trace;
  ui::EditSession<int> edit{value.binding(), callbacks(trace)};
  ui::detail::StateReadAccess::read(value.binding(), [&](const int &snapshot) {
    check(!edit.set(1, ui::EditSource::Keyboard),
          "Read must reject atomic edit");
    check(snapshot == 0 && trace.empty(), "Rejected edit must stay silent");
    return snapshot;
  });
  check(value.get() == 0 && trace.empty() && !edit.active(),
        "Rejected atomic edit must not queue a later write");
  check(edit.set(2, ui::EditSource::Keyboard), "Next ordinary edit must work");
  check(value.get() == 2 && trace == "BCE" && !edit.active(),
        "Ordinary edit must report the committed change before ending");
}
void begin_rejected_during_read_recovers() {
  ui::State<int> value{0};
  std::string trace;
  ui::EditSession<int> edit{value.binding(), callbacks(trace)};
  ui::detail::StateReadAccess::read(value.binding(), [&](const int &snapshot) {
    check(!edit.begin(ui::EditSource::Pointer),
          "Read must reject beginning edit");
    check(trace.empty() && !edit.active(), "Rejected begin must stay silent");
    return snapshot;
  });
  check(value.get() == 0 && trace.empty(),
        "Rejected begin must preserve state");
  check(edit.begin(ui::EditSource::Pointer), "Next pointer edit must begin");
  check(edit.finish(3), "Next pointer edit must publish");
  check(value.get() == 3 && trace == "BCE" && !edit.active(),
        "Recovered pointer edit must finish once");
}
void update_rejected_during_read_recovers() {
  ui::State<int> value{0};
  std::string trace;
  ui::EditSession<int> edit{value.binding(), callbacks(trace)};
  check(edit.begin(ui::EditSource::Pointer), "Pointer edit must begin");
  ui::detail::StateReadAccess::read(value.binding(), [&](const int &snapshot) {
    check(!edit.update(1), "Read must reject changing an active edit");
    check(snapshot == 0 && trace == "B",
          "Rejected update must report no change");
    return snapshot;
  });
  check(value.get() == 0 && trace == "B" && edit.active(),
        "Rejected update must neither queue a write nor terminate the edit");
  check(edit.finish(4), "Active edit must recover after read completes");
  check(value.get() == 4 && trace == "BCE" && !edit.active(),
        "Recovered update must publish and finish once");
}
} // namespace
int main(int argc, char **argv) {
  const std::string_view mode = argc > 1 ? argv[1] : "all";
  try {
    if (mode == "all" || mode == "set")
      set_rejected_during_read_recovers();
    if (mode == "all" || mode == "begin")
      begin_rejected_during_read_recovers();
    if (mode == "all" || mode == "update")
      update_rejected_during_read_recovers();
    if (mode != "all" && mode != "set" && mode != "begin" && mode != "update")
      return 2;
  } catch (const std::exception &error) {
    std::cerr << "edit_read_transaction: " << error.what() << '\n';
    return 1;
  }
  return 0;
}
