#include "test_support.hpp"

#import <AppKit/AppKit.h>

// CMake substitutes a consumer-specific runtime name, just like the Pugl
// classes. The fixture owns its pasteboard and never uses the general one.
@interface NativeUIDropTestDraggingInfo : NSObject
@property(nonatomic, strong) NSPasteboard* draggingPasteboard;
@property(nonatomic) NSPoint draggingLocation;
@end

@implementation NativeUIDropTestDraggingInfo
@end

namespace {

struct DropState {
    bool accept{true};
    bool accepted{};
    int offers{};
    int deliveries{};
    ui::Point position{};
    std::string type;
    std::string payload;
};

ui::Canvas target(DropState& state) {
    return ui::Canvas{240.0f, 120.0f, [](ui::CanvasContext2D&) {}}
        .on_input([&state](const ui::InputEvent& event, ui::CanvasInputContext& context) {
            if (event.type == ui::InputType::DropOffer) {
                ++state.offers;
                if (state.accept && event.offers_drop_type("text/uri-list")) {
                    state.accepted = context.accept_drop("text/uri-list");
                } else {
                    context.reject_drop();
                    state.accepted = false;
                }
                return ui::EventResult::Handled;
            }
            if (event.type == ui::InputType::DropData) {
                ++state.deliveries;
                state.position = event.position;
                state.type = event.drop_type;
                state.payload.assign(event.drop_data.begin(), event.drop_data.end());
                return ui::EventResult::Handled;
            }
            return ui::EventResult::Ignored;
        });
}

void drop(ui::NativeViewHandle handle, DropState& state, NSPasteboard* pasteboard, bool cancel = false) {
    NSView* const wrapper = (__bridge NSView*)reinterpret_cast<void*>(handle);
    NSView* const backend = wrapper.subviews.firstObject;
    NUI_CHECK(backend != nil);
    NUI_CHECK([backend.registeredDraggedTypes containsObject:NSPasteboardTypeFileURL]);
    NUI_CHECK([backend conformsToProtocol:@protocol(NSDraggingDestination)]);

    NativeUIDropTestDraggingInfo* info = [NativeUIDropTestDraggingInfo new];
    info.draggingPasteboard = pasteboard;
    info.draggingLocation = [wrapper convertPoint:NSMakePoint(64.0, 48.0) toView:nil];
    id<NSDraggingDestination> const destination = (id<NSDraggingDestination>)backend;
    id<NSDraggingInfo> const sender = (id<NSDraggingInfo>)info;
    const int deliveries = state.deliveries;
    const int offers = state.offers;

    const auto operation = [destination draggingEntered:sender];
    NUI_CHECK(state.offers == offers + 1);
    NUI_CHECK(state.deliveries == deliveries);
    NUI_CHECK(operation == (state.accept ? NSDragOperationCopy : NSDragOperationNone));
    NUI_CHECK([destination draggingUpdated:sender] == operation);
    NUI_CHECK(state.offers == offers + 2);
    NUI_CHECK(state.deliveries == deliveries);
    if (cancel) {
        [destination draggingExited:sender];
        [destination draggingEnded:sender];
        NUI_CHECK(state.deliveries == deliveries);
        NUI_CHECK(![destination prepareForDragOperation:sender]);
        return;
    }
    NUI_CHECK([destination prepareForDragOperation:sender] == state.accept);
    NUI_CHECK([destination performDragOperation:sender] == state.accept);
    NUI_CHECK(state.deliveries == deliveries + (state.accept ? 1 : 0));
    [destination concludeDragOperation:sender];
    [destination draggingEnded:sender];
    NUI_CHECK(state.deliveries == deliveries + (state.accept ? 1 : 0));
    NUI_CHECK(![destination prepareForDragOperation:sender]);

    if (state.accept) {
        NUI_CHECK(state.accepted);
        NUI_CHECK(state.type == "text/uri-list");
        NUI_CHECK_NEAR(state.position.x, 64.0f, 0.01f);
        NUI_CHECK_NEAR(state.position.y, 48.0f, 0.01f);
    }
}

void suite() {
    @autoreleasepool {
        DropState host_state;
        ui::UI host_ui{target(host_state)};
        ui::Application application;
        ui::StandaloneWindow host{application, host_ui,
            {.title = "NativeUI macOS background drop regression", .size = {320.0f, 180.0f}}};
        (void)application.poll(0.0);
        NUI_CHECK(host.last_error().empty());

        NSView* const host_wrapper = (__bridge NSView*)reinterpret_cast<void*>(host.native_handle());
        NSWindow* const window = host_wrapper.window;
        [window makeKeyWindow];
        [window resignKeyWindow];
        NUI_CHECK(!window.isKeyWindow);

        NSPasteboard* const board = [NSPasteboard pasteboardWithUniqueName];
        NSURL* const file_a = [NSURL fileURLWithPath:@"/tmp/hello.txt"];
        NSURL* const file_b = [NSURL fileURLWithPath:@"/tmp/nativeui café second.txt"];
        NUI_CHECK(([board writeObjects:@[file_a, file_b]]));

        // Use the real OpenGL destination, Pugl offer/accept/payload bridge and
        // native focus-loss notification. Direct calls deliberately model the
        // destination callbacks, not Finder's drag-session routing itself.
        drop(host.native_handle(), host_state, board);
        NUI_CHECK(host_state.payload ==
            std::string{file_a.absoluteString.UTF8String} + "\n" + file_b.absoluteString.UTF8String + "\n");
        NUI_CHECK(!window.isKeyWindow);
        host_state.accept = false;
        drop(host.native_handle(), host_state, board);
        host_state.accept = true;
        drop(host.native_handle(), host_state, board, true);
        drop(host.native_handle(), host_state, board);
        NUI_CHECK(host_state.deliveries == 2);

        DropState a_state;
        DropState b_state;
        ui::UI a_ui{target(a_state)};
        ui::UI b_ui{target(b_state)};
        auto a = std::make_unique<ui::EmbeddedView>(a_ui, host.native_handle(), ui::Size{240.0f, 120.0f});
        ui::EmbeddedView b{b_ui, host.native_handle(), {240.0f, 120.0f}};
        a_ui.activate(*a);
        a_ui.deactivate(*a);
        b_ui.activate(b);
        b_ui.deactivate(b);
        drop(a->native_handle(), a_state, board);
        NUI_CHECK(b_state.offers == 0 && b_state.deliveries == 0);
        a.reset();
        drop(b.native_handle(), b_state, board);
        NUI_CHECK(b_state.deliveries == 1 && a_state.deliveries == 1);

        for (int iteration = 0; iteration < 3; ++iteration) {
            DropState cycle_state;
            ui::UI cycle_ui{target(cycle_state)};
            ui::EmbeddedView cycle{cycle_ui, host.native_handle(), {240.0f, 120.0f}};
            drop(cycle.native_handle(), cycle_state, board);
        }
        NUI_CHECK(b_state.deliveries == 1);
        [board releaseGlobally];
    }
}

} // namespace

int main() {
    return test::run("macOS native background drop", suite);
}
