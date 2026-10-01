// Windows-only accessibility UIA provider suite.
//
// This remote-CI test compiles the real Win32 adapter source into the test
// executable, creates a real window, attaches the production bridge and queries
// the fragment through a real IUIAutomation client. It travels with the
// consumer/prefixed Windows build and cannot run on macOS/Linux, so it is
// registered only under if(WIN32) in tests/CMakeLists.txt.

#if !defined(_WIN32)
#error "nativeui_accessibility_uia_win32_provider_tests requires the Win32 platform"
#endif

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#include <windows.h>

#include <uiautomationclient.h>

#include "../../src/detail/native_accessibility_binding.hpp"
#include "../../src/detail/native_accessibility_bridge.h"

#include <nativeui/detail/dispatcher_owner.hpp>
#include <nativeui/detail/semantic_native_view_bridge.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("line " + std::to_string(__LINE__) +       \
                                     ": check failed: " #condition);            \
        }                                                                       \
    } while (false)

using ui::SemanticAction;
using ui::SemanticChange;
using ui::SemanticRole;
using ui::VirtualSemanticItemMetadata;
using ui::VirtualSemanticItemToken;
using ui::detail::SemanticActionRequest;
using ui::detail::SemanticActionTarget;
using ui::detail::SemanticIdentity;
using ui::detail::SemanticNativeGeometry;
using ui::detail::SemanticNativePublicationBatch;
using ui::detail::SemanticNativeViewBridge;

constexpr wchar_t kWindowClassName[] = L"NativeUI_Accessibility_UIA_Win32_Test";
constexpr std::size_t kVirtualItemCount = 100000U;
constexpr VirtualSemanticItemToken kFirstToken = 9000U;

LRESULT CALLBACK test_window_proc(HWND hwnd,
                                  UINT message,
                                  WPARAM wparam,
                                  LPARAM lparam) {
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

/// Real top-level window owned by the test thread.
class TestWindow final {
public:
    TestWindow() {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.lpfnWndProc = &test_window_proc;
        window_class.hInstance = ::GetModuleHandleW(nullptr);
        window_class.lpszClassName = kWindowClassName;
        class_atom_ = ::RegisterClassExW(&window_class);
        if (class_atom_ == 0U) {
            throw std::runtime_error("RegisterClassExW failed");
        }
        hwnd_ = ::CreateWindowExW(0, kWindowClassName, L"NativeUI accessibility UIA",
                                  WS_OVERLAPPEDWINDOW, 0, 0, 480, 320, nullptr,
                                  nullptr, window_class.hInstance, nullptr);
        if (!hwnd_) {
            ::UnregisterClassW(kWindowClassName,
                               ::GetModuleHandleW(nullptr));
            class_atom_ = 0U;
            throw std::runtime_error("CreateWindowExW failed");
        }
        (void)::ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
        (void)::UpdateWindow(hwnd_);
    }

    ~TestWindow() {
        if (hwnd_) {
            ::DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
        if (class_atom_ != 0U) {
            ::UnregisterClassW(kWindowClassName, ::GetModuleHandleW(nullptr));
        }
    }

    TestWindow(const TestWindow&) = delete;
    TestWindow& operator=(const TestWindow&) = delete;

    [[nodiscard]] HWND handle() const noexcept { return hwnd_; }

private:
    ATOM class_atom_{};
    HWND hwnd_{};
};

class RecordingTarget final : public SemanticActionTarget {
public:
    explicit RecordingTarget(
        std::shared_ptr<std::vector<SemanticAction>> actions)
        : actions_(std::move(actions)) {}

    [[nodiscard]] std::optional<ui::SemanticInfo> current_semantics(
        const SemanticIdentity& identity) const override {
        ui::SemanticInfo info;
        if (identity.virtual_token) {
            if (identity.node_id != 11U) {
                return std::nullopt;
            }
            info.role = SemanticRole::ListItem;
            info.actions = {SemanticAction::Select, SemanticAction::Focus};
            return info;
        }
        switch (identity.node_id) {
            case 1U:
                info.role = SemanticRole::Group;
                return info;
            case 2U:
                info.role = SemanticRole::Button;
                info.name = "action";
                info.actions = {SemanticAction::Activate,
                                SemanticAction::Focus};
                return info;
            case 12U:
                info.role = SemanticRole::Slider;
                info.numeric_value = 0.25;
                info.value_range = ui::SemanticValueRange{0.0, 1.0, 0.1};
                info.actions = {
                    SemanticAction::Increment,
                    SemanticAction::Decrement,
                    SemanticAction::SetValue,
                    SemanticAction::Focus,
                };
                return info;
            default:
                return std::nullopt;
        }
    }

    bool dispatch_semantic_action(const SemanticIdentity& identity,
                                  const SemanticActionRequest& request) override {
        actions_->push_back(request.action);
        if (request.action == SemanticAction::SetValue) {
            value_ = request.numeric_value;
        }
        return identity.node_id != 0U;
    }

    [[nodiscard]] std::optional<double> numeric_value() const noexcept {
        return value_;
    }

private:
    std::shared_ptr<std::vector<SemanticAction>> actions_;
    std::optional<double> value_;
};

/// Minimal in-process IUIAutomation client with deterministic cleanup.
class UiaClient final {
public:
    UiaClient() {
        HRESULT hr = ::CoCreateInstance(CLSID_CUIAutomation8, nullptr,
                                        CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&automation_));
        if (FAILED(hr)) {
            hr = ::CoCreateInstance(CLSID_CUIAutomation, nullptr,
                                    CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&automation_));
        }
        if (FAILED(hr) || !automation_) {
            throw std::runtime_error("CoCreateInstance(CUIAutomation) failed");
        }
    }

    ~UiaClient() {
        if (automation_) {
            automation_->Release();
            automation_ = nullptr;
        }
    }

    UiaClient(const UiaClient&) = delete;
    UiaClient& operator=(const UiaClient&) = delete;

    [[nodiscard]] IUIAutomation* get() const noexcept { return automation_; }

private:
    IUIAutomation* automation_{};
};

