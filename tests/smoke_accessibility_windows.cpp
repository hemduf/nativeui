#include "smoke_accessibility_windows.hpp"

#include <nativeui/nativeui.hpp>

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#include <windows.h>

#include <uiautomationclient.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>

namespace {

int fail(std::string_view stage, std::string_view message) {
    std::cerr << "[nativeui smoke accessibility windows] " << stage << ": "
              << message << '\n';
    return 1;
}

bool near(double actual, double expected) {
    return std::fabs(actual - expected) <= 0.0001;
}

} // namespace

int nativeui_smoke_accessibility_windows::run_uia_query_fixture() {
    const char* stage = "uia-construct";
    ui::Application application;
    if (!application.valid()) return fail(stage, application.last_error());

    ui::State<float> value{0.25f};
    ui::UI app_ui{
        ui::Column{ui::Slider{value}.range(0.0f, 1.0f)}.padding(12.0f)};
    auto window = std::make_unique<ui::StandaloneWindow>(
        application,
        app_ui,
        ui::WindowDesc{.title = "NativeUI accessibility UIA fixture",
                       .size = {320.0f, 180.0f},
                       .resizable = true});
    if (!window->valid()) return fail(stage, window->last_error());

    stage = "uia-initial-publication";
    for (int i = 0; i < 6 && !window->should_close(); ++i) {
        (void)application.poll(0.0);
        if (!std::string_view{window->last_error()}.empty()) break;
    }
    if (!std::string_view{window->last_error()}.empty()) {
        return fail(stage, window->last_error());
    }

    stage = "uia-client";
    const HWND hwnd = reinterpret_cast<HWND>(
        static_cast<std::uintptr_t>(window->native_handle()));
    if (!hwnd) return fail(stage, "native handle is zero");

    IUIAutomation* automation = nullptr;
    HRESULT hr = ::CoCreateInstance(CLSID_CUIAutomation8, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&automation));
    if (FAILED(hr)) {
        hr = ::CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&automation));
    }
    if (FAILED(hr) || !automation) {
        return fail(stage, "CoCreateInstance(CUIAutomation) failed");
    }

    IUIAutomationElement* window_element = nullptr;
    if (FAILED(automation->ElementFromHandle(hwnd, &window_element)) ||
        !window_element) {
        automation->Release();
        return fail(stage, "ElementFromHandle failed");
    }

    stage = "uia-fragment-root";
    VARIANT slider_type;
    ::VariantInit(&slider_type);
    slider_type.vt = VT_I4;
    slider_type.lVal = UIA_SliderControlTypeId;
    IUIAutomationCondition* condition = nullptr;
    hr = automation->CreatePropertyCondition(UIA_ControlTypePropertyId,
                                             slider_type, &condition);
    if (FAILED(hr) || !condition) {
        window_element->Release();
        automation->Release();
        return fail(stage, "CreatePropertyCondition failed");
    }

    IUIAutomationElement* root = nullptr;
    hr = window_element->FindFirst(TreeScope_Subtree, condition, &root);
    condition->Release();
    if (FAILED(hr) || !root) {
        window_element->Release();
        automation->Release();
        return fail(stage, "production bridge did not expose the slider fragment");
    }

    IUIAutomationRangeValuePattern* range = nullptr;
    hr = root->GetCurrentPatternAs(UIA_RangeValuePatternId,
                                   IID_PPV_ARGS(&range));
    if (FAILED(hr) || !range) {
        root->Release();
        window_element->Release();
        automation->Release();
        return fail(stage, "slider fragment does not expose RangeValue");
    }

    stage = "uia-initial-value";
    double initial_value = 0.0;
    BOOL read_only = FALSE;
    if (FAILED(range->get_CurrentValue(&initial_value)) ||
        !near(initial_value, 0.25)) {
        range->Release();
        root->Release();
        window_element->Release();
        automation->Release();
        return fail(stage, "fragment value does not match the initial slider value");
    }
    if (FAILED(range->get_CurrentIsReadOnly(&read_only)) || read_only != FALSE) {
        range->Release();
        root->Release();
        window_element->Release();
        automation->Release();
        return fail(stage, "writable slider fragment reports read-only");
    }

    // A committed UI-thread value update must reach UIA through the same
    // fragment identity: the retained client element reports the new value on
    // the next query.
    stage = "uia-posted-update";
    if (!window->dispatcher().post([&value] { value.set(0.75f); })) {
        range->Release();
        root->Release();
        window_element->Release();
        automation->Release();
        return fail(stage, "dispatcher rejected the slider value update");
    }
    (void)application.poll(0.0);
    if (!std::string_view{window->last_error()}.empty()) {
        const std::string error{window->last_error()};
        range->Release();
        root->Release();
        window_element->Release();
        automation->Release();
        return fail(stage, error);
    }

    double updated_value = 0.0;
    if (FAILED(range->get_CurrentValue(&updated_value)) ||
        !near(updated_value, 0.75)) {
        range->Release();
        root->Release();
        window_element->Release();
        automation->Release();
        return fail(stage, "fragment value did not observe the committed update");
    }

    range->Release();
    root->Release();
    window_element->Release();
    automation->Release();

    window.reset();
    (void)application.poll(0.0);
    return 0;
}
