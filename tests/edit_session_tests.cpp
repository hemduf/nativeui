#include <nativeui/edit.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#define CHECK(condition) do { if (!(condition)) { std::cerr << "check failed: " << #condition << " at " << __LINE__ << "\n"; std::abort(); } } while (false)

int main() {
    ui::State<float> value{0.5f};
    std::string trace;
    ui::EditCallbacks<float> callbacks{
        [&](ui::EditSource) { trace += 'B'; },
        [&](const float& v, ui::EditSource) { CHECK(v == value.get()); trace += 'C'; },
        [&](ui::EditSource) { trace += 'E'; },
        [&](ui::EditSource) { trace += 'X'; }};
    ui::EditSession<float> edit{value.binding(), callbacks};
    CHECK(edit.begin(ui::EditSource::Pointer));
    CHECK(!edit.begin(ui::EditSource::Keyboard));
    CHECK(edit.update(0.7f));
    CHECK(!edit.update(0.7f));
    edit.end();
    edit.cancel();
    CHECK(trace == "BCE");
    value.set(0.2f);
    CHECK(trace == "BCE");
    trace.clear();
    CHECK(!edit.set(0.2f, ui::EditSource::Wheel));
    CHECK(edit.set(0.3f, ui::EditSource::Keyboard));
    CHECK(trace == "BCE");
    trace.clear();
    CHECK(edit.begin(ui::EditSource::Pointer));
    CHECK(edit.update(0.4f));
    edit.cancel();
    CHECK(trace == "BCX" && value.get() == 0.4f);

    for (int failure = 0; failure < 4; ++failure) {
        bool fail = true;
        trace.clear();
        ui::EditSession<float> faulty{value.binding(), {
            [&](ui::EditSource) { trace += 'B'; if (fail && failure == 0) throw std::runtime_error("begin"); },
            [&](const float&, ui::EditSource) { trace += 'C'; if (fail && failure == 1) throw std::runtime_error("change"); },
            [&](ui::EditSource) { trace += 'E'; if (fail && failure == 2) throw std::runtime_error("end"); },
            [&](ui::EditSource) { trace += 'X'; if (fail && failure == 3) throw std::runtime_error("cancel"); }}};
        try {
            faulty.begin(ui::EditSource::Pointer);
            faulty.update(value.get() + 0.01f);
            if (failure == 3) faulty.cancel(); else faulty.end();
            CHECK(false);
        } catch (const std::runtime_error&) {}
        CHECK(!faulty.active());
        fail = false;
        trace.clear();
        CHECK(faulty.set(value.get() + 0.01f, ui::EditSource::Keyboard));
        CHECK(trace == "BCE");
    }

    trace.clear();
    std::unique_ptr<ui::EditSession<float>> owned;
    callbacks.change = [&](const float&, ui::EditSource) { trace += 'C'; owned.reset(); };
    owned = std::make_unique<ui::EditSession<float>>(value.binding(), callbacks);
    owned->set(0.8f, ui::EditSource::Keyboard);
    CHECK(!owned && trace == "BCX");

    trace.clear();
    auto observer = value.observe([&](const float&) { edit.cancel(); });
    edit.set(0.9f, ui::EditSource::Accessibility);
    CHECK(trace == "BCX");
    observer.reset();
    trace.clear();
    CHECK(edit.set(0.1f, ui::EditSource::Keyboard));
    CHECK(trace == "BCE");

    // Reentrant edits are rejected; terminal requests are deferred and cancel wins.
    trace.clear();
    ui::EditSession<float>* reentrant_ptr{};
    ui::EditSession<float> reentrant{value.binding(), {
        [&](ui::EditSource) {
            trace += 'B';
            CHECK(!reentrant_ptr->begin(ui::EditSource::Wheel));
            CHECK(!reentrant_ptr->update(0.8f));
            CHECK(!reentrant_ptr->set(0.8f,ui::EditSource::Wheel));
            reentrant_ptr->end(); reentrant_ptr->cancel();
        }, {}, [&](ui::EditSource){trace+='E';}, [&](ui::EditSource){trace+='X';}}};
    reentrant_ptr=&reentrant;
    CHECK(!reentrant.begin(ui::EditSource::Pointer)); CHECK(trace=="BX");

    trace.clear();
    auto dying_state=std::make_unique<ui::State<float>>(0.0f);
    ui::EditSession<float> retained{dying_state->binding(), callbacks};
    CHECK(retained.begin(ui::EditSource::Pointer));
    dying_state.reset();
    CHECK(!retained.update(1)); CHECK(!retained.active()); CHECK(trace=="BX");
    CHECK(!retained.begin(ui::EditSource::Pointer));

    trace.clear();
    callbacks.begin=[&](ui::EditSource){trace+='B';owned.reset();};
    owned=std::make_unique<ui::EditSession<float>>(value.binding(),callbacks);
    CHECK(!owned->begin(ui::EditSource::Pointer)); CHECK(!owned); CHECK(trace=="BX");

    trace.clear();
    ui::EditSession<float> observer_failure{value.binding(), {
        [&](ui::EditSource){trace+='B';},{},{},[&](ui::EditSource){trace+='X';throw 7;}}};
    auto throwing_observer=value.observe([](const float&){throw std::runtime_error("observer");});
    try { observer_failure.set(0.7f,ui::EditSource::Keyboard); CHECK(false); }
    catch(const std::runtime_error& error){CHECK(std::string(error.what())=="observer");}
    CHECK(trace=="BX"); CHECK(!observer_failure.active());
    throwing_observer.reset();

    // A foreign State observer cannot start an edit whose write would only queue.
    trace.clear();
    auto recursive=value.observe([&](const float&){
        CHECK(!edit.set(0.99f,ui::EditSource::Keyboard));
        CHECK(!edit.begin(ui::EditSource::Pointer));
    });
    value.set(0.3f); CHECK(trace.empty()); CHECK(value.get()==0.3f);
    recursive.reset();

    // Independent owners do not share editing state, even for one Binding.
    ui::EditSession<float> other{value.binding()};
    CHECK(edit.begin(ui::EditSource::Pointer));
    CHECK(other.set(0.4f,ui::EditSource::Keyboard));
    CHECK(edit.active()); edit.end(); CHECK(trace=="BE");
    // Keep an explicit success status: hosted Windows qualification observed
    // non-zero process status after every CHECK passed with implicit return.
    return 0;
}