[[nodiscard]] IUIAutomationElement* element_from_handle(
    IUIAutomation* automation,
    HWND hwnd) {
    IUIAutomationElement* element = nullptr;
    const HRESULT hr = automation->ElementFromHandle(hwnd, &element);
    CHECK(SUCCEEDED(hr));
    CHECK(element != nullptr);
    return element;
}

[[nodiscard]] std::wstring element_name(IUIAutomationElement* element) {
    BSTR name = nullptr;
    if (FAILED(element->get_CurrentName(&name)) || !name) {
        return {};
    }
    std::wstring result{name};
    ::SysFreeString(name);
    return result;
}

[[nodiscard]] CONTROLTYPEID element_control_type(
    IUIAutomationElement* element) {
    CONTROLTYPEID type = 0;
    CHECK(SUCCEEDED(element->get_CurrentControlType(&type)));
    return type;
}

[[nodiscard]] std::wstring element_automation_id(IUIAutomationElement* element) {
    BSTR id = nullptr;
    if (FAILED(element->get_CurrentAutomationId(&id)) || !id) {
        return {};
    }
    std::wstring result{id};
    ::SysFreeString(id);
    return result;
}

[[nodiscard]] IUIAutomationElement* find_by_automation_id(
    IUIAutomation* automation,
    IUIAutomationElement* scope,
    TreeScope tree_scope,
    const std::wstring& automation_id) {
    VARIANT value;
    ::VariantInit(&value);
    value.vt = VT_BSTR;
    value.bstrVal = ::SysAllocString(automation_id.c_str());
    CHECK(value.bstrVal != nullptr);

    IUIAutomationCondition* condition = nullptr;
    HRESULT hr = automation->CreatePropertyCondition(
        UIA_AutomationIdPropertyId, value, &condition);
    ::VariantClear(&value);
    CHECK(SUCCEEDED(hr));
    CHECK(condition != nullptr);

    IUIAutomationElement* found = nullptr;
    hr = scope->FindFirst(tree_scope, condition, &found);
    condition->Release();
    CHECK(SUCCEEDED(hr));
    return found;
}

