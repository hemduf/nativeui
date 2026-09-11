#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <thread>

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

bool wait_until(const std::function<bool()>& done,
                std::chrono::steady_clock::duration limit) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!done() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return done();
}

struct SemanticSnapshot final {
    std::uint64_t generation{};
    std::string name;
};

} // namespace

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    LinuxDbusTransport transport;
    if (transport.start() != LinuxDbusErrorCode::None) {
        return EXIT_FAILURE;
    }

    const auto provider_client = transport.register_client();
    const auto caller_client = transport.register_client();
    if (provider_client == kInvalidLinuxDbusClientId ||
        caller_client == kInvalidLinuxDbusClientId ||
        provider_client == caller_client) {
        return EXIT_FAILURE;
    }

    DispatcherOwner ui_owner;
    DispatcherOwner completion_owner;
    const auto ui_dispatcher = ui_owner.dispatcher();
    const auto ui_thread = std::this_thread::get_id();
    const auto snapshot = std::make_shared<const SemanticSnapshot>(
        SemanticSnapshot{17, "immutable-node"});

    std::atomic<bool> read_handler_called{false};
    std::atomic<bool> read_handler_ran_on_ui{false};
    std::atomic<bool> action_handler_called{false};
    std::atomic<std::size_t> mutation_count{0};
    std::atomic<bool> mutation_ran_on_ui{false};
    std::atomic<std::uint64_t> mutation_generation{0};
    std::atomic<std::size_t> rejected_mutation_count{0};

    DispatcherOwner closed_owner;
    const auto closed_dispatcher = closed_owner.dispatcher();
    closed_owner.shutdown();

    const auto registration = transport.register_object_path(
        provider_client,
        "/org/nativeui/T072/Semantics",
        [snapshot,
         ui_dispatcher,
         closed_dispatcher,
         ui_thread,
         &read_handler_called,
         &read_handler_ran_on_ui,
         &action_handler_called,
         &mutation_count,
         &mutation_ran_on_ui,
         &mutation_generation,
         &rejected_mutation_count](const LinuxDbusMethodRequest& request) {
            if (request.member == "ReadSnapshot") {
                read_handler_ran_on_ui.store(
                    std::this_thread::get_id() == ui_thread,
                    std::memory_order_release);
                read_handler_called.store(true, std::memory_order_release);
                return LinuxDbusMethodReply::method_return({
                    LinuxDbusValue::uint64(snapshot->generation),
                    LinuxDbusValue::string(snapshot->name),
                });
            }

            if (request.member == "Activate") {
                action_handler_called.store(true, std::memory_order_release);
                const bool posted = ui_dispatcher.post(
                    [snapshot,
                     ui_thread,
                     &mutation_count,
                     &mutation_ran_on_ui,
                     &mutation_generation] {
                        mutation_ran_on_ui.store(
                            std::this_thread::get_id() == ui_thread,
                            std::memory_order_release);
                        mutation_generation.store(snapshot->generation,
                                                  std::memory_order_release);
                        mutation_count.fetch_add(1, std::memory_order_acq_rel);
                    });
                if (!posted) {
                    return LinuxDbusMethodReply::error(
                        "org.nativeui.T072.Defunct",
                        "UI dispatcher rejected semantic mutation");
                }
                return LinuxDbusMethodReply::method_return({});
            }

            if (request.member == "ActivateDefunct") {
                const bool posted = closed_dispatcher.post(
                    [&rejected_mutation_count] {
                        rejected_mutation_count.fetch_add(1,
                                                          std::memory_order_acq_rel);
                    });
                if (posted) {
                    return LinuxDbusMethodReply::error(
                        "org.nativeui.T072.ProtocolError",
                        "closed dispatcher unexpectedly accepted semantic mutation");
                }
                return LinuxDbusMethodReply::error(
                    "org.nativeui.T072.Defunct",
                    "semantic target is defunct");
            }

            return LinuxDbusMethodReply::error(
                "org.nativeui.T072.UnknownMethod",
                "unknown synthetic semantic method");
        });
    if (registration == kInvalidLinuxDbusObjectRegistrationId) {
        return EXIT_FAILURE;
    }

    bool read_done = false;
    LinuxDbusCompletion read_result;
    const auto read_id = transport.call_method(
        caller_client,
        completion_owner.dispatcher(),
        LinuxDbusMethodCall{
            transport.unique_name(),
            "/org/nativeui/T072/Semantics",
            "org.nativeui.T072.Semantics",
            "ReadSnapshot",
            2s,
        },
        [&](LinuxDbusCompletion completion) {
            read_result = std::move(completion);
            read_done = true;
        });
    if (read_id == kInvalidLinuxDbusRequestId ||
        !drain_until(completion_owner, [&] { return read_done; }, 2s) ||
        !read_handler_called.load(std::memory_order_acquire) ||
        read_handler_ran_on_ui.load(std::memory_order_acquire) ||
        read_result.code != LinuxDbusErrorCode::None ||
        read_result.values.size() != 2 ||
        read_result.values[0] != LinuxDbusValue::uint64(snapshot->generation) ||
        read_result.values[1] != LinuxDbusValue::string(snapshot->name)) {
        return EXIT_FAILURE;
    }

    bool action_done = false;
    LinuxDbusCompletion action_result;
    const auto action_id = transport.call_method(
        caller_client,
        completion_owner.dispatcher(),
        LinuxDbusMethodCall{
            transport.unique_name(),
            "/org/nativeui/T072/Semantics",
            "org.nativeui.T072.Semantics",
            "Activate",
            2s,
        },
        [&](LinuxDbusCompletion completion) {
            action_result = std::move(completion);
            action_done = true;
        });
    if (action_id == kInvalidLinuxDbusRequestId ||
        !wait_until(
            [&] { return action_handler_called.load(std::memory_order_acquire); },
            2s) ||
        mutation_count.load(std::memory_order_acquire) != 0 ||
        !drain_until(completion_owner, [&] { return action_done; }, 2s) ||
        action_result.code != LinuxDbusErrorCode::None ||
        mutation_count.load(std::memory_order_acquire) != 0) {
        return EXIT_FAILURE;
    }

    (void)ui_owner.checkpoint();
    if (mutation_count.load(std::memory_order_acquire) != 1 ||
        !mutation_ran_on_ui.load(std::memory_order_acquire) ||
        mutation_generation.load(std::memory_order_acquire) != snapshot->generation) {
        return EXIT_FAILURE;
    }

    bool defunct_done = false;
    LinuxDbusCompletion defunct_result;
    const auto defunct_id = transport.call_method(
        caller_client,
        completion_owner.dispatcher(),
        LinuxDbusMethodCall{
            transport.unique_name(),
            "/org/nativeui/T072/Semantics",
            "org.nativeui.T072.Semantics",
            "ActivateDefunct",
            2s,
        },
        [&](LinuxDbusCompletion completion) {
            defunct_result = std::move(completion);
            defunct_done = true;
        });
    if (defunct_id == kInvalidLinuxDbusRequestId ||
        !drain_until(completion_owner, [&] { return defunct_done; }, 2s) ||
        defunct_result.code != LinuxDbusErrorCode::RemoteError ||
        defunct_result.remote_error_name != "org.nativeui.T072.Defunct" ||
        rejected_mutation_count.load(std::memory_order_acquire) != 0) {
        return EXIT_FAILURE;
    }

    if (!transport.unregister_object_path(provider_client, registration)) {
        return EXIT_FAILURE;
    }
    transport.release_client(caller_client);
    transport.release_client(provider_client);
    transport.stop();
    return EXIT_SUCCESS;
}
