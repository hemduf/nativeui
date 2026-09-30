// Windows UI Automation fragment/provider implementation for accessibility.
//
// This translation unit is compiled only on Win32 into the platform target (and
// into the Windows-only provider test). It implements the same internal C ABI
// as the macOS bridge so the portable view lifecycle can attach/detach an
// accessibility bridge unconditionally.
//
// Architecture (docs/accessibility.md section 8, 10, 11):
//   * exactly one in-view UIA fragment root per NativeUI view; no second HWND.
//     WM_GETOBJECT is answered from a SetWindowSubclass hook that chains to the
//     original Pugl window procedure through DefSubclassProc;
//   * every provider is a COM object that retains only a weak per-view endpoint
//     plus stable semantic identity ({SemanticId} or {ListView SemanticId,
//     VirtualSemanticItemToken}); no Component*/Node*/Tree pointer is stored;
//   * per-view providers are materialized lazily and bounded by the number of
//     identities actually queried. A 100k-item virtual ListView creates zero
//     providers at publication time; a queried row creates exactly one;
//   * stale elements report UIA_E_ELEMENTNOTAVAILABLE and
//     UiaDisconnectProvider is called when the per-view cache retires an
//     identity;
//   * patterns exist only for the advertised semantic actions plus the frozen
//     section 8 read patterns (read-only RangeValue, Selection/ItemContainer,
//     Text);
//   * notifications are derived from the committed native publication batch:
//     structure-changed, automation-focus-changed, selection item/property
//     changed, value property changed and bounding-rectangle property changed.
//     A value-only update never becomes a structure rebuild;
//   * the COM apartment is host-owned unless this bridge initialized it;
//     RPC_E_CHANGED_MODE is tolerated and CoUninitialize is never called for an
//     apartment we did not initialize.
//
// UIA unavailability (COM initialization failure or a failed core probe) simply
// disables accessibility for that view: attach returns NULL and the native
// window stays fully functional.

#include "native_accessibility_bridge.h"

#if !defined(_WIN32)
#error "native_accessibility_windows.cpp requires the Win32 platform"
#endif

#include "native_accessibility_binding.hpp"
#include "semantic_native_bounds.hpp"

#include <nativeui/detail/semantic_uia_provider.hpp>

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif

#include <windows.h>

#include <commctrl.h>
#include <oleauto.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <utility>

/// One per-native-view Win32 accessibility bridge (the C ABI opaque type).
struct NativeUIAccessibilityBridge final {
    HWND hwnd{};
    DWORD owner_thread_id{};
    bool com_owned{};
    bool subclassed{};
    bool uia_available{true};
    std::weak_ptr<const ui::detail::SemanticNativePublicationSource> publication_source;
    std::shared_ptr<ui::detail::UiaProviderEndpoint> endpoint;
    void* recorder_user_data{};
    NativeUIAccessibilityNotificationRecorder recorder{};
};

namespace {

using ui::detail::SemanticNativePublicationBatch;
using ui::detail::UiaControlType;
using ui::detail::UiaPatternEligibility;
using ui::detail::UiaProviderEndpoint;
using ui::detail::UiaProviderHandle;
using ui::detail::UiaProviderHandlePtr;
using ui::detail::UiaProviderIdentity;
using ui::detail::UiaProviderRead;
using ui::detail::UiaProviderState;
using ui::detail::UiaValueKind;

constexpr wchar_t kFrameworkId[] = L"NativeUI";
constexpr UINT_PTR kAccessibilitySubclassId = 0x4E554941U;

constexpr char kStructureNotification[] = "structure";
constexpr char kFocusNotification[] = "focus";
constexpr char kSelectionNotification[] = "selection";
constexpr char kValueNotification[] = "value";
constexpr char kBoundsNotification[] = "bounds";

// ---------------------------------------------------------------------------
// VARIANT/SAFEARRAY helpers
// ---------------------------------------------------------------------------

[[nodiscard]] bool utf8_to_wide(const std::string& utf8,
                                std::wstring& out) noexcept {
    out.clear();
    if (utf8.empty()) {
        return true;
    }
    if (utf8.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    const int length = static_cast<int>(utf8.size());
    const int required = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                               utf8.data(), length, nullptr, 0);
    if (required <= 0) {
        return false;
    }
    try {
        out.resize(static_cast<std::size_t>(required));
    } catch (...) {
        return false;
    }
    const int written = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                              utf8.data(), length, out.data(),
                                              required);
    if (written != required) {
        out.clear();
        return false;
    }
    return true;
}