/// Resolve the fragment root whether UIA returns it for the HWND directly or
/// beneath the host window element. The frozen contract exposes exactly one
/// root with the requested AutomationId.
[[nodiscard]] IUIAutomationElement* fragment_root(
    IUIAutomation* automation,
    HWND hwnd,
    const wchar_t* root_automation_id = L"1") {
    IUIAutomationElement* window = element_from_handle(automation, hwnd);
    IUIAutomationElement* root = find_by_automation_id(
        automation, window, TreeScope_Subtree, root_automation_id);
    window->Release();
    CHECK(root != nullptr);
    return root;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> ordinary_snapshot(
    std::uint64_t generation,
    bool include_child,
    bool child_focused = false,
    bool child_selected = false) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 1U;

    ui::SemanticNodeSnapshot root;
    root.id = 1U;
    root.info.role = SemanticRole::Group;
    root.info.name = "root";
    if (include_child) root.children.push_back(2U);
    snapshot->nodes.push_back(std::move(root));

    if (include_child) {
        ui::SemanticNodeSnapshot child;
        child.id = 2U;
        child.parent = 1U;
        child.info.role = SemanticRole::Button;
        child.info.name = "action";
        child.info.focused = child_focused;
        child.info.selected = child_selected;
        child.info.actions = {SemanticAction::Activate, SemanticAction::Focus};
        snapshot->nodes.push_back(std::move(child));
    }
    return snapshot;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> slider_snapshot(
    std::uint64_t generation,
    float value) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 12U;

    ui::SemanticNodeSnapshot slider;
    slider.id = 12U;
    slider.info.role = SemanticRole::Slider;
    slider.info.name = "slider";
    slider.info.numeric_value = static_cast<double>(value);
    slider.info.value_range = ui::SemanticValueRange{0.0, 1.0, 0.1};
    slider.info.focusable = true;
    slider.info.actions = {
        SemanticAction::Increment,
        SemanticAction::Decrement,
        SemanticAction::SetValue,
        SemanticAction::Focus,
    };
    slider.bounds = {0.0f, 0.0f, 200.0f, 40.0f};
    snapshot->nodes.push_back(std::move(slider));
    return snapshot;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> virtual_snapshot(
    std::uint64_t generation,
    std::size_t count,
    VirtualSemanticItemToken first_token) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 11U;

    auto metadata = std::make_shared<ui::VirtualSemanticChildren::Metadata>();
    auto token_index = std::make_shared<ui::VirtualSemanticChildren::TokenIndex>();
    metadata->reserve(count);
    token_index->reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const auto token = static_cast<VirtualSemanticItemToken>(
            first_token + static_cast<VirtualSemanticItemToken>(index));
        VirtualSemanticItemMetadata item;
        item.token = token;
        item.name = "row" + std::to_string(index);
        item.actions = {SemanticAction::Select, SemanticAction::Focus};
        metadata->push_back(std::move(item));
        token_index->emplace(token, index);
    }

    ui::SemanticNodeSnapshot list;
    list.id = 11U;
    list.info.role = SemanticRole::ListView;
    list.bounds = {0.0f, 0.0f, 200.0f, 300.0f};
    list.virtual_children = ui::VirtualSemanticChildren::from_indexed_metadata(
        generation,
        std::shared_ptr<const ui::VirtualSemanticChildren::Metadata>{metadata},
        std::shared_ptr<const ui::VirtualSemanticChildren::TokenIndex>{token_index},
        std::nullopt,
        list.bounds,
        20.0f,
        0.0f);
    snapshot->nodes.push_back(std::move(list));
    return snapshot;
}

void record_event(void* user_data, const char* name, const void* element) {
    (void)element;
    if (!user_data || !name) {
        return;
    }
    static_cast<std::vector<std::string>*>(user_data)->emplace_back(name);
}

[[nodiscard]] bool has_event(const std::vector<std::string>& events,
                             const char* name) {
    for (const auto& event : events) {
        if (event == name) {
            return true;
        }
    }
    return false;
}

struct Fixture final {
    ui::detail::DispatcherOwner owner;
    SemanticNativeViewBridge bridge;
    std::shared_ptr<std::vector<SemanticAction>> dispatched{
        std::make_shared<std::vector<SemanticAction>>()};
    TestWindow window;
    NativeUIAccessibilityBridge* accessibility{nullptr};

    Fixture() {
        bridge.bind_actions(
            owner.dispatcher(),
            std::make_shared<RecordingTarget>(dispatched));
    }

    ~Fixture() {
        if (accessibility) {
            nativeuiAccessibilityDestroy(accessibility);
            accessibility = nullptr;
        }
    }

    Fixture(const Fixture&) = delete;
    Fixture& operator=(const Fixture&) = delete;

