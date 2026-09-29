#include "example_support.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Contact {
    ui::PointerId id{};
    ui::Point position{};
};

struct Model {
    std::vector<Contact> contacts;
    int cancellations{};
};

Contact* find_contact(Model& model, ui::PointerId id) {
    const auto it = std::find_if(model.contacts.begin(),
                                 model.contacts.end(),
                                 [id](const Contact& contact) { return contact.id == id; });
    return it == model.contacts.end() ? nullptr : &*it;
}

ui::InputEvent touch(ui::InputType type, ui::PointerId id, float x, float y) {
    ui::InputEvent event{};
    event.type = type;
    event.position = {x, y};
    event.pointer.id = id;
    event.pointer.type = ui::PointerType::Touch;
    event.pointer.primary = id == 1U;
    event.pointer.pressure = 0.5f;
    event.pointer.contact_size = {12.0f, 12.0f};
    return event;
}

std::unique_ptr<ui::UI> make_ui(Model& model) {
    return std::make_unique<ui::UI>(
        ui::Column{
            ui::Header{"RAW MULTI-TOUCH POINTERS"},
            ui::Canvas{520.0f, 240.0f, [&](ui::CanvasContext2D& g) {
                g.fill_rounded_rect({0.0f, 0.0f, g.width(), g.height()},
                                    12.0f,
                                    ui::colors::panel);
                for (const auto& contact : model.contacts) {
                    g.circle(contact.position, 22.0f, ui::colors::accent);
                    g.text({contact.position.x + 28.0f, contact.position.y},
                           "id=" + std::to_string(contact.id),
                           11.0f,
                           ui::colors::text);
                }
                g.text({14.0f, 20.0f},
                       "Each touch keeps an independent NativeUI capture.",
                       11.0f,
                       ui::colors::textMuted);
            }}.on_input([&](const ui::InputEvent& event, ui::CanvasInputContext& context) {
                if (event.type == ui::InputType::PointerDown &&
                    event.pointer.type == ui::PointerType::Touch) {
                    if (!find_contact(model, event.pointer.id)) {
                        model.contacts.push_back({event.pointer.id, event.position});
                    }
                    context.capture_pointer();
                    context.invalidate();
                    return ui::EventResult::Handled;
                }

                if (event.type == ui::InputType::PointerMove) {
                    if (auto* contact = find_contact(model, event.pointer.id)) {
                        contact->position = event.position;
                        context.invalidate();
                        return ui::EventResult::Handled;
                    }
                }

                if (event.type == ui::InputType::PointerUp ||
                    event.type == ui::InputType::PointerCancel) {
                    const auto before = model.contacts.size();
                    model.contacts.erase(
                        std::remove_if(model.contacts.begin(),
                                       model.contacts.end(),
                                       [&](const Contact& contact) {
                                           return contact.id == event.pointer.id;
                                       }),
                        model.contacts.end());
                    if (event.type == ui::InputType::PointerCancel &&
                        model.contacts.size() != before) {
                        ++model.cancellations;
                    }
                    context.invalidate();
                    return before != model.contacts.size()
                        ? ui::EventResult::Handled
                        : ui::EventResult::Ignored;
                }
                return ui::EventResult::Ignored;
            })
        }.padding(20.0f).gap(14.0f));
}

int run_self_test() {
    Model model;
    auto tree = make_ui(model);
    example::Platform platform;
    tree->resize({560.0f, 320.0f});
    tree->activate(platform);

    (void)tree->dispatch(touch(ui::InputType::PointerDown, 1U, 100.0f, 140.0f), platform);
    (void)tree->dispatch(touch(ui::InputType::PointerDown, 2U, 360.0f, 140.0f), platform);
    if (model.contacts.size() != 2U) return example::fail("two contacts were not retained");

    const auto* first_before = find_contact(model, 1U);
    const auto* second_before = find_contact(model, 2U);
    if (!first_before || !second_before) return example::fail("missing contact before move");
    const ui::Point first_start = first_before->position;
    const ui::Point second_start = second_before->position;

    (void)tree->dispatch(touch(ui::InputType::PointerMove, 1U, 500.0f, 280.0f), platform);
    const auto* first = find_contact(model, 1U);
    const auto* second = find_contact(model, 2U);
    if (!first || example::near(first->position.x, first_start.x)) {
        return example::fail("first captured contact did not move independently");
    }
    if (!second || !example::near(second->position.x, second_start.x) ||
        !example::near(second->position.y, second_start.y)) {
        return example::fail("moving first contact disturbed second contact");
    }

    (void)tree->dispatch(touch(ui::InputType::PointerUp, 1U, 500.0f, 280.0f), platform);
    if (model.contacts.size() != 1U || !find_contact(model, 2U)) {
        return example::fail("releasing one contact disturbed its sibling");
    }

    (void)tree->dispatch(touch(ui::InputType::PointerCancel, 2U, 360.0f, 140.0f), platform);
    if (!model.contacts.empty() || model.cancellations != 1) {
        return example::fail("contact cancellation did not terminate exactly one contact");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (example::self_test_requested(argc, argv)) return run_self_test();
    Model model;
    auto tree = make_ui(model);
    return example::run_window(*tree, "NativeUI - Multi-touch pointers", {580.0f, 360.0f});
}
