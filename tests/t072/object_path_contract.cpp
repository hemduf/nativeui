#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {

bool drain_until(ui::detail::DispatcherOwner& owner,
                 const std::function<bool()>& done,
                 std::chrono::steady_clock::duration limit) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!done() && std::chrono::steady_clock::now() < deadline) {
        (void)owner.checkpoint();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    (void)owner.checkpoint();
    return done();
}

} // namespace

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    LinuxDbusTransport transport;
    if (transport.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }

    constexpr LinuxDbusClientId client_a = 1;
    constexpr LinuxDbusClientId client_b = 2;
    const auto main_thread = std::this_thread::get_id();
    std::thread::id handler_thread;

    if (transport.register_object_path(client_a, "relative/path", [](const auto&) {
            return LinuxDbusMethodReply::method_return({});
        }) != kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }

    const auto echo_registration = transport.register_object_path(
        client_a,
        "/org/nativeui/T072/Echo",
        [&](const LinuxDbusMethodRequest& request) {
            handler_thread = std::this_thread::get_id();
            if (request.path != "/org/nativeui/T072/Echo" ||
                request.interface != "org.nativeui.T072.Test" ||
                request.member != "Echo" || request.arguments.size() != 1 ||
                request.arguments.front().kind != LinuxDbusValueKind::String) {
                return LinuxDbusMethodReply::error(
                    "org.nativeui.T072.BadRequest", "unexpected request");
            }
            return LinuxDbusMethodReply::method_return(
                {LinuxDbusValue::string(request.arguments.front().text)});
        });
    if (echo_registration == kInvalidLinuxDbusObjectRegistrationId ||
        transport.object_path_count() != 1 ||
        transport.register_object_path(client_b, "/org/nativeui/T072/Echo", [](const auto&) {
            return LinuxDbusMethodReply::method_return({});
        }) != kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }

    DispatcherOwner owner;
    LinuxDbusMethodCall echo_call{
        transport.unique_name(),
        "/org/nativeui/T072/Echo",
        "org.nativeui.T072.Test",
        "Echo",
        2s,
    };
    echo_call.arguments.push_back(LinuxDbusValue::string("hello"));

    bool echo_done = false;
    LinuxDbusCompletion echo_result;
    const auto echo_id = transport.call_method(
        client_b, owner.dispatcher(), echo_call,
        [&](LinuxDbusCompletion result) {
            echo_result = std::move(result);
            echo_done = true;
        });
    if (echo_id == kInvalidLinuxDbusRequestId ||
        !drain_until(owner, [&] { return echo_done; }, 2s) ||
        echo_result.code != LinuxDbusErrorCode::None ||
        echo_result.values.size() != 1 ||
        echo_result.values.front() != LinuxDbusValue::string("hello") ||
        handler_thread == std::thread::id{} || handler_thread == main_thread) {
        return EXIT_FAILURE;
    }

    const auto error_registration = transport.register_object_path(
        client_b,
        "/org/nativeui/T072/Error",
        [](const LinuxDbusMethodRequest&) {
            return LinuxDbusMethodReply::error(
                "org.nativeui.T072.Expected", "expected failure");
        });
    if (error_registration == kInvalidLinuxDbusObjectRegistrationId ||
        transport.object_path_count() != 2) {
        return EXIT_FAILURE;
    }

    bool error_done = false;
    LinuxDbusCompletion error_result;
    const auto error_id = transport.call_method(
        client_a,
        owner.dispatcher(),
        LinuxDbusMethodCall{
            transport.unique_name(),
            "/org/nativeui/T072/Error",
            "org.nativeui.T072.Test",
            "Fail",
            2s,
        },
        [&](LinuxDbusCompletion result) {
            error_result = std::move(result);
            error_done = true;
        });
    if (error_id == kInvalidLinuxDbusRequestId ||
        !drain_until(owner, [&] { return error_done; }, 2s) ||
        error_result.code != LinuxDbusErrorCode::RemoteError ||
        error_result.remote_error_name != "org.nativeui.T072.Expected" ||
        error_result.message != "expected failure") {
        return EXIT_FAILURE;
    }

    if (transport.unregister_object_path(client_b, echo_registration) ||
        !transport.unregister_object_path(client_a, echo_registration) ||
        transport.unregister_object_path(client_a, echo_registration) ||
        !transport.unregister_object_path(client_b, error_registration) ||
        transport.object_path_count() != 0) {
        return EXIT_FAILURE;
    }

    std::vector<LinuxDbusObjectRegistrationId> capacity_registrations;
    capacity_registrations.reserve(kLinuxDbusMaxObjectPaths);
    for (std::size_t i = 0; i < kLinuxDbusMaxObjectPaths; ++i) {
        const auto path = "/org/nativeui/T072/Capacity/Path" + std::to_string(i);
        const auto id = transport.register_object_path(
            client_a, path, [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            });
        if (id == kInvalidLinuxDbusObjectRegistrationId) {
            return EXIT_FAILURE;
        }
        capacity_registrations.push_back(id);
    }
    if (transport.object_path_count() != kLinuxDbusMaxObjectPaths ||
        transport.register_object_path(
            client_a,
            "/org/nativeui/T072/Capacity/Overflow",
            [](const LinuxDbusMethodRequest&) {
                return LinuxDbusMethodReply::method_return({});
            }) != kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }
    for (const auto id : capacity_registrations) {
        if (!transport.unregister_object_path(client_a, id)) {
            return EXIT_FAILURE;
        }
    }
    if (transport.object_path_count() != 0) {
        return EXIT_FAILURE;
    }

    transport.stop();
    return EXIT_SUCCESS;
}