    [[nodiscard]] std::optional<SemanticNativePublicationBatch> publish(
        const std::shared_ptr<const ui::SemanticTreeSnapshot>& snapshot,
        SemanticNativeGeometry geometry = {}) {
        bridge.stage(*snapshot);
        return bridge.checkpoint_native_publication(geometry);
    }

    void attach() {
        const ui::detail::NativeAccessibilityAttachBinding binding{
            bridge.native_reader_source(), bridge.native_action_endpoint()};
        accessibility = nativeuiAccessibilityCreate(window.handle(), &binding);
        CHECK(accessibility != nullptr);
    }

    [[nodiscard]] bool deliver(const SemanticNativePublicationBatch& batch) {
        CHECK(accessibility != nullptr);
        return nativeuiAccessibilityDeliver(accessibility, &batch);
    }
};

void invalid_attach_disables_accessibility_without_breaking_the_window() {
    Fixture fixture;

    const ui::detail::NativeAccessibilityAttachBinding binding{
        fixture.bridge.native_reader_source(),
        fixture.bridge.native_action_endpoint()};
    CHECK(nativeuiAccessibilityCreate(nullptr, &binding) == nullptr);
    CHECK(nativeuiAccessibilityCreate(
              reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234U)),
              &binding) == nullptr);
    CHECK(nativeuiAccessibilityCreate(fixture.window.handle(), nullptr) ==
          nullptr);
    nativeuiAccessibilityDestroy(nullptr);

    // The real window must stay fully functional after every attach failure.
    (void)::SendMessageW(fixture.window.handle(), WM_NULL, 0, 0);
    CHECK(::IsWindow(fixture.window.handle()) != FALSE);
}

void client_queries_the_in_view_fragment_root() {
    Fixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true)).has_value());
    fixture.attach();

    UiaClient client;
    IUIAutomation* automation = client.get();
    IUIAutomationElement* root =
        fragment_root(automation, fixture.window.handle());
    CHECK(element_control_type(root) == UIA_GroupControlTypeId);
    CHECK(element_name(root) == L"root");

    IUIAutomationElement* child =
        find_by_automation_id(automation, root, TreeScope_Children, L"2");
    CHECK(child != nullptr);
    CHECK(element_control_type(child) == UIA_ButtonControlTypeId);
    CHECK(element_name(child) == L"action");

    IUIAutomationInvokePattern* invoke = nullptr;
    HRESULT hr = child->GetCurrentPatternAs(
        UIA_InvokePatternId, IID_PPV_ARGS(&invoke));
    CHECK(SUCCEEDED(hr));
    CHECK(invoke != nullptr);
    CHECK(SUCCEEDED(invoke->Invoke()));
    CHECK(fixture.dispatched->empty());
    CHECK(fixture.owner.checkpoint() == 1U);
    CHECK(fixture.dispatched->size() == 1U);
    CHECK(fixture.dispatched->front() == SemanticAction::Activate);

    invoke->Release();
    child->Release();
    root->Release();
}

void value_update_is_not_a_structure_rebuild() {
    Fixture fixture;
    CHECK(fixture.publish(slider_snapshot(1U, 0.25f)).has_value());

    std::vector<std::string> events;
    fixture.attach();
    nativeuiAccessibilitySetNotificationRecorderForTest(
        fixture.accessibility, &events, &record_event);

    UiaClient client;
    IUIAutomation* automation = client.get();
    IUIAutomationElement* slider =
        fragment_root(automation, fixture.window.handle(), L"12");
    CHECK(element_control_type(slider) == UIA_SliderControlTypeId);

    IUIAutomationRangeValuePattern* range = nullptr;
    CHECK(SUCCEEDED(slider->GetCurrentPatternAs(
        UIA_RangeValuePatternId, IID_PPV_ARGS(&range))));
    CHECK(range != nullptr);
    double value = 0.0;
    CHECK(SUCCEEDED(range->get_CurrentValue(&value)));
    CHECK(value > 0.249 && value < 0.251);

    events.clear();
    const auto update = fixture.publish(slider_snapshot(2U, 0.75f));
    CHECK(update.has_value());
    CHECK(fixture.deliver(*update));
    CHECK(has_event(events, "value"));
    CHECK(!has_event(events, "structure"));

    double updated = 0.0;
    CHECK(SUCCEEDED(range->get_CurrentValue(&updated)));
    CHECK(updated > 0.749 && updated < 0.751);

    range->Release();
    slider->Release();
}

