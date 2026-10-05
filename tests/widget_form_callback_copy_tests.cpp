#include "test_support.hpp"

#include <nativeui/enabled.hpp>
#include <nativeui/form.hpp>
#include <nativeui/read_only.hpp>

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace {
struct Observation {
    bool armed{};
    int armed_copies{};
    int calls{};
    std::function<void()> on_copy;
};

struct CopyAction {
    std::shared_ptr<Observation> observation;

    explicit CopyAction(std::shared_ptr<Observation> value) : observation(std::move(value)) {}
    CopyAction(const CopyAction& other) : observation(other.observation) {
        if (std::exchange(observation->armed,false)) {
            ++observation->armed_copies;
            const auto callback = observation->on_copy;
            if (callback) callback();
        }
    }
    CopyAction(CopyAction&&) = default;
    void operator()() const { ++observation->calls; }
};

ui::InputEvent trigger(bool submit) {
    if (!submit) return test::key(ui::Key::Escape);
    ui::InputEvent event;
    event.type = ui::InputType::Command;
    event.command = ui::Command::Submit;
    return event;
}

void copying_form_action_revalidates_ancestor_availability(bool submit) {
    ui::State<bool> enabled{true};
    ui::State<bool> read_only{true};
    auto observation = std::make_shared<Observation>();
    auto form = ui::Form{ui::Button{"Control",[] {}}};
    if (submit) std::move(form).on_submit(CopyAction{observation});
    else std::move(form).on_cancel(CopyAction{observation});
    ui::UI tree{ui::ReadOnly{read_only,ui::Enabled{enabled,std::move(form)}}};
    test::MockPlatform platform;
    tree.resize({320.0f,180.0f});
    tree.activate(platform);

    // Arm only after compilation and activation: this copy belongs to input.
    observation->on_copy = [&] { enabled.set(false); };
    observation->armed = true;
    tree.dispatch(trigger(submit),platform);
    NUI_CHECK(!enabled.get());
    NUI_CHECK(observation->armed_copies == 1);
    NUI_CHECK(observation->calls == 0);

    // The retired action must not replay at a retained checkpoint.
    enabled.set(true);
    tree.resize({320.0f,180.0f});
    ui::HeadlessRenderer renderer{{320.0f,180.0f},1.0f};
    NUI_CHECK(renderer.render(tree));
    NUI_CHECK(observation->calls == 0);
    tree.dispatch(trigger(submit),platform);
    NUI_CHECK(observation->calls == 1);
    NUI_CHECK(read_only.get());
}

void submit_suite() { copying_form_action_revalidates_ancestor_availability(true); }
void cancel_suite() { copying_form_action_revalidates_ancestor_availability(false); }
void suite() { submit_suite(); cancel_suite(); }
}

int main(int argc,char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "all";
    if (mode == "submit") return test::run("form_copy_submit",&submit_suite);
    if (mode == "cancel") return test::run("form_copy_cancel",&cancel_suite);
    return test::run("form_callback_copy",&suite);
}
