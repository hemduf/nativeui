#include "../../src/detail/native_accessibility_binding.hpp"
#include "../../src/detail/native_accessibility_bridge.h"

#import <AppKit/AppKit.h>
#import <objc/runtime.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            throw std::runtime_error("check failed: " #condition);              \
        }                                                                       \
    } while (false)

Class test_view_class(const char* runtime_name) {
    if (Class existing = objc_lookUpClass(runtime_name)) {
        return existing;
    }

    Class created = objc_allocateClassPair([NSView class], runtime_name, 0U);
    CHECK(created != Nil);
    objc_registerClassPair(created);
    return created;
}

std::shared_ptr<const ui::SemanticTreeSnapshot> root_snapshot(std::uint64_t generation) {
    auto snapshot = std::make_shared<ui::SemanticTreeSnapshot>();
    snapshot->generation = generation;
    snapshot->root = 1U;

    ui::SemanticNodeSnapshot root;
    root.id = 1U;
    root.info.role = ui::SemanticRole::Group;
    snapshot->nodes.push_back(std::move(root));
    return snapshot;
}

bool has_suffix(const std::string& value, const char* suffix) {
    const std::size_t size = std::strlen(suffix);
    return value.size() > size && value.compare(value.size() - size, size, suffix) == 0;
}

void distinct_consumer_prefixed_runtime_names_and_lookup_reuse() {
    Class const first_class = test_view_class(
        "NUI_t068_runtime_probe_first_6a6a6a6a6a6a_PuglWrapperView");
    Class const second_class = test_view_class(
        "NUI_t068_runtime_probe_second_7b7b7b7b7b7b_PuglOpenGLView");
    NSView* const first = [[first_class alloc] init];
    NSView* const second = [[second_class alloc] init];

    ui::detail::SemanticNativePublicationState first_publication;
    ui::detail::SemanticNativePublicationState second_publication;
    CHECK(first_publication
              .publish(
                  root_snapshot(1U),
                  {ui::SemanticChange::StructureChanged},
                  ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}})
              .has_value());
    CHECK(second_publication
              .publish(
                  root_snapshot(1U),
                  {ui::SemanticChange::StructureChanged},
                  ui::detail::SemanticNativeGeometry{1.0f, {0.0f, 0.0f}})
              .has_value());

    const ui::detail::NativeAccessibilityAttachBinding first_binding{
        first_publication.reader_source(), {}};
    const ui::detail::NativeAccessibilityAttachBinding second_binding{
        second_publication.reader_source(), {}};

    NativeUIAccessibilityBridge* const first_bridge =
        nativeuiAccessibilityCreate(first, &first_binding);
    NativeUIAccessibilityBridge* const second_bridge =
        nativeuiAccessibilityCreate(second, &second_binding);
    CHECK(first_bridge != nullptr);
    CHECK(second_bridge != nullptr);

    Class const first_target =
        static_cast<Class>(nativeuiAccessibilityTargetClass(first_bridge));
    Class const second_target =
        static_cast<Class>(nativeuiAccessibilityTargetClass(second_bridge));
    CHECK(first_target != nil);
    CHECK(second_target != nil);
    CHECK(first_target != second_target);
    CHECK(class_getSuperclass(first_target) == first_class);
    CHECK(class_getSuperclass(second_target) == second_class);
    CHECK(object_getClass(first) == first_target);
    CHECK(object_getClass(second) == second_target);

    const std::string first_target_name = class_getName(first_target);
    const std::string second_target_name = class_getName(second_target);
    CHECK(first_target_name != second_target_name);
    CHECK(first_target_name.rfind(class_getName(first_class), 0U) == 0U);
    CHECK(second_target_name.rfind(class_getName(second_class), 0U) == 0U);
    CHECK(has_suffix(first_target_name, "_NativeUIAccessibilityView"));
    CHECK(has_suffix(second_target_name, "_NativeUIAccessibilityView"));

    // No generic runtime-visible NativeUI class name exists, and the registered
    // consumer-scoped class is reused rather than re-allocated.
    CHECK(objc_lookUpClass("NativeUIAccessibilityView") == Nil);
    CHECK(objc_lookUpClass("NSView_NativeUIAccessibilityView") == Nil);
    CHECK(objc_lookUpClass("NativeUIAccessibilityElement") == Nil);
    CHECK(objc_lookUpClass(first_target_name.c_str()) == first_target);
    CHECK(objc_lookUpClass(second_target_name.c_str()) == second_target);

    NSView* const first_again = [[first_class alloc] init];
    NativeUIAccessibilityBridge* const reuse_bridge =
        nativeuiAccessibilityCreate(first_again, &first_binding);
    CHECK(reuse_bridge != nullptr);
    CHECK(static_cast<Class>(nativeuiAccessibilityTargetClass(reuse_bridge)) ==
          first_target);
    CHECK(object_getClass(first_again) == first_target);

    // The lazy proxy elements use the same consumer-scoped derivation from the
    // original consumer view class.
    NSArray* const first_children = [first accessibilityChildren];
    CHECK(first_children != nil && [first_children count] == 1U);
    id const first_root = [first_children objectAtIndex:0U];
    const std::string first_element_name = class_getName(object_getClass(first_root));
    const std::string first_expected_prefix =
        std::string{class_getName(first_class)} + "_NativeUIAccessibilityElement";
    CHECK(first_element_name.rfind(first_expected_prefix, 0U) == 0U);
    CHECK(objc_lookUpClass(first_element_name.c_str()) ==
          object_getClass(first_root));

    NSArray* const second_children = [second accessibilityChildren];
    CHECK(second_children != nil && [second_children count] == 1U);
    id const second_root = [second_children objectAtIndex:0U];
    const std::string second_element_name = class_getName(object_getClass(second_root));
    const std::string second_expected_prefix =
        std::string{class_getName(second_class)} + "_NativeUIAccessibilityElement";
    CHECK(second_element_name.rfind(second_expected_prefix, 0U) == 0U);
    CHECK(second_element_name != first_element_name);
    CHECK(first_root != second_root);

    nativeuiAccessibilityDestroy(reuse_bridge);
    nativeuiAccessibilityDestroy(second_bridge);
    nativeuiAccessibilityDestroy(first_bridge);
    [first_again release];
    [second release];
    [first release];
}

void target_class_accessor_is_null_for_missing_bridge() {
    CHECK(nativeuiAccessibilityTargetClass(nullptr) == nullptr);
    CHECK(nativeuiAccessibilityDeliver(nullptr, nullptr) == false);
    CHECK(nativeuiAccessibilityCreate(nullptr, nullptr) == nullptr);
}

} // namespace

int main() {
    @autoreleasepool {
        try {
            distinct_consumer_prefixed_runtime_names_and_lookup_reuse();
            target_class_accessor_is_null_for_missing_bridge();
            std::cout << "PASS macOS accessibility runtime prefix probe\n";
            return EXIT_SUCCESS;
        } catch (const std::exception& error) {
            std::cerr << "FAIL macOS accessibility runtime prefix probe: "
                      << error.what() << '\n';
            return EXIT_FAILURE;
        }
    }
}