void virtual_list_items_are_lazy_and_addressable() {
    Fixture fixture;
    CHECK(fixture.publish(virtual_snapshot(1U, kVirtualItemCount, kFirstToken))
              .has_value());
    fixture.attach();

    UiaClient client;
    IUIAutomation* automation = client.get();
    IUIAutomationElement* list =
        fragment_root(automation, fixture.window.handle(), L"11");
    CHECK(element_control_type(list) == UIA_ListControlTypeId);

    IUIAutomationItemContainerPattern* container = nullptr;
    CHECK(SUCCEEDED(list->GetCurrentPatternAs(
        UIA_ItemContainerPatternId, IID_PPV_ARGS(&container))));
    CHECK(container != nullptr);

    const auto token = static_cast<VirtualSemanticItemToken>(
        kFirstToken + static_cast<VirtualSemanticItemToken>(kVirtualItemCount - 1U));
    VARIANT wanted;
    ::VariantInit(&wanted);
    wanted.vt = VT_BSTR;
    const std::wstring token_text = std::to_wstring(token);
    wanted.bstrVal = ::SysAllocString(token_text.c_str());
    CHECK(wanted.bstrVal != nullptr);

    IUIAutomationElement* item = nullptr;
    HRESULT hr = container->FindItemByProperty(
        nullptr, UIA_AutomationIdPropertyId, wanted, &item);
    ::VariantClear(&wanted);
    CHECK(SUCCEEDED(hr));
    CHECK(item != nullptr);
    CHECK(element_control_type(item) == UIA_ListItemControlTypeId);
    CHECK(element_automation_id(item) == token_text);

    IUIAutomationVirtualizedItemPattern* virtualized = nullptr;
    CHECK(SUCCEEDED(item->GetCurrentPatternAs(
        UIA_VirtualizedItemPatternId, IID_PPV_ARGS(&virtualized))));
    CHECK(virtualized != nullptr);
    CHECK(SUCCEEDED(virtualized->Realize()));

    IUIAutomationSelectionItemPattern* selection = nullptr;
    CHECK(SUCCEEDED(item->GetCurrentPatternAs(
        UIA_SelectionItemPatternId, IID_PPV_ARGS(&selection))));
    CHECK(selection != nullptr);
    CHECK(SUCCEEDED(selection->Select()));
    CHECK(fixture.owner.checkpoint() == 1U);
    CHECK(fixture.dispatched->size() == 1U);
    CHECK(fixture.dispatched->front() == SemanticAction::Select);

    selection->Release();
    virtualized->Release();
    item->Release();
    container->Release();
    list->Release();
}

void structure_change_retires_removed_elements() {
    Fixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true)).has_value());

    std::vector<std::string> events;
    fixture.attach();
    nativeuiAccessibilitySetNotificationRecorderForTest(
        fixture.accessibility, &events, &record_event);

    UiaClient client;
    IUIAutomation* automation = client.get();
    IUIAutomationElement* root =
        fragment_root(automation, fixture.window.handle());
    IUIAutomationElement* child =
        find_by_automation_id(automation, root, TreeScope_Children, L"2");
    CHECK(child != nullptr);

    events.clear();
    const auto removal = fixture.publish(ordinary_snapshot(2U, false));
    CHECK(removal.has_value());
    CHECK(fixture.deliver(*removal));
    CHECK(has_event(events, "structure"));

    IUIAutomationElement* gone =
        find_by_automation_id(automation, root, TreeScope_Children, L"2");
    CHECK(gone == nullptr);

    // A retained element for the removed identity must fail closed instead of
    // invoking a stale target.
    IUIAutomationInvokePattern* stale_invoke = nullptr;
    const HRESULT stale = child->GetCurrentPatternAs(
        UIA_InvokePatternId, IID_PPV_ARGS(&stale_invoke));
    CHECK(FAILED(stale) || stale_invoke == nullptr);
    if (stale_invoke) {
        stale_invoke->Release();
    }

    child->Release();
    root->Release();
}