[[nodiscard]] bool wide_to_utf8(const wchar_t* text,
                                std::string& out) noexcept {
    out.clear();
    if (!text) {
        return true;
    }
    const auto length = static_cast<std::size_t>(std::wcslen(text));
    if (length == 0U) {
        return true;
    }
    if (length > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    const int source_length = static_cast<int>(length);
    const int required = ::WideCharToMultiByte(CP_UTF8, 0, text, source_length,
                                               nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return false;
    }
    try {
        out.resize(static_cast<std::size_t>(required));
    } catch (...) {
        return false;
    }
    const int written = ::WideCharToMultiByte(CP_UTF8, 0, text, source_length,
                                              out.data(), required, nullptr,
                                              nullptr);
    if (written != required) {
        out.clear();
        return false;
    }
    return true;
}

[[nodiscard]] HRESULT set_variant_bstr(VARIANT& out,
                                       const std::wstring& text) noexcept {
    ::VariantInit(&out);
    if (text.size() >
        static_cast<std::size_t>(std::numeric_limits<UINT>::max())) {
        return E_OUTOFMEMORY;
    }
    out.vt = VT_BSTR;
    out.bstrVal =
        ::SysAllocStringLen(text.data(), static_cast<UINT>(text.size()));
    if (!out.bstrVal && !text.empty()) {
        out.vt = VT_EMPTY;
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

[[nodiscard]] HRESULT set_variant_bstr_utf8(VARIANT& out,
                                            const std::string& utf8) noexcept {
    std::wstring wide;
    if (!utf8_to_wide(utf8, wide)) {
        return set_variant_bstr(out, std::wstring{});
    }
    return set_variant_bstr(out, wide);
}

void set_variant_bool(VARIANT& out, bool value) noexcept {
    ::VariantInit(&out);
    out.vt = VT_BOOL;
    out.boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
}

void set_variant_i4(VARIANT& out, LONG value) noexcept {
    ::VariantInit(&out);
    out.vt = VT_I4;
    out.lVal = value;
}

void set_variant_r8(VARIANT& out, double value) noexcept {
    ::VariantInit(&out);
    out.vt = VT_R8;
    out.dblVal = value;
}

[[nodiscard]] HRESULT set_variant_rect(VARIANT& out,
                                       const UiaRect& rect) noexcept {
    ::VariantInit(&out);
    SAFEARRAY* array = ::SafeArrayCreateVector(VT_R8, 0, 4);
    if (!array) {
        return E_OUTOFMEMORY;
    }
    const double values[4] = {rect.left, rect.top, rect.width, rect.height};
    for (LONG index = 0; index < 4; ++index) {
        if (FAILED(::SafeArrayPutElement(array, &index, &values[index]))) {
            ::SafeArrayDestroy(array);
            return E_FAIL;
        }
    }
    out.vt = VT_ARRAY | VT_R8;
    out.parray = array;
    return S_OK;
}

[[nodiscard]] HRESULT set_reserved_not_supported(VARIANT& out) noexcept {
    ::VariantInit(&out);
    IUnknown* reserved = nullptr;
    const HRESULT hr = ::UiaGetReservedNotSupportedValue(&reserved);
    if (FAILED(hr) || !reserved) {
        return S_OK;
    }
    // The reserved value is owned by UI Automation and must not be released.
    out.vt = VT_UNKNOWN;
    out.punkVal = reserved;
    return S_OK;
}

[[nodiscard]] HRESULT create_interface_array(SAFEARRAY** out,
                                             IUnknown* const* elements,
                                             LONG count) noexcept {
    if (!out) {
        return E_POINTER;
    }
    *out = nullptr;
    SAFEARRAY* array = ::SafeArrayCreateVector(VT_UNKNOWN, 0, count);
    if (!array) {
        return E_OUTOFMEMORY;
    }
    for (LONG index = 0; index < count; ++index) {
        // SafeArrayPutElement AddRefs VT_UNKNOWN elements; SafeArrayDestroy
        // releases them again.
        if (FAILED(::SafeArrayPutElement(array, &index, elements[index]))) {
            ::SafeArrayDestroy(array);
            return E_FAIL;
        }
    }
    *out = array;
    return S_OK;
}

// ---------------------------------------------------------------------------
// UIA identifier/geometry helpers
// ---------------------------------------------------------------------------

[[nodiscard]] CONTROLTYPEID uia_control_type_id(UiaControlType type) noexcept {
    switch (type) {
        case UiaControlType::Button:
            return UIA_ButtonControlTypeId;
        case UiaControlType::CheckBox:
            return UIA_CheckBoxControlTypeId;
        case UiaControlType::RadioButton:
            return UIA_RadioButtonControlTypeId;
        case UiaControlType::Slider:
            return UIA_SliderControlTypeId;
        case UiaControlType::Thumb:
            return UIA_ThumbControlTypeId;
        case UiaControlType::ProgressBar:
            return UIA_ProgressBarControlTypeId;
        case UiaControlType::Text:
            return UIA_TextControlTypeId;
        case UiaControlType::Edit:
            return UIA_EditControlTypeId;
        case UiaControlType::ComboBox:
            return UIA_ComboBoxControlTypeId;
        case UiaControlType::Menu:
            return UIA_MenuControlTypeId;
        case UiaControlType::MenuItem:
            return UIA_MenuItemControlTypeId;
        case UiaControlType::List:
            return UIA_ListControlTypeId;
        case UiaControlType::ListItem:
            return UIA_ListItemControlTypeId;
        case UiaControlType::Tab:
            return UIA_TabControlTypeId;
        case UiaControlType::TabItem:
            return UIA_TabItemControlTypeId;
        case UiaControlType::Group:
            return UIA_GroupControlTypeId;
        case UiaControlType::Window:
            return UIA_WindowControlTypeId;
        case UiaControlType::Image:
            return UIA_ImageControlTypeId;
    }
    return UIA_CustomControlTypeId;
}

[[nodiscard]] LONG toggle_state_id(const ui::SemanticInfo& info) noexcept {
    switch (info.checked) {
        case ui::SemanticCheckedState::Checked:
            return ToggleState_On;
        case ui::SemanticCheckedState::Mixed:
            return ToggleState_Indeterminate;
        case ui::SemanticCheckedState::Unchecked:
        case ui::SemanticCheckedState::NotApplicable:
            break;
    }
    return ToggleState_Off;
}

[[nodiscard]] LONG expand_collapse_state_id(
    const ui::SemanticInfo& info) noexcept {
    return info.expanded == ui::SemanticExpandedState::Expanded
        ? ExpandCollapseState_Expanded
        : ExpandCollapseState_Collapsed;
}

[[nodiscard]] std::string automation_id(const UiaProviderRead& read) {
    if (read.virtual_token()) {
        return std::to_string(*read.virtual_token());
    }
    return std::to_string(read.node_id());
}

[[nodiscard]] UiaRect physical_screen_rect(const UiaProviderRead& read) noexcept {
    const ui::Rect physical =
        ui::detail::SemanticNativeBoundsTransform{read.geometry()}
            .physical_screen_bounds(read.logical_bounds());
    UiaRect rect{};
    rect.left = static_cast<double>(physical.x);
    rect.top = static_cast<double>(physical.y);
    rect.width = static_cast<double>(physical.w);
    rect.height = static_cast<double>(physical.h);
    return rect;
}

[[nodiscard]] ui::Point physical_screen_to_logical(
    const UiaProviderRead& read,
    double x,
    double y) noexcept {
    const auto& geometry = read.geometry();
    const float scale = geometry.scale > 0.0f ? geometry.scale : 1.0f;
    return ui::Point{
        static_cast<float>(
            (x - static_cast<double>(geometry.physical_screen_origin.x)) /
            static_cast<double>(scale)),
        static_cast<float>(
            (y - static_cast<double>(geometry.physical_screen_origin.y)) /
            static_cast<double>(scale))};
}

[[nodiscard]] bool contains_point(const UiaProviderRead& read,
                                  double x,
                                  double y) noexcept {
    const UiaRect rect = physical_screen_rect(read);
    return rect.width >= 0.0 && rect.height >= 0.0 && x >= rect.left &&
           x <= rect.left + rect.width && y >= rect.top &&
           y <= rect.top + rect.height;
}

[[nodiscard]] bool is_offscreen(HWND hwnd, const UiaProviderRead& read) noexcept {
    RECT client{};
    if (!hwnd || !::GetClientRect(hwnd, &client)) {
        return false;
    }
    POINT top_left{client.left, client.top};
    POINT bottom_right{client.right, client.bottom};
    if (!::ClientToScreen(hwnd, &top_left) ||
        !::ClientToScreen(hwnd, &bottom_right)) {
        return false;
    }
    const UiaRect rect = physical_screen_rect(read);
    if (rect.width <= 0.0 || rect.height <= 0.0) {
        return true;
    }
    return rect.left >= static_cast<double>(bottom_right.x) ||
           rect.top >= static_cast<double>(bottom_right.y) ||
           rect.left + rect.width <= static_cast<double>(top_left.x) ||
           rect.top + rect.height <= static_cast<double>(top_left.y);
}

// ---------------------------------------------------------------------------
// One lazily materialized UIA provider
// ---------------------------------------------------------------------------

class UiaWin32TextRange;

class UiaWin32Provider final
    : public UiaProviderHandle,
      public IRawElementProviderSimple,
      public IRawElementProviderFragment,
      public IRawElementProviderFragmentRoot,
      public IInvokeProvider,
      public IToggleProvider,
      public ISelectionItemProvider,
      public IRangeValueProvider,
      public IExpandCollapseProvider,
      public IValueProvider,
      public ISelectionProvider,
      public IItemContainerProvider,
      public IVirtualizedItemProvider,
      public ITextProvider {
public:
    UiaWin32Provider(std::weak_ptr<UiaProviderEndpoint> endpoint,
                     UiaProviderState state,
                     UiaProviderRead read,
                     HWND hwnd) noexcept
        : endpoint_(std::move(endpoint)),
          state_(std::move(state)),
          hwnd_(hwnd),
          fragment_root_(read.fragment_root()) {}

    UiaWin32Provider(const UiaWin32Provider&) = delete;
    UiaWin32Provider& operator=(const UiaWin32Provider&) = delete;

    // -- IUnknown ----------------------------------------------------------

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                             void** object) noexcept override {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;

        // COM identity requires QueryInterface support to remain stable for the
        // lifetime of this object. Pattern availability is dynamic semantic
        // state and is exposed only through GetPatternProvider; individual
        // pattern methods still fail closed when the current snapshot no longer
        // supports the requested operation.
        void* result = nullptr;
        if (::IsEqualIID(iid, IID_IUnknown) ||
            ::IsEqualIID(iid, IID_IRawElementProviderSimple)) {
            result = static_cast<IRawElementProviderSimple*>(this);
        } else if (::IsEqualIID(iid, IID_IRawElementProviderFragment)) {
            result = static_cast<IRawElementProviderFragment*>(this);
        } else if (fragment_root_ &&
                   ::IsEqualIID(iid, IID_IRawElementProviderFragmentRoot)) {
            result = static_cast<IRawElementProviderFragmentRoot*>(this);
        } else if (::IsEqualIID(iid, IID_IInvokeProvider)) {
            result = static_cast<IInvokeProvider*>(this);
        } else if (::IsEqualIID(iid, IID_IToggleProvider)) {
            result = static_cast<IToggleProvider*>(this);
        } else if (::IsEqualIID(iid, IID_ISelectionItemProvider)) {
            result = static_cast<ISelectionItemProvider*>(this);
        } else if (::IsEqualIID(iid, IID_IRangeValueProvider)) {
            result = static_cast<IRangeValueProvider*>(this);
        } else if (::IsEqualIID(iid, IID_IExpandCollapseProvider)) {
            result = static_cast<IExpandCollapseProvider*>(this);
        } else if (::IsEqualIID(iid, IID_IValueProvider)) {
            result = static_cast<IValueProvider*>(this);
        } else if (::IsEqualIID(iid, IID_ITextProvider)) {
            result = static_cast<ITextProvider*>(this);
        } else if (::IsEqualIID(iid, IID_ISelectionProvider)) {
            result = static_cast<ISelectionProvider*>(this);
        } else if (::IsEqualIID(iid, IID_IItemContainerProvider)) {
            result = static_cast<IItemContainerProvider*>(this);
        } else if (::IsEqualIID(iid, IID_IVirtualizedItemProvider)) {
            result = static_cast<IVirtualizedItemProvider*>(this);
        } else {
            return E_NOINTERFACE;
        }

        AddRef();
        *object = result;
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override {
        return ref_count_.fetch_add(1U, std::memory_order_relaxed) + 1U;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override {
        const ULONG previous = ref_count_.fetch_sub(1U, std::memory_order_acq_rel);
        if (previous == 1U) {
            delete this;
            return 0U;
        }
        return previous - 1U;
    }

    // -- UiaProviderHandle -------------------------------------------------

    void on_retired() noexcept override {
        // Tell UI Automation that this provider is gone before the per-view
        // cache releases its reference, so clients stop reaching a retired
        // element and no raised-event map entry keeps it alive.
        (void)::UiaDisconnectProvider(
            static_cast<IRawElementProviderSimple*>(this));
    }

    // -- IRawElementProviderSimple ----------------------------------------

    HRESULT STDMETHODCALLTYPE get_ProviderOptions(
        ProviderOptions* options) noexcept override {
        if (!options) {
            return E_POINTER;
        }
        *options = ProviderOptions_ServerSideProvider;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPatternProvider(
        PATTERNID pattern_id,
        IUnknown** pattern) noexcept override {
        if (!pattern) {
            return E_POINTER;
        }
        *pattern = nullptr;
        const auto read = current_read();
        if (!read) {
            return S_OK;
        }
        const auto& patterns = read->patterns();

        IUnknown* result = nullptr;
        switch (pattern_id) {
            case UIA_InvokePatternId:
                if (patterns.invoke) {
                    result = static_cast<IInvokeProvider*>(this);
                }
                break;
            case UIA_TogglePatternId:
                if (patterns.toggle) {
                    result = static_cast<IToggleProvider*>(this);
                }
                break;
            case UIA_SelectionItemPatternId:
                if (patterns.selection_item) {
                    result = static_cast<ISelectionItemProvider*>(this);
                }
                break;
            case UIA_RangeValuePatternId:
                if (patterns.range_value) {
                    result = static_cast<IRangeValueProvider*>(this);
                }
                break;
            case UIA_ExpandCollapsePatternId:
                if (patterns.expand_collapse) {
                    result = static_cast<IExpandCollapseProvider*>(this);
                }
                break;
            case UIA_ValuePatternId:
                if (patterns.value) {
                    result = static_cast<IValueProvider*>(this);
                }
                break;
            case UIA_TextPatternId:
                if (patterns.text) {
                    result = static_cast<ITextProvider*>(this);
                }
                break;
            case UIA_SelectionPatternId:
                if (patterns.selection) {
                    result = static_cast<ISelectionProvider*>(this);
                }
                break;
            case UIA_ItemContainerPatternId:
                if (patterns.item_container) {
                    result = static_cast<IItemContainerProvider*>(this);
                }
                break;
            case UIA_VirtualizedItemPatternId:
                if (patterns.virtualized_item) {
                    result = static_cast<IVirtualizedItemProvider*>(this);
                }
                break;
            default:
                break;
        }

        if (result) {
            result->AddRef();
            *pattern = result;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyValue(
        PROPERTYID property_id,
        VARIANT* value) noexcept override {
        if (!value) {
            return E_POINTER;
        }
        ::VariantInit(value);

        const auto read = current_read();
        if (!read) {
            return set_reserved_not_supported(*value);
        }
        const auto& info = read->info();
        const auto& patterns = read->patterns();
        const auto& mapping = read->role_mapping();

        switch (property_id) {
            case UIA_ControlTypePropertyId:
                set_variant_i4(*value, uia_control_type_id(mapping.control_type));
                return S_OK;
            case UIA_NamePropertyId:
                return set_variant_bstr_utf8(*value, info.name);
            case UIA_HelpTextPropertyId:
                return set_variant_bstr_utf8(*value, info.description);
            case UIA_AutomationIdPropertyId:
                return set_variant_bstr_utf8(*value, automation_id(*read));
            case UIA_FrameworkIdPropertyId:
                return set_variant_bstr(*value, kFrameworkId);
            case UIA_ProcessIdPropertyId:
                set_variant_i4(*value,
                               static_cast<LONG>(::GetCurrentProcessId()));
                return S_OK;
            case UIA_IsEnabledPropertyId:
                set_variant_bool(*value, info.enabled);
                return S_OK;
            case UIA_IsKeyboardFocusablePropertyId:
                set_variant_bool(*value, info.focusable);
                return S_OK;
            case UIA_HasKeyboardFocusPropertyId:
                set_variant_bool(*value, info.focused);
                return S_OK;
            case UIA_IsContentElementPropertyId:
            case UIA_IsControlElementPropertyId:
                set_variant_bool(*value, true);
                return S_OK;
            case UIA_IsOffscreenPropertyId:
                set_variant_bool(*value, is_offscreen(hwnd_, *read));
                return S_OK;
            case UIA_BoundingRectanglePropertyId:
                return set_variant_rect(*value, physical_screen_rect(*read));
            case UIA_IsInvokePatternAvailablePropertyId:
                set_variant_bool(*value, patterns.invoke);
                return S_OK;
            case UIA_IsTogglePatternAvailablePropertyId:
                set_variant_bool(*value, patterns.toggle);
                return S_OK;
            case UIA_IsSelectionItemPatternAvailablePropertyId:
                set_variant_bool(*value, patterns.selection_item);
                return S_OK;
            case UIA_IsRangeValuePatternAvailablePropertyId:
                set_variant_bool(*value, patterns.range_value);
                return S_OK;
            case UIA_IsExpandCollapsePatternAvailablePropertyId:
                set_variant_bool(*value, patterns.expand_collapse);
                return S_OK;
            case UIA_IsValuePatternAvailablePropertyId:
                set_variant_bool(*value, patterns.value);
                return S_OK;
            case UIA_IsTextPatternAvailablePropertyId:
                set_variant_bool(*value, patterns.text);
                return S_OK;
            case UIA_IsSelectionPatternAvailablePropertyId:
                set_variant_bool(*value, patterns.selection);
                return S_OK;
            case UIA_IsItemContainerPatternAvailablePropertyId:
                set_variant_bool(*value, patterns.item_container);
                return S_OK;
            case UIA_IsVirtualizedItemPatternAvailablePropertyId:
                set_variant_bool(*value, patterns.virtualized_item);
                return S_OK;
            case UIA_ToggleToggleStatePropertyId:
                if (patterns.toggle) {
                    set_variant_i4(*value, toggle_state_id(info));
                    return S_OK;
                }
                break;
            case UIA_SelectionItemIsSelectedPropertyId:
                if (patterns.selection_item) {
                    set_variant_bool(*value, info.selected);
                    return S_OK;
                }
                break;
            case UIA_ExpandCollapseExpandCollapseStatePropertyId:
                if (patterns.expand_collapse) {
                    set_variant_i4(*value, expand_collapse_state_id(info));
                    return S_OK;
                }
                break;
            case UIA_ValueValuePropertyId:
                if (patterns.value) {
                    return set_variant_bstr_utf8(
                        *value, info.text_value.value_or(std::string{}));
                }
                break;
            case UIA_ValueIsReadOnlyPropertyId:
                if (patterns.value) {
                    set_variant_bool(*value, !info.enabled || info.read_only);
                    return S_OK;
                }
                break;
            case UIA_RangeValueValuePropertyId:
                if (patterns.range_value) {
                    set_variant_r8(*value, info.numeric_value.value_or(0.0));
                    return S_OK;
                }
                break;
            case UIA_RangeValueMinimumPropertyId:
                if (patterns.range_value) {
                    set_variant_r8(
                        *value,
                        info.value_range.value_or(ui::SemanticValueRange{})
                            .minimum);
                    return S_OK;
                }
                break;
            case UIA_RangeValueMaximumPropertyId:
                if (patterns.range_value) {
                    set_variant_r8(
                        *value,
                        info.value_range.value_or(ui::SemanticValueRange{})
                            .maximum);
                    return S_OK;
                }
                break;
            case UIA_RangeValueSmallChangePropertyId:
            case UIA_RangeValueLargeChangePropertyId:
                if (patterns.range_value) {
                    set_variant_r8(
                        *value,
                        info.value_range.value_or(ui::SemanticValueRange{})
                            .step);
                    return S_OK;
                }
                break;
            case UIA_RangeValueIsReadOnlyPropertyId:
                if (patterns.range_value) {
                    set_variant_bool(*value,
                                     !patterns.range_value_writable ||
                                         info.read_only || !info.enabled);
                    return S_OK;
                }
                break;
            default:
                break;
        }

        return set_reserved_not_supported(*value);
    }

    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(
        IRawElementProviderSimple** host) noexcept override {
        if (!host) {
            return E_POINTER;
        }
        *host = nullptr;
        if (!fragment_root_ || !hwnd_) {
            return S_OK;
        }
        return ::UiaHostProviderFromHwnd(hwnd_, host);
    }

    // -- IRawElementProviderFragment --------------------------------------

    HRESULT STDMETHODCALLTYPE Navigate(
        NavigateDirection direction,
        IRawElementProviderFragment** result) noexcept override {
        if (!result) {
            return E_POINTER;
        }
        *result = nullptr;

        const auto endpoint = endpoint_.lock();
        if (!endpoint) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        const auto identity = state_.identity();

        std::optional<UiaProviderIdentity> target;
        switch (direction) {
            case NavigateDirection_Parent:
                if (fragment_root_) {
                    return S_OK;
                }
                target = parent_identity(*endpoint, identity);
                break;
            case NavigateDirection_FirstChild:
                target = child_identity(*endpoint, identity, true);
                break;
            case NavigateDirection_LastChild:
                target = child_identity(*endpoint, identity, false);
                break;
            case NavigateDirection_NextSibling:
                if (!fragment_root_) {
                    target = sibling_identity(*endpoint, identity, true);
                }
                break;
            case NavigateDirection_PreviousSibling:
                if (!fragment_root_) {
                    target = sibling_identity(*endpoint, identity, false);
                }
                break;
            default:
                return E_INVALIDARG;
        }

        if (!target) {
            return S_OK;
        }
        auto provider = endpoint->provider_for(*target);
        if (!provider) {
            return S_OK;
        }
        auto* win32_provider = static_cast<UiaWin32Provider*>(provider.get());
        *result = static_cast<IRawElementProviderFragment*>(win32_provider);
        win32_provider->AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetRuntimeId(
        SAFEARRAY** runtime_id) noexcept override {
        if (!runtime_id) {
            return E_POINTER;
        }
        *runtime_id = nullptr;

        const bool virtual_item = state_.virtual_token().has_value();
        const LONG count = virtual_item ? 5 : 3;
        SAFEARRAY* array = ::SafeArrayCreateVector(VT_I4, 0, count);
        if (!array) {
            return E_OUTOFMEMORY;
        }

        LONG index = 0;
        const auto put = [&array, &index](LONG value) noexcept {
            if (FAILED(::SafeArrayPutElement(array, &index, &value))) {
                return false;
            }
            ++index;
            return true;
        };

        const auto node = state_.node_id();
        const bool written = put(UiaAppendRuntimeId) &&
                             put(static_cast<LONG>(node & 0xffffffffULL)) &&
                             put(static_cast<LONG>(node >> 32U));
        bool virtual_written = true;
        if (written && virtual_item) {
            const auto token = *state_.virtual_token();
            virtual_written =
                put(static_cast<LONG>(token & 0xffffffffULL)) &&
                put(static_cast<LONG>(token >> 32U));
        }
        if (!written || !virtual_written) {
            ::SafeArrayDestroy(array);
            return E_FAIL;
        }
        *runtime_id = array;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_BoundingRectangle(
        UiaRect* rect) noexcept override {
        if (!rect) {
            return E_POINTER;
        }
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        *rect = physical_screen_rect(*read);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEmbeddedFragmentRoots(
        SAFEARRAY** roots) noexcept override {
        if (!roots) {
            return E_POINTER;
        }
        *roots = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetFocus() noexcept override {
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!ui::detail::semantic_action_allowed(read->info(),
                                                 ui::SemanticAction::Focus)) {
            return UIA_E_NOTSUPPORTED;
        }
        return state_.post_action(ui::SemanticAction::Focus)
            ? S_OK
            : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE get_FragmentRoot(
        IRawElementProviderFragmentRoot** root) noexcept override {
        if (!root) {
            return E_POINTER;
        }
        *root = nullptr;
        if (fragment_root_) {
            *root = static_cast<IRawElementProviderFragmentRoot*>(this);
            AddRef();
            return S_OK;
        }

        const auto endpoint = endpoint_.lock();
        if (!endpoint) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        auto provider = endpoint->root();
        if (!provider) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        auto* win32_provider = static_cast<UiaWin32Provider*>(provider.get());
        *root = static_cast<IRawElementProviderFragmentRoot*>(win32_provider);
        win32_provider->AddRef();
        return S_OK;
    }

    // -- IRawElementProviderFragmentRoot ----------------------------------

    HRESULT STDMETHODCALLTYPE ElementProviderFromPoint(
        double x,
        double y,
        IRawElementProviderFragment** result) noexcept override {
        if (!result) {
            return E_POINTER;
        }
        *result = nullptr;
        if (!fragment_root_) {
            return S_OK;
        }

        const auto endpoint = endpoint_.lock();
        if (!endpoint) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        const auto hit = hit_test(*endpoint, state_.identity(), x, y);
        if (!hit) {
            return S_OK;
        }
        auto provider = endpoint->provider_for(*hit);
        if (!provider) {
            return S_OK;
        }
        auto* win32_provider = static_cast<UiaWin32Provider*>(provider.get());
        *result = static_cast<IRawElementProviderFragment*>(win32_provider);
        win32_provider->AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetFocus(
        IRawElementProviderFragment** result) noexcept override {
        if (!result) {
            return E_POINTER;
        }
        *result = nullptr;
        if (!fragment_root_) {
            return S_OK;
        }

        const auto endpoint = endpoint_.lock();
        if (!endpoint) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        auto provider = endpoint->focused();
        if (!provider) {
            return S_OK;
        }
        auto* win32_provider = static_cast<UiaWin32Provider*>(provider.get());
        *result = static_cast<IRawElementProviderFragment*>(win32_provider);
        win32_provider->AddRef();
        return S_OK;
    }

    // -- IInvokeProvider ---------------------------------------------------

    HRESULT STDMETHODCALLTYPE Invoke() noexcept override {
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        return state_.post_action(ui::SemanticAction::Activate)
            ? S_OK
            : UIA_E_ELEMENTNOTAVAILABLE;
    }

    // -- IToggleProvider ---------------------------------------------------

    HRESULT STDMETHODCALLTYPE Toggle() noexcept override {
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().toggle) {
            return UIA_E_NOTSUPPORTED;
        }
        return state_.post_action(ui::SemanticAction::Toggle)
            ? S_OK
            : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE get_ToggleState(
        ToggleState* state) noexcept override {
        if (!state) {
            return E_POINTER;
        }
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().toggle) {
            return UIA_E_NOTSUPPORTED;
        }
        *state = static_cast<ToggleState>(toggle_state_id(read->info()));
        return S_OK;
    }

    // -- ISelectionItemProvider -------------------------------------------

    HRESULT STDMETHODCALLTYPE Select() noexcept override {
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        return state_.post_action(ui::SemanticAction::Select)
            ? S_OK
            : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE AddToSelection() noexcept override {
        // The frozen v1 semantic model has one selected flag per container.
        return UIA_E_INVALIDOPERATION;
    }

    HRESULT STDMETHODCALLTYPE RemoveFromSelection() noexcept override {
        return UIA_E_INVALIDOPERATION;
    }

    HRESULT STDMETHODCALLTYPE get_IsSelected(BOOL* selected) noexcept override {
        if (!selected) {
            return E_POINTER;
        }
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().selection_item) {
            return UIA_E_NOTSUPPORTED;
        }
        *selected = read->info().selected ? TRUE : FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_SelectionContainer(
        IRawElementProviderSimple** container) noexcept override {
        if (!container) {
            return E_POINTER;
        }
        *container = nullptr;
        const auto endpoint = endpoint_.lock();
        if (!endpoint) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        auto parent = endpoint->parent(state_.identity());
        if (!parent) {
            return S_OK;
        }
        auto* win32_provider = static_cast<UiaWin32Provider*>(parent.get());
        *container = static_cast<IRawElementProviderSimple*>(win32_provider);
        win32_provider->AddRef();
        return S_OK;
    }

    // -- IRangeValueProvider ----------------------------------------------

    HRESULT STDMETHODCALLTYPE SetValue(double value) noexcept override {
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().range_value ||
            !read->patterns().range_value_writable ||
            read->info().read_only || !read->info().enabled ||
            !std::isfinite(value)) {
            return UIA_E_NOTSUPPORTED;
        }
        if (const auto range = read->info().value_range) {
            value = std::clamp(value, range->minimum, range->maximum);
        }
        return state_.post_action(ui::SemanticAction::SetValue, value)
            ? S_OK
            : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE get_Value(double* value) noexcept override {
        if (!value) {
            return E_POINTER;
        }
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().range_value) {
            return UIA_E_NOTSUPPORTED;
        }
        *value = read->info().numeric_value.value_or(
            read->info().value_range.value_or(ui::SemanticValueRange{}).minimum);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_IsReadOnly(BOOL* read_only) noexcept override {
        if (!read_only) {
            return E_POINTER;
        }
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        const auto& patterns = read->patterns();
        // Shared by IRangeValueProvider and IValueProvider; no standard role
        // advertises both, and the frozen mapping keeps them disjoint.
        if (!patterns.range_value && !patterns.value) {
            return UIA_E_NOTSUPPORTED;
        }
        *read_only = (!read->info().enabled || read->info().read_only ||
                      (patterns.range_value && !patterns.range_value_writable))
            ? TRUE
            : FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_Maximum(double* maximum) noexcept override {
        if (!maximum) {
            return E_POINTER;
        }
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().range_value) {
            return UIA_E_NOTSUPPORTED;
        }
        *maximum =
            read->info().value_range.value_or(ui::SemanticValueRange{}).maximum;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_Minimum(double* minimum) noexcept override {
        if (!minimum) {
            return E_POINTER;
        }
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().range_value) {
            return UIA_E_NOTSUPPORTED;
        }
        *minimum =
            read->info().value_range.value_or(ui::SemanticValueRange{}).minimum;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_LargeChange(double* change) noexcept override {
        return range_change(change);
    }

    HRESULT STDMETHODCALLTYPE get_SmallChange(double* change) noexcept override {
        return range_change(change);
    }

    // -- IExpandCollapseProvider -------------------------------------------

    HRESULT STDMETHODCALLTYPE Expand() noexcept override {
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().expand_collapse) {
            return UIA_E_NOTSUPPORTED;
        }
        return state_.post_action(ui::SemanticAction::Expand)
            ? S_OK
            : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE Collapse() noexcept override {
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().expand_collapse) {
            return UIA_E_NOTSUPPORTED;
        }
        return state_.post_action(ui::SemanticAction::Collapse)
            ? S_OK
            : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE get_ExpandCollapseState(
        ExpandCollapseState* state) noexcept override {
        if (!state) {
            return E_POINTER;
        }
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().expand_collapse) {
            return UIA_E_NOTSUPPORTED;
        }
        *state = static_cast<ExpandCollapseState>(
            expand_collapse_state_id(read->info()));
        return S_OK;
    }

    // -- IValueProvider ----------------------------------------------------

    HRESULT STDMETHODCALLTYPE SetValue(LPCWSTR value) noexcept override {
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().value || read->info().read_only ||
            !read->info().enabled) {
            return UIA_E_NOTSUPPORTED;
        }
        if (!value) {
            return E_INVALIDARG;
        }
        std::string utf8;
        if (!wide_to_utf8(value, utf8)) {
            return E_INVALIDARG;
        }
        return state_.post_action(ui::SemanticAction::SetValue, std::nullopt,
                                  std::move(utf8))
            ? S_OK
            : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE get_Value(BSTR* value) noexcept override {
        if (!value) {
            return E_POINTER;
        }
        *value = nullptr;
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().value) {
            return UIA_E_NOTSUPPORTED;
        }
        std::wstring wide;
        if (!utf8_to_wide(read->info().text_value.value_or(std::string{}),
                          wide)) {
            wide.clear();
        }
        *value =
            ::SysAllocStringLen(wide.data(), static_cast<UINT>(wide.size()));
        return *value ? S_OK : E_OUTOFMEMORY;
    }

    // -- ISelectionProvider / ITextProvider selection ---------------------

    HRESULT STDMETHODCALLTYPE GetSelection(
        SAFEARRAY** selection) noexcept override {
        if (!selection) {
            return E_POINTER;
        }
        *selection = nullptr;

        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        const auto& patterns = read->patterns();

        // Both ISelectionProvider and ITextProvider declare this method. No
        // frozen role advertises both, so the pattern decides the content.
        if (patterns.selection) {
            const auto endpoint = endpoint_.lock();
            if (!endpoint) {
                return UIA_E_ELEMENTNOTAVAILABLE;
            }
            auto selected = endpoint->selected_child(state_.node_id());
            if (!selected) {
                return create_interface_array(selection, nullptr, 0);
            }
            auto* win32_provider =
                static_cast<UiaWin32Provider*>(selected.get());
            IUnknown* element = static_cast<IUnknown*>(
                static_cast<IRawElementProviderSimple*>(win32_provider));
            return create_interface_array(selection, &element, 1);
        }
        if (patterns.text) {
            // The frozen semantic model exposes no text selection; the
            // supported selection value is None and the selection is empty.
            return create_interface_array(selection, nullptr, 0);
        }
        return UIA_E_NOTSUPPORTED;
    }

    HRESULT STDMETHODCALLTYPE get_CanSelectMultiple(
        BOOL* multiple) noexcept override {
        if (!multiple) {
            return E_POINTER;
        }
        *multiple = FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_IsSelectionRequired(
        BOOL* required) noexcept override {
        if (!required) {
            return E_POINTER;
        }
        *required = FALSE;
        return S_OK;
    }

    // -- IItemContainerProvider --------------------------------------------

    HRESULT STDMETHODCALLTYPE FindItemByProperty(
        IRawElementProviderSimple* start_after,
        PROPERTYID property_id,
        VARIANT value,
        IRawElementProviderSimple** found) noexcept override {
        if (!found) {
            return E_POINTER;
        }
        *found = nullptr;

        const auto read = current_read();
        const auto endpoint = endpoint_.lock();
        if (!read || !endpoint) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().item_container) {
            return UIA_E_NOTSUPPORTED;
        }

        const std::size_t total = read->virtual_child_count();
        if (total == 0U) {
            return S_OK;
        }

        std::size_t start = 0U;
        if (start_after) {
            const auto previous = identity_from_provider(start_after);
            if (previous && previous->node_id == state_.node_id() &&
                previous->virtual_token) {
                const auto index = endpoint->virtual_child_index_of(
                    state_.node_id(), *previous->virtual_token);
                if (!index || *index + 1U >= total) {
                    return S_OK;
                }
                start = *index + 1U;
            }
        }

        if (property_id == UIA_AutomationIdPropertyId && value.vt == VT_BSTR) {
            const auto token = parse_token(value.bstrVal);
            if (!token) {
                return S_OK;
            }
            const auto index =
                endpoint->virtual_child_index_of(state_.node_id(), *token);
            if (!index || *index < start) {
                return S_OK;
            }
            return expose_provider(*endpoint,
                                   UiaProviderIdentity{state_.node_id(), *token},
                                   found);
        }

        if (property_id == UIA_NamePropertyId && value.vt == VT_BSTR) {
            const std::wstring wanted{value.bstrVal};
            for (std::size_t index = start; index < total; ++index) {
                const auto token =
                    endpoint->virtual_child_token_at(state_.node_id(), index);
                if (!token) {
                    continue;
                }
                const UiaProviderIdentity identity{state_.node_id(), *token};
                const auto item_read = endpoint->read(identity);
                if (!item_read) {
                    continue;
                }
                std::wstring name;
                if (!utf8_to_wide(item_read->info().name, name)) {
                    continue;
                }
                if (name != wanted) {
                    continue;
                }
                return expose_provider(*endpoint, identity, found);
            }
            return S_OK;
        }

        return S_OK;
    }

    // -- IVirtualizedItemProvider ------------------------------------------

    HRESULT STDMETHODCALLTYPE Realize() noexcept override {
        // The lazily materialized provider already is the fully addressable
        // semantic item; virtual-list owns visual row materialization/scroll policy.
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        return S_OK;
    }

    // -- ITextProvider (read-only document range) --------------------------

    HRESULT STDMETHODCALLTYPE GetVisibleRanges(
        SAFEARRAY** ranges) noexcept override {
        if (!ranges) {
            return E_POINTER;
        }
        *ranges = nullptr;
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().text) {
            return UIA_E_NOTSUPPORTED;
        }
        ITextRangeProvider* document = create_document_range();
        if (!document) {
            return E_OUTOFMEMORY;
        }
        IUnknown* element = static_cast<IUnknown*>(document);
        const HRESULT hr = create_interface_array(ranges, &element, 1);
        document->Release();
        return hr;
    }

    HRESULT STDMETHODCALLTYPE RangeFromChild(
        IRawElementProviderSimple* child,
        ITextRangeProvider** range) noexcept override {
        if (!range) {
            return E_POINTER;
        }
        *range = nullptr;
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().text) {
            return UIA_E_NOTSUPPORTED;
        }
        if (child) {
            IUnknown* other = nullptr;
            if (FAILED(child->QueryInterface(
                    IID_IUnknown, reinterpret_cast<void**>(&other))) ||
                !other) {
                return S_OK;
            }
            const bool same =
                other == static_cast<IUnknown*>(
                             static_cast<IRawElementProviderSimple*>(this));
            other->Release();
            if (!same) {
                return S_OK;
            }
        }
        *range = create_document_range();
        return *range ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE RangeFromPoint(
        UiaPoint point,
        ITextRangeProvider** range) noexcept override {
        (void)point;
        if (!range) {
            return E_POINTER;
        }
        *range = nullptr;
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().text) {
            return UIA_E_NOTSUPPORTED;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_DocumentRange(
        ITextRangeProvider** range) noexcept override {
        if (!range) {
            return E_POINTER;
        }
        *range = nullptr;
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().text) {
            return UIA_E_NOTSUPPORTED;
        }
        *range = create_document_range();
        return *range ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE get_SupportedTextSelection(
        SupportedTextSelection* supported) noexcept override {
        if (!supported) {
            return E_POINTER;
        }
        *supported = SupportedTextSelection_None;
        return S_OK;
    }

private:
    friend class UiaWin32TextRange;

    static constexpr LONG kWholeDocument = -1;

    [[nodiscard]] ITextRangeProvider* create_document_range() noexcept;

    [[nodiscard]] std::optional<UiaProviderRead> current_read() const noexcept {
        try {
            return UiaProviderRead::from_state(state_);
        } catch (...) {
            return std::nullopt;
        }
    }

    [[nodiscard]] HRESULT range_change(double* change) noexcept {
        if (!change) {
            return E_POINTER;
        }
        const auto read = current_read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!read->patterns().range_value) {
            return UIA_E_NOTSUPPORTED;
        }
        *change =
            read->info().value_range.value_or(ui::SemanticValueRange{}).step;
        return S_OK;
    }

    [[nodiscard]] static std::optional<UiaProviderIdentity> parent_identity(
        UiaProviderEndpoint& endpoint,
        const UiaProviderIdentity& identity) noexcept {
        const auto read = endpoint.read(identity);
        if (!read) {
            return std::nullopt;
        }
        const ui::SemanticId parent = read->parent_id();
        if (parent == ui::kInvalidSemanticId) {
            return std::nullopt;
        }
        return UiaProviderIdentity{parent, std::nullopt};
    }

    [[nodiscard]] static std::optional<UiaProviderIdentity> child_identity(
        UiaProviderEndpoint& endpoint,
        const UiaProviderIdentity& identity,
        bool first) noexcept {
        const auto read = endpoint.read(identity);
        if (!read) {
            return std::nullopt;
        }
        const std::size_t ordinary = read->child_count();
        const std::size_t virtual_count = read->virtual_child_count();
        // The frozen model never mixes ordinary and logical-virtual children;
        // a malformed generation fails closed instead of inventing an order.
        if (ordinary != 0U && virtual_count != 0U) {
            return std::nullopt;
        }
        if (virtual_count != 0U) {
            const auto token =
                read->virtual_child_token_at(first ? 0U : virtual_count - 1U);
            if (!token) {
                return std::nullopt;
            }
            return UiaProviderIdentity{identity.node_id, *token};
        }
        if (ordinary == 0U) {
            return std::nullopt;
        }
        const auto child = read->child_at(first ? 0U : ordinary - 1U);
        if (!child || *child == ui::kInvalidSemanticId) {
            return std::nullopt;
        }
        return UiaProviderIdentity{*child, std::nullopt};
    }

    [[nodiscard]] static std::optional<UiaProviderIdentity> sibling_identity(
        UiaProviderEndpoint& endpoint,
        const UiaProviderIdentity& identity,
        bool next) noexcept {
        if (identity.virtual_token) {
            const auto list_read = endpoint.read(
                UiaProviderIdentity{identity.node_id, std::nullopt});
            if (!list_read) {
                return std::nullopt;
            }
            const auto index =
                list_read->virtual_child_index_of(*identity.virtual_token);
            if (!index || (!next && *index == 0U)) {
                return std::nullopt;
            }
            const std::size_t neighbor = next ? *index + 1U : *index - 1U;
            if (neighbor >= list_read->virtual_child_count()) {
                return std::nullopt;
            }
            const auto token = list_read->virtual_child_token_at(neighbor);
            if (!token) {
                return std::nullopt;
            }
            return UiaProviderIdentity{identity.node_id, *token};
        }

        const auto read = endpoint.read(identity);
        if (!read) {
            return std::nullopt;
        }
        const ui::SemanticId parent = read->parent_id();
        if (parent == ui::kInvalidSemanticId) {
            return std::nullopt;
        }
        const auto parent_read =
            endpoint.read(UiaProviderIdentity{parent, std::nullopt});
        if (!parent_read) {
            return std::nullopt;
        }
        const auto index =
            parent_read->ordinary_child_index_of(identity.node_id);
        if (!index || (!next && *index == 0U)) {
            return std::nullopt;
        }
        const std::size_t neighbor = next ? *index + 1U : *index - 1U;
        const auto child = parent_read->child_at(neighbor);
        if (!child || *child == ui::kInvalidSemanticId) {
            return std::nullopt;
        }
        return UiaProviderIdentity{*child, std::nullopt};
    }

    [[nodiscard]] static std::optional<UiaProviderIdentity> hit_test(
        UiaProviderEndpoint& endpoint,
        const UiaProviderIdentity& identity,
        double x,
        double y) noexcept {
        const auto read = endpoint.read(identity);
        if (!read || !contains_point(*read, x, y)) {
            return std::nullopt;
        }

        const std::size_t ordinary = read->child_count();
        const std::size_t virtual_count = read->virtual_child_count();
        if (ordinary != 0U && virtual_count != 0U) {
            return identity;
        }

        if (virtual_count != 0U) {
            // O(1) row arithmetic through the retained fixed-height/scroll
            // transform; never enumerates or mounts logical rows.
            const ui::Point logical = physical_screen_to_logical(*read, x, y);
            const auto index =
                read->virtual_child_index_at_logical_y(logical.y);
            if (index) {
                const auto token = read->virtual_child_token_at(*index);
                if (token) {
                    const UiaProviderIdentity item{identity.node_id, *token};
                    const auto item_read = endpoint.read(item);
                    if (item_read && contains_point(*item_read, x, y)) {
                        return item;
                    }
                }
            }
            return identity;
        }

        for (std::size_t index = 0U; index < ordinary; ++index) {
            const auto child = read->child_at(index);
            if (!child || *child == ui::kInvalidSemanticId) {
                continue;
            }
            const auto hit = hit_test(
                endpoint, UiaProviderIdentity{*child, std::nullopt}, x, y);
            if (hit) {
                return hit;
            }
        }
        return identity;
    }

    [[nodiscard]] static std::optional<UiaProviderIdentity>
    identity_from_provider(IRawElementProviderSimple* provider) noexcept {
        if (!provider) {
            return std::nullopt;
        }
        IRawElementProviderFragment* fragment = nullptr;
        if (FAILED(provider->QueryInterface(
                IID_IRawElementProviderFragment,
                reinterpret_cast<void**>(&fragment))) ||
            !fragment) {
            return std::nullopt;
        }
        SAFEARRAY* runtime_id = nullptr;
        const HRESULT hr = fragment->GetRuntimeId(&runtime_id);
        fragment->Release();
        if (FAILED(hr) || !runtime_id) {
            return std::nullopt;
        }

        std::optional<UiaProviderIdentity> identity;
        LONG lower = 0;
        LONG upper = -1;
        if (SUCCEEDED(::SafeArrayGetLBound(runtime_id, 1, &lower)) &&
            SUCCEEDED(::SafeArrayGetUBound(runtime_id, 1, &upper)) &&
            lower == 0 && (upper == 2 || upper == 4)) {
            LONG values[5] = {0, 0, 0, 0, 0};
            bool valid = true;
            for (LONG index = 0; index <= upper; ++index) {
                if (FAILED(::SafeArrayGetElement(runtime_id, &index,
                                                 &values[index]))) {
                    valid = false;
                    break;
                }
            }
            if (valid && values[0] == UiaAppendRuntimeId) {
                const auto low = static_cast<std::uint32_t>(values[1]);
                const auto high = static_cast<std::uint32_t>(values[2]);
                const auto node = static_cast<ui::SemanticId>(low) |
                                  (static_cast<ui::SemanticId>(high) << 32U);
                if (upper == 4) {
                    const auto token_low =
                        static_cast<std::uint32_t>(values[3]);
                    const auto token_high =
                        static_cast<std::uint32_t>(values[4]);
                    const auto token =
                        static_cast<ui::VirtualSemanticItemToken>(token_low) |
                        (static_cast<ui::VirtualSemanticItemToken>(token_high)
                         << 32U);
                    identity = UiaProviderIdentity{node, token};
                } else {
                    identity = UiaProviderIdentity{node, std::nullopt};
                }
            }
        }
        ::SafeArrayDestroy(runtime_id);
        return identity;
    }

    [[nodiscard]] static std::optional<ui::VirtualSemanticItemToken> parse_token(
        const BSTR text) noexcept {
        if (!text) {
            return std::nullopt;
        }
        unsigned long long token = 0U;
        std::size_t digits = 0U;
        for (const wchar_t* cursor = text; *cursor != L'\0'; ++cursor) {
            ++digits;
            if (digits > 20U || *cursor < L'0' || *cursor > L'9') {
                return std::nullopt;
            }
            token =
                token * 10U + static_cast<unsigned long long>(*cursor - L'0');
        }
        if (digits == 0U || token == 0U) {
            return std::nullopt;
        }
        return static_cast<ui::VirtualSemanticItemToken>(token);
    }

    [[nodiscard]] static HRESULT expose_provider(
        UiaProviderEndpoint& endpoint,
        const UiaProviderIdentity& identity,
        IRawElementProviderSimple** found) noexcept {
        auto provider = endpoint.provider_for(identity);
        if (!provider) {
            return S_OK;
        }
        auto* win32_provider = static_cast<UiaWin32Provider*>(provider.get());
        *found = static_cast<IRawElementProviderSimple*>(win32_provider);
        win32_provider->AddRef();
        return S_OK;
    }

    std::atomic<ULONG> ref_count_{0U};
    std::weak_ptr<UiaProviderEndpoint> endpoint_;
    UiaProviderState state_;
    HWND hwnd_{};
    bool fragment_root_{};
};

// ---------------------------------------------------------------------------
// Read-only document text range over one provider's exact text value
// ---------------------------------------------------------------------------

class UiaWin32TextRange final : public ITextRangeProvider {
public:
    UiaWin32TextRange(std::weak_ptr<UiaProviderEndpoint> endpoint,
                      UiaProviderState state,
                      LONG start,
                      LONG end) noexcept
        : endpoint_(std::move(endpoint)),
          state_(std::move(state)),
          start_(start),
          end_(end) {}

    UiaWin32TextRange(const UiaWin32TextRange&) = delete;
    UiaWin32TextRange& operator=(const UiaWin32TextRange&) = delete;

    virtual ~UiaWin32TextRange() noexcept = default;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                             void** object) noexcept override {
        if (!object) {
            return E_POINTER;
        }
        *object = nullptr;
        if (::IsEqualIID(iid, IID_IUnknown) ||
            ::IsEqualIID(iid, IID_ITextRangeProvider)) {
            AddRef();
            *object = static_cast<ITextRangeProvider*>(this);
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() noexcept override {
        return ref_count_.fetch_add(1U, std::memory_order_relaxed) + 1U;
    }

    ULONG STDMETHODCALLTYPE Release() noexcept override {
        const ULONG previous = ref_count_.fetch_sub(1U, std::memory_order_acq_rel);
        if (previous == 1U) {
            delete this;
            return 0U;
        }
        return previous - 1U;
    }

    HRESULT STDMETHODCALLTYPE Clone(
        ITextRangeProvider** clone) noexcept override {
        if (!clone) {
            return E_POINTER;
        }
        *clone = nullptr;
        auto* copy =
            new (std::nothrow) UiaWin32TextRange(endpoint_, state_, start_, end_);
        if (!copy) {
            return E_OUTOFMEMORY;
        }
        *clone = static_cast<ITextRangeProvider*>(copy);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Compare(ITextRangeProvider* range,
                                      BOOL* equal) noexcept override {
        if (!equal) {
            return E_POINTER;
        }
        *equal = FALSE;
        auto* other = dynamic_cast<UiaWin32TextRange*>(range);
        if (!other) {
            return E_INVALIDARG;
        }
        const auto left = resolved_bounds();
        const auto right = other->resolved_bounds();
        *equal = (left.first == right.first && left.second == right.second)
            ? TRUE
            : FALSE;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE CompareEndpoints(
        TextPatternRangeEndpoint endpoint,
        ITextRangeProvider* target_range,
        TextPatternRangeEndpoint target_endpoint,
        int* comparison) noexcept override {
        if (!comparison) {
            return E_POINTER;
        }
        *comparison = 0;
        auto* other = dynamic_cast<UiaWin32TextRange*>(target_range);
        if (!other) {
            return E_INVALIDARG;
        }
        const auto left = resolved_bounds();
        const auto right = other->resolved_bounds();
        const LONG left_endpoint =
            endpoint == TextPatternRangeEndpoint_Start ? left.first
                                                       : left.second;
        const LONG right_endpoint =
            target_endpoint == TextPatternRangeEndpoint_Start ? right.first
                                                              : right.second;
        *comparison = left_endpoint < right_endpoint
            ? -1
            : (left_endpoint > right_endpoint ? 1 : 0);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE ExpandToEnclosingUnit(TextUnit) noexcept override {
        // A whole-document provider only exposes whole-document or exact-match
        // sub-ranges; enclosing expansion is the document itself.
        start_ = 0;
        end_ = kWholeDocument;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FindAttribute(TEXTATTRIBUTEID,
                                            VARIANT,
                                            BOOL,
                                            ITextRangeProvider** range) noexcept override {
        if (!range) {
            return E_POINTER;
        }
        *range = nullptr;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE FindText(BSTR text,
                                       BOOL backward,
                                       BOOL ignore_case,
                                       ITextRangeProvider** range) noexcept override {
        if (!range) {
            return E_POINTER;
        }
        *range = nullptr;
        const auto content = current_text();
        if (!content) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (!text || text[0] == L'\0') {
            return S_OK;
        }

        const auto bounds = resolved_bounds();
        const auto range_start = static_cast<std::size_t>(bounds.first);
        const auto range_end = static_cast<std::size_t>(bounds.second);
        if (range_start >= range_end) {
            return S_OK;
        }

        std::wstring haystack = *content;
        std::wstring needle{text};
        if (ignore_case != FALSE) {
            for (wchar_t& character : haystack) {
                character = static_cast<wchar_t>(std::towlower(character));
            }
            for (wchar_t& character : needle) {
                character = static_cast<wchar_t>(std::towlower(character));
            }
        }

        std::size_t position = std::wstring::npos;
        if (backward != FALSE) {
            position = haystack.rfind(needle, range_end - 1U);
        } else {
            position = haystack.find(needle, range_start);
        }
        if (position == std::wstring::npos || position < range_start ||
            position + needle.size() > range_end) {
            return S_OK;
        }

        const LONG start = static_cast<LONG>(position);
        const LONG end = static_cast<LONG>(position + needle.size());
        auto* found =
            new (std::nothrow) UiaWin32TextRange(endpoint_, state_, start, end);
        if (!found) {
            return E_OUTOFMEMORY;
        }
        *range = static_cast<ITextRangeProvider*>(found);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetAttributeValue(TEXTATTRIBUTEID,
                                                VARIANT* value) noexcept override {
        if (!value) {
            return E_POINTER;
        }
        return set_reserved_not_supported(*value);
    }

    HRESULT STDMETHODCALLTYPE GetBoundingRectangles(
        SAFEARRAY** rectangles) noexcept override {
        if (!rectangles) {
            return E_POINTER;
        }
        *rectangles = nullptr;
        const auto read = state_.read();
        if (!read) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        const UiaRect rect = physical_screen_rect(*read);
        SAFEARRAY* array = ::SafeArrayCreateVector(VT_R8, 0, 4);
        if (!array) {
            return E_OUTOFMEMORY;
        }
        const double values[4] = {rect.left, rect.top, rect.width, rect.height};
        for (LONG index = 0; index < 4; ++index) {
            if (FAILED(::SafeArrayPutElement(array, &index, &values[index]))) {
                ::SafeArrayDestroy(array);
                return E_FAIL;
            }
        }
        *rectangles = array;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetEnclosingElement(
        IRawElementProviderSimple** element) noexcept override {
        if (!element) {
            return E_POINTER;
        }
        *element = nullptr;
        const auto endpoint = endpoint_.lock();
        if (!endpoint) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        auto provider = endpoint->provider_for(state_.identity());
        if (!provider) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        auto* win32_provider = static_cast<UiaWin32Provider*>(provider.get());
        *element = static_cast<IRawElementProviderSimple*>(win32_provider);
        win32_provider->AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetText(int max_length,
                                      BSTR* text) noexcept override {
        if (!text) {
            return E_POINTER;
        }
        *text = nullptr;
        const auto content = current_text();
        if (!content) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        const auto bounds = resolved_bounds();
        const auto begin = static_cast<std::size_t>(bounds.first);
        std::size_t count = static_cast<std::size_t>(bounds.second) - begin;
        if (max_length >= 0 &&
            count > static_cast<std::size_t>(max_length)) {
            count = static_cast<std::size_t>(max_length);
        }
        *text = ::SysAllocStringLen(content->data() + begin,
                                    static_cast<UINT>(count));
        return *text ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT STDMETHODCALLTYPE Move(TextUnit unit,
                                   int count,
                                   int* moved) noexcept override {
        if (!moved) {
            return E_POINTER;
        }
        *moved = 0;
        if (!current_text()) {
            return UIA_E_ELEMENTNOTAVAILABLE;
        }
        if (unit == TextUnit_Document || unit == TextUnit_Page) {
            start_ = count >= 0 ? 0 : kWholeDocument;
            end_ = kWholeDocument;
            *moved = count;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE MoveEndpointByUnit(TextPatternRangeEndpoint,
                                                 TextUnit,
                                                 int,
                                                 int* moved) noexcept override {
        if (!moved) {
            return E_POINTER;
        }
        *moved = 0;
        return current_text() ? S_OK : UIA_E_ELEMENTNOTAVAILABLE;
    }

    HRESULT STDMETHODCALLTYPE MoveEndpointByRange(
        TextPatternRangeEndpoint,
        ITextRangeProvider*,
        TextPatternRangeEndpoint) noexcept override {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Select() noexcept override { return S_OK; }

    HRESULT STDMETHODCALLTYPE AddToSelection() noexcept override {
        return UIA_E_INVALIDOPERATION;
    }

    HRESULT STDMETHODCALLTYPE RemoveFromSelection() noexcept override {
        return UIA_E_INVALIDOPERATION;
    }

    HRESULT STDMETHODCALLTYPE ScrollIntoView(BOOL) noexcept override {
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetChildren(
        SAFEARRAY** children) noexcept override {
        if (!children) {
            return E_POINTER;
        }
        *children = nullptr;
        return create_interface_array(children, nullptr, 0);
    }

private:
    static constexpr LONG kWholeDocument = -1;

    [[nodiscard]] std::optional<std::wstring> current_text() const noexcept {
        try {
            const auto read = state_.read();
            if (!read) {
                return std::nullopt;
            }
            if (!read->info().text_value) {
                return std::wstring{};
            }
            std::wstring wide;
            if (!utf8_to_wide(*read->info().text_value, wide)) {
                return std::wstring{};
            }
            return wide;
        } catch (...) {
            return std::nullopt;
        }
    }

    [[nodiscard]] std::pair<LONG, LONG> resolved_bounds() const noexcept {
        const auto content = current_text();
        const LONG length = content
            ? static_cast<LONG>(std::min<std::size_t>(
                  content->size(),
                  static_cast<std::size_t>(std::numeric_limits<LONG>::max())))
            : 0;
        const LONG start = std::clamp(start_, 0L, length);
        const LONG end = end_ == kWholeDocument
            ? length
            : std::clamp(end_, start, length);
        return {start, end};
    }

    std::atomic<ULONG> ref_count_{1U};
    std::weak_ptr<UiaProviderEndpoint> endpoint_;
    UiaProviderState state_;
    LONG start_{0};
    LONG end_{kWholeDocument};
};

ITextRangeProvider* UiaWin32Provider::create_document_range() noexcept {
    auto* range = new (std::nothrow)
        UiaWin32TextRange(endpoint_, state_, 0, kWholeDocument);
    return static_cast<ITextRangeProvider*>(range);
}

// ---------------------------------------------------------------------------
// Lazy provider factory
// ---------------------------------------------------------------------------

[[nodiscard]] UiaProviderHandlePtr ProviderFactory(void* user_data,
                                                   const UiaProviderState& state,
                                                   const UiaProviderRead& read) {
    auto* bridge = static_cast<NativeUIAccessibilityBridge*>(user_data);
    if (!bridge || !bridge->hwnd || !bridge->endpoint) {
        return {};
    }

    auto* provider =
        new (std::nothrow) UiaWin32Provider(bridge->endpoint, state, read,
                                            bridge->hwnd);
    if (!provider) {
        return {};
    }
    // The cache owns exactly one COM reference; the shared_ptr deleter is its
    // Release. A construction failure releases the reference immediately.
    provider->AddRef();
    try {
        return UiaProviderHandlePtr{
            static_cast<UiaProviderHandle*>(provider),
            [](UiaProviderHandle* handle) noexcept {
                static_cast<UiaWin32Provider*>(handle)->Release();
            }};
    } catch (...) {
        provider->Release();
        return {};
    }
}

// ---------------------------------------------------------------------------
// Committed-batch notification mapping
// ---------------------------------------------------------------------------

[[nodiscard]] bool deliver_events(
    NativeUIAccessibilityBridge& bridge,
    const SemanticNativePublicationBatch& batch) {
    const auto events = ui::detail::derive_uia_events(batch);

    // Only a structure change can make a cached identity disappear; consume it
    // even when no client is listening so defunct providers are not retained.
    if (events.structure) {
        (void)bridge.endpoint->apply_publication_batch(batch);
    }

    const bool recording = bridge.recorder != nullptr;
    if (!recording && !::UiaClientsAreListening()) {
        return true;
    }

    const auto notify = [&bridge](const char* name, IUnknown* element) {
        bridge.recorder(bridge.recorder_user_data, name, element);
    };

    if (events.structure) {
        if (auto root = bridge.endpoint->root()) {
            auto* provider = static_cast<UiaWin32Provider*>(root.get());
            if (recording) {
                notify(kStructureNotification,
                       static_cast<IRawElementProviderSimple*>(provider));
            } else {
                (void)::UiaRaiseStructureChangedEvent(
                    static_cast<IRawElementProviderSimple*>(provider),
                    StructureChangeType_ChildrenInvalidated, nullptr, 0);
            }
        }
    }

    if (events.focus && events.focus_target) {
        if (auto focus = bridge.endpoint->provider_for(*events.focus_target)) {
            auto* provider = static_cast<UiaWin32Provider*>(focus.get());
            if (recording) {
                notify(kFocusNotification,
                       static_cast<IRawElementProviderSimple*>(provider));
            } else {
                (void)::UiaRaiseAutomationEvent(
                    static_cast<IRawElementProviderSimple*>(provider),
                    UIA_AutomationFocusChangedEventId);
            }
        }
    }

    if (events.selection && events.selection_target) {
        if (auto selected =
                bridge.endpoint->provider_for(*events.selection_target)) {
            auto* provider = static_cast<UiaWin32Provider*>(selected.get());
            if (recording) {
                notify(kSelectionNotification,
                       static_cast<IRawElementProviderSimple*>(provider));
            } else {
                (void)::UiaRaiseAutomationEvent(
                    static_cast<IRawElementProviderSimple*>(provider),
                    UIA_SelectionItem_ElementSelectedEventId);
                VARIANT old_value;
                ::VariantInit(&old_value);
                VARIANT new_value;
                set_variant_bool(new_value, true);
                (void)::UiaRaiseAutomationPropertyChangedEvent(
                    static_cast<IRawElementProviderSimple*>(provider),
                    UIA_SelectionItemIsSelectedPropertyId, old_value,
                    new_value);
                ::VariantClear(&new_value);
            }
        }
    }

    if (events.value && events.value_target &&
        events.value_kind != UiaValueKind::None) {
        if (auto target = bridge.endpoint->provider_for(*events.value_target)) {
            auto* provider = static_cast<UiaWin32Provider*>(target.get());
            if (recording) {
                notify(kValueNotification,
                       static_cast<IRawElementProviderSimple*>(provider));
            } else {
                const auto read = bridge.endpoint->read(*events.value_target);
                if (read) {
                    PROPERTYID property_id = UIA_ValueValuePropertyId;
                    VARIANT old_value;
                    ::VariantInit(&old_value);
                    VARIANT new_value;
                    ::VariantInit(&new_value);
                    bool prepared = true;
                    switch (events.value_kind) {
                        case UiaValueKind::Text:
                            property_id = UIA_ValueValuePropertyId;
                            prepared = SUCCEEDED(set_variant_bstr_utf8(
                                new_value,
                                read->info().text_value.value_or(std::string{})));
                            break;
                        case UiaValueKind::Numeric:
                            property_id = UIA_RangeValueValuePropertyId;
                            set_variant_r8(
                                new_value,
                                read->info().numeric_value.value_or(0.0));
                            break;
                        case UiaValueKind::Toggle:
                            property_id = UIA_ToggleToggleStatePropertyId;
                            set_variant_i4(new_value,
                                           toggle_state_id(read->info()));
                            break;
                        case UiaValueKind::Expanded:
                            property_id =
                                UIA_ExpandCollapseExpandCollapseStatePropertyId;
                            set_variant_i4(
                                new_value,
                                expand_collapse_state_id(read->info()));
                            break;
                        case UiaValueKind::None:
                            prepared = false;
                            break;
                    }
                    if (prepared) {
                        (void)::UiaRaiseAutomationPropertyChangedEvent(
                            static_cast<IRawElementProviderSimple*>(provider),
                            property_id, old_value, new_value);
                    }
                    ::VariantClear(&new_value);
                }
            }
        }
    }

    if (events.bounds && events.bounds_target) {
        if (auto root = bridge.endpoint->provider_for(*events.bounds_target)) {
            auto* provider = static_cast<UiaWin32Provider*>(root.get());
            if (recording) {
                notify(kBoundsNotification,
                       static_cast<IRawElementProviderSimple*>(provider));
            } else {
                const auto read = bridge.endpoint->read(*events.bounds_target);
                if (read) {
                    VARIANT old_value;
                    ::VariantInit(&old_value);
                    VARIANT new_value;
                    if (SUCCEEDED(set_variant_rect(
                            new_value, physical_screen_rect(*read)))) {
                        (void)::UiaRaiseAutomationPropertyChangedEvent(
                            static_cast<IRawElementProviderSimple*>(provider),
                            UIA_BoundingRectanglePropertyId, old_value,
                            new_value);
                        ::VariantClear(&new_value);
                    }
                }
            }
        }
    }

    return true;
}

LRESULT CALLBACK AccessibilitySubclassProc(HWND hwnd,
                                           UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam,
                                           UINT_PTR subclass_id,
                                           DWORD_PTR ref_data) {
    (void)subclass_id;
    auto* bridge = reinterpret_cast<NativeUIAccessibilityBridge*>(ref_data);
    if (message == WM_GETOBJECT && bridge && bridge->endpoint &&
        static_cast<LONG>(lparam) == UiaRootObjectId) {
        auto root = bridge->endpoint->root();
        if (root) {
            auto* provider = static_cast<UiaWin32Provider*>(root.get());
            IRawElementProviderSimple* element =
                static_cast<IRawElementProviderSimple*>(provider);
            element->AddRef();
            const LRESULT result =
                ::UiaReturnRawElementProvider(hwnd, wparam, lparam, element);
            element->Release();
            return result;
        }
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

} // namespace

// ---------------------------------------------------------------------------
// C ABI shared with the macOS bridge
// ---------------------------------------------------------------------------

NativeUIAccessibilityBridge*
nativeuiAccessibilityCreate(void* native_view, const void* binding)
{
    if (!native_view || !binding) {
        return NULL;
    }

    const HWND hwnd = reinterpret_cast<HWND>(native_view);
    if (!::IsWindow(hwnd)) {
        return NULL;
    }
    // The window subclass stores this bridge pointer; only the window's owning
    // thread may install or remove it.
    if (::GetWindowThreadProcessId(hwnd, nullptr) != ::GetCurrentThreadId()) {
        return NULL;
    }

    const auto* attach = static_cast<
        const ui::detail::NativeAccessibilityAttachBinding*>(binding);
    if (attach->publication_source.expired()) {
        return NULL;
    }

    bool com_owned = false;
    const HRESULT com = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (com == S_OK || com == S_FALSE) {
        com_owned = true;
    } else if (com != RPC_E_CHANGED_MODE) {
        // A host-owned apartment in another mode is fine; anything else means
        // UIA cannot be used for this view.
        return NULL;
    }

    IUnknown* reserved = nullptr;
    if (FAILED(::UiaGetReservedNotSupportedValue(&reserved))) {
        if (com_owned) {
            ::CoUninitialize();
        }
        return NULL;
    }

    std::unique_ptr<NativeUIAccessibilityBridge> bridge(
        new (std::nothrow) NativeUIAccessibilityBridge());
    if (!bridge) {
        if (com_owned) {
            ::CoUninitialize();
        }
        return NULL;
    }

    try {
        bridge->endpoint = std::make_shared<UiaProviderEndpoint>(
            attach->publication_source, attach->action_endpoint);
    } catch (...) {
        bridge->endpoint.reset();
        if (com_owned) {
            ::CoUninitialize();
        }
        return NULL;
    }

    bridge->hwnd = hwnd;
    bridge->owner_thread_id = ::GetCurrentThreadId();
    bridge->com_owned = com_owned;
    bridge->uia_available = true;
    bridge->publication_source = attach->publication_source;
    bridge->endpoint->set_factory(&ProviderFactory, bridge.get());

    if (!::SetWindowSubclass(
            hwnd, &AccessibilitySubclassProc, kAccessibilitySubclassId,
            reinterpret_cast<DWORD_PTR>(bridge.get()))) {
        bridge->endpoint.reset();
        if (com_owned) {
            ::CoUninitialize();
        }
        return NULL;
    }
    bridge->subclassed = true;

    return bridge.release();
}

void
nativeuiAccessibilityDestroy(NativeUIAccessibilityBridge* bridge)
{
    if (!bridge) {
        return;
    }

    if (bridge->hwnd &&
        ::GetWindowThreadProcessId(bridge->hwnd, nullptr) !=
            ::GetCurrentThreadId()) {
        // Documented UI-thread-only contract: a wrong-thread destroy cannot
        // safely unhook the window subclass, so it fails closed instead of
        // leaving a dangling subclass reference.
        return;
    }

    if (bridge->hwnd && bridge->subclassed) {
        (void)::RemoveWindowSubclass(bridge->hwnd, &AccessibilitySubclassProc,
                                     kAccessibilitySubclassId);
        bridge->subclassed = false;
    }

    if (bridge->hwnd && bridge->uia_available) {
        // Documented teardown form: let UI Automation drop every provider/event
        // map entry associated with this window before the providers retire.
        (void)::UiaReturnRawElementProvider(bridge->hwnd, 0, 0, nullptr);
    }

    bridge->endpoint.reset();
    if (bridge->com_owned) {
        ::CoUninitialize();
        bridge->com_owned = false;
    }
    delete bridge;
}

bool
nativeuiAccessibilityDeliver(NativeUIAccessibilityBridge* bridge, const void* batch)
{
    if (!bridge || !batch) {
        return false;
    }
    if (::GetCurrentThreadId() != bridge->owner_thread_id) {
        return false;
    }

    try {
        const auto& publication_batch =
            *static_cast<const SemanticNativePublicationBatch*>(batch);
        if (!publication_batch.publication) {
            return false;
        }

        const auto source = bridge->publication_source.lock();
        if (!source) {
            return false;
        }
        const auto current = source->current();
        if (!current || current.get() != publication_batch.publication.get()) {
            // A superseded or foreign batch must never notify for this view's
            // current accessibility tree.
            return false;
        }

        return deliver_events(*bridge, publication_batch);
    } catch (...) {
        return false;
    }
}

void*
nativeuiAccessibilityTargetClass(NativeUIAccessibilityBridge* bridge)
{
    // Windows has no runtime-visible native accessibility class; the fragment
    // provider is an ordinary COM object. The macOS class-audit seam therefore
    // stays empty here.
    (void)bridge;
    return NULL;
}

void
nativeuiAccessibilitySetNotificationRecorderForTest(
    NativeUIAccessibilityBridge* bridge,
    void* user_data,
    NativeUIAccessibilityNotificationRecorder recorder)
{
    if (!bridge) {
        return;
    }
    bridge->recorder_user_data = user_data;
    bridge->recorder = recorder;
}