void committed_categories_map_to_named_events() {
    Fixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true)).has_value());

    std::vector<std::string> events;
    fixture.attach();
    nativeuiAccessibilitySetNotificationRecorderForTest(
        fixture.accessibility, &events, &record_event);

    events.clear();
    const auto focus = fixture.publish(
        ordinary_snapshot(2U, true, /*child_focused=*/true,
                          /*child_selected=*/false),
        SemanticNativeGeometry{});
    CHECK(focus.has_value());
    CHECK(fixture.deliver(*focus));
    CHECK(has_event(events, "focus"));
    CHECK(!has_event(events, "structure"));

    events.clear();
    const auto selection = fixture.publish(
        ordinary_snapshot(3U, true, /*child_focused=*/true,
                          /*child_selected=*/true),
        SemanticNativeGeometry{});
    CHECK(selection.has_value());
    CHECK(fixture.deliver(*selection));
    CHECK(has_event(events, "selection"));

    events.clear();
    const auto bounds = fixture.publish(
        ordinary_snapshot(4U, true, /*child_focused=*/true,
                          /*child_selected=*/true),
        SemanticNativeGeometry{1.5f, {120.0f, -40.0f}});
    CHECK(bounds.has_value());
    CHECK(fixture.deliver(*bounds));
    CHECK(has_event(events, "bounds"));
    CHECK(!has_event(events, "structure"));

    // A superseded batch must not notify for the current tree.
    events.clear();
    CHECK(!fixture.deliver(*focus));
    CHECK(events.empty());
}

void detach_retires_the_fragment_without_breaking_the_window() {
    Fixture fixture;
    CHECK(fixture.publish(ordinary_snapshot(1U, true)).has_value());
    fixture.attach();

    UiaClient client;
    IUIAutomation* automation = client.get();
    IUIAutomationElement* root =
        fragment_root(automation, fixture.window.handle());
    CHECK(root != nullptr);
    root->Release();

    nativeuiAccessibilityDestroy(fixture.accessibility);
    fixture.accessibility = nullptr;

    // The window remains a valid UIA element, but the NativeUI fragment and its
    // semantic children are gone.
    IUIAutomationElement* window =
        element_from_handle(automation, fixture.window.handle());
    CHECK(window != nullptr);
    CHECK(find_by_automation_id(automation, window, TreeScope_Subtree, L"2") ==
          nullptr);
    window->Release();

    (void)::SendMessageW(fixture.window.handle(), WM_NULL, 0, 0);
    CHECK(::IsWindow(fixture.window.handle()) != FALSE);
}

void two_views_keep_isolated_fragments() {
    Fixture first;
    Fixture second;
    CHECK(first.publish(ordinary_snapshot(1U, true)).has_value());
    CHECK(second.publish(ordinary_snapshot(1U, false)).has_value());
    first.attach();
    second.attach();

    UiaClient client;
    IUIAutomation* automation = client.get();
    IUIAutomationElement* first_root =
        fragment_root(automation, first.window.handle());
    IUIAutomationElement* second_root =
        fragment_root(automation, second.window.handle());
    CHECK(first_root != nullptr);
    CHECK(second_root != nullptr);

    CHECK(find_by_automation_id(automation, first_root, TreeScope_Children,
                                L"2") != nullptr);
    CHECK(find_by_automation_id(automation, second_root, TreeScope_Children,
                                L"2") == nullptr);

    nativeuiAccessibilityDestroy(first.accessibility);
    first.accessibility = nullptr;

    // The surviving view keeps working after the independent view retires.
    CHECK(find_by_automation_id(automation, second_root, TreeScope_Children,
                                L"2") == nullptr);
    CHECK(element_control_type(second_root) == UIA_GroupControlTypeId);

    first_root->Release();
    second_root->Release();
}

} // namespace

int main() {
    const char* stage = "uia-win32";
    try {
        (void)::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        invalid_attach_disables_accessibility_without_breaking_the_window();
        client_queries_the_in_view_fragment_root();
        value_update_is_not_a_structure_rebuild();
        virtual_list_items_are_lazy_and_addressable();
        structure_change_retires_removed_elements();
        committed_categories_map_to_named_events();
        detach_retires_the_fragment_without_breaking_the_window();
        two_views_keep_isolated_fragments();

        std::cout << "PASS semantic UIA Win32 provider\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL semantic UIA Win32 provider (" << stage
                  << "): " << error.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "FAIL semantic UIA Win32 provider: unknown exception\n";
        return EXIT_FAILURE;
    }
}
