#include "detail/linux_dbus.hpp"
#include "detail/linux_dbus_codec.hpp"

#include <dbus/dbus.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <new>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace ui::detail {
namespace {

std::once_flag g_dbus_threads_once;

[[nodiscard]] bool is_path_element_char(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

[[nodiscard]] bool contains_nul(std::string_view text) noexcept {
    return text.find('\0') != std::string_view::npos;
}

[[nodiscard]] bool valid_method_call(const LinuxDbusMethodCall& call) noexcept {
    if (call.destination.empty() || call.path.empty() || call.interface.empty() ||
        call.member.empty() || contains_nul(call.destination) || contains_nul(call.path) ||
        contains_nul(call.interface) || contains_nul(call.member) ||
        !linux_dbus_valid_timeout(call.timeout)) {
        return false;
    }

    if (dbus_validate_bus_name(call.destination.c_str(), nullptr) == FALSE ||
        dbus_validate_path(call.path.c_str(), nullptr) == FALSE ||
        dbus_validate_interface(call.interface.c_str(), nullptr) == FALSE ||
        dbus_validate_member(call.member.c_str(), nullptr) == FALSE) {
        return false;
    }

    for (const auto& argument : call.arguments) {
        std::string signature;
        std::string error;
        if (!linux_dbus_value_signature(argument, signature, error)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_signal_match(const LinuxDbusSignalMatch& match) noexcept {
    if (contains_nul(match.sender) || contains_nul(match.path) ||
        contains_nul(match.interface) || contains_nul(match.member)) {
        return false;
    }
    if (!match.sender.empty() &&
        dbus_validate_bus_name(match.sender.c_str(), nullptr) == FALSE) {
        return false;
    }
    if (!match.path.empty() && dbus_validate_path(match.path.c_str(), nullptr) == FALSE) {
        return false;
    }
    if (!match.interface.empty() &&
        dbus_validate_interface(match.interface.c_str(), nullptr) == FALSE) {
        return false;
    }
    if (!match.member.empty() &&
        dbus_validate_member(match.member.c_str(), nullptr) == FALSE) {
        return false;
    }
    return true;
}

[[nodiscard]] std::string signal_match_rule(const LinuxDbusSignalMatch& match) {
    std::string rule{"type='signal'"};
    const auto append = [&rule](std::string_view field, const std::string& value) {
        if (!value.empty()) {
            rule += ",";
            rule += field;
            rule += "='";
            rule += value;
            rule += "'";
        }
    };
    append("sender", match.sender);
    append("path", match.path);
    append("interface", match.interface);
    append("member", match.member);
    return rule;
}

template <typename Map>
[[nodiscard]] std::uint64_t acquire_slot(Map& owners,
                                         std::uint64_t& next_id,
                                         std::size_t capacity,
                                         LinuxDbusClientId client) {
    if (client == kInvalidLinuxDbusClientId || owners.size() >= capacity) {
        return 0;
    }

    for (;;) {
        const std::uint64_t candidate = next_id;
        ++next_id;
        if (next_id == 0) {
            next_id = 1;
        }
        if (candidate == 0 || owners.contains(candidate)) {
            continue;
        }
        owners.emplace(candidate, client);
        return candidate;
    }
}

template <typename Map>
[[nodiscard]] bool release_slot(Map& owners,
                                LinuxDbusClientId client,
                                std::uint64_t id) {
    if (client == kInvalidLinuxDbusClientId || id == 0) {
        return false;
    }
    const auto found = owners.find(id);
    if (found == owners.end() || found->second != client) {
        return false;
    }
    owners.erase(found);
    return true;
}

} // namespace

bool linux_dbus_initialize_threads() noexcept {
    static const bool initialized = []() noexcept {
        bool result = false;
        std::call_once(g_dbus_threads_once, [&result] {
            result = dbus_threads_init_default() != FALSE;
        });
        return result;
    }();
    return initialized;
}

bool linux_dbus_valid_timeout(std::chrono::milliseconds timeout) noexcept {
    return timeout >= kLinuxDbusMinTimeout && timeout <= kLinuxDbusMaxTimeout;
}

bool linux_dbus_valid_object_path(std::string_view path) noexcept {
    if (path == "/") {
        return true;
    }
    if (path.size() < 2 || path.front() != '/' || path.back() == '/') {
        return false;
    }

    bool element_has_character = false;
    for (std::size_t i = 1; i < path.size(); ++i) {
        const char c = path[i];
        if (c == '/') {
            if (!element_has_character) {
                return false;
            }
            element_has_character = false;
            continue;
        }
        if (!is_path_element_char(c)) {
            return false;
        }
        element_has_character = true;
    }
    return element_has_character;
}

bool linux_dbus_library_probe() noexcept {
    if (!linux_dbus_initialize_threads()) {
        return false;
    }

    DBusError error;
    dbus_error_init(&error);
    const auto valid = dbus_validate_path("/", &error) != FALSE;
    dbus_error_free(&error);
    return valid;
}

struct LinuxDbusResourceLedger::Impl final {
    mutable std::mutex mutex;
    std::uint64_t next_request_id{1};
    std::uint64_t next_subscription_id{1};
    std::uint64_t next_object_path_id{1};
    std::unordered_map<std::uint64_t, LinuxDbusClientId> requests;
    std::unordered_map<std::uint64_t, LinuxDbusClientId> subscriptions;
    std::unordered_map<std::uint64_t, LinuxDbusClientId> object_paths;
};

LinuxDbusResourceLedger::LinuxDbusResourceLedger()
    : impl_(std::make_unique<Impl>()) {}

LinuxDbusResourceLedger::~LinuxDbusResourceLedger() = default;

LinuxDbusRequestId LinuxDbusResourceLedger::acquire_request(LinuxDbusClientId client) {
    std::lock_guard lock{impl_->mutex};
    return acquire_slot(impl_->requests, impl_->next_request_id,
                        kLinuxDbusMaxPendingCalls, client);
}

bool LinuxDbusResourceLedger::release_request(LinuxDbusClientId client,
                                               LinuxDbusRequestId id) {
    std::lock_guard lock{impl_->mutex};
    return release_slot(impl_->requests, client, id);
}

std::size_t LinuxDbusResourceLedger::pending_request_count() const noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->requests.size();
}

LinuxDbusSubscriptionId LinuxDbusResourceLedger::acquire_subscription(LinuxDbusClientId client) {
    std::lock_guard lock{impl_->mutex};
    return acquire_slot(impl_->subscriptions, impl_->next_subscription_id,
                        kLinuxDbusMaxSubscriptions, client);
}

bool LinuxDbusResourceLedger::release_subscription(LinuxDbusClientId client,
                                                    LinuxDbusSubscriptionId id) {
    std::lock_guard lock{impl_->mutex};
    return release_slot(impl_->subscriptions, client, id);
}

std::size_t LinuxDbusResourceLedger::subscription_count() const noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->subscriptions.size();
}

LinuxDbusObjectRegistrationId LinuxDbusResourceLedger::acquire_object_path(
    LinuxDbusClientId client) {
    std::lock_guard lock{impl_->mutex};
    return acquire_slot(impl_->object_paths, impl_->next_object_path_id,
                        kLinuxDbusMaxObjectPaths, client);
}

bool LinuxDbusResourceLedger::release_object_path(LinuxDbusClientId client,
                                                  LinuxDbusObjectRegistrationId id) {
    std::lock_guard lock{impl_->mutex};
    return release_slot(impl_->object_paths, client, id);
}

std::size_t LinuxDbusResourceLedger::object_path_count() const noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->object_paths.size();
}

struct LinuxDbusPendingCallSet::Impl final {
    struct Entry final {
        LinuxDbusClientId client{};
        ui::Dispatcher dispatcher;
        LinuxDbusCompletionCallback callback;
        std::shared_ptr<std::atomic<bool>> active;
    };

    explicit Impl(LinuxDbusResourceLedger& resource_ledger)
        : ledger(resource_ledger) {}

    LinuxDbusResourceLedger& ledger;
    mutable std::mutex mutex;
    std::unordered_map<LinuxDbusRequestId, std::unique_ptr<Entry>> calls;
    std::unordered_map<LinuxDbusClientId, std::shared_ptr<std::atomic<bool>>> client_gates;
    bool closing{};
};

LinuxDbusPendingCallSet::LinuxDbusPendingCallSet(LinuxDbusResourceLedger& ledger)
    : impl_(std::make_unique<Impl>(ledger)) {}

LinuxDbusPendingCallSet::~LinuxDbusPendingCallSet() {
    shutdown();
}

LinuxDbusRequestId LinuxDbusPendingCallSet::begin(LinuxDbusClientId client,
                                                   ui::Dispatcher dispatcher,
                                                   std::chrono::milliseconds timeout,
                                                   LinuxDbusCompletionCallback callback) {
    if (client == kInvalidLinuxDbusClientId || !dispatcher.valid() ||
        !linux_dbus_valid_timeout(timeout) || !callback) {
        return kInvalidLinuxDbusRequestId;
    }

    LinuxDbusRequestId id = kInvalidLinuxDbusRequestId;
    try {
        id = impl_->ledger.acquire_request(client);
        if (id == kInvalidLinuxDbusRequestId) {
            return id;
        }

        auto entry = std::make_unique<Impl::Entry>();
        auto new_gate = std::make_shared<std::atomic<bool>>(true);
        entry->client = client;
        entry->dispatcher = std::move(dispatcher);
        entry->callback = std::move(callback);

        bool inserted = false;
        {
            std::lock_guard lock{impl_->mutex};
            if (!impl_->closing) {
                auto [gate_it, gate_inserted] =
                    impl_->client_gates.try_emplace(client, std::move(new_gate));
                (void)gate_inserted;
                if (gate_it->second && gate_it->second->load(std::memory_order_acquire)) {
                    entry->active = gate_it->second;
                    auto [it, did_insert] = impl_->calls.try_emplace(id);
                    if (did_insert) {
                        it->second = std::move(entry);
                        inserted = true;
                    }
                }
            }
        }

        if (!inserted) {
            (void)impl_->ledger.release_request(client, id);
            return kInvalidLinuxDbusRequestId;
        }
        return id;
    } catch (...) {
        if (id != kInvalidLinuxDbusRequestId) {
            (void)impl_->ledger.release_request(client, id);
        }
        return kInvalidLinuxDbusRequestId;
    }
}

bool LinuxDbusPendingCallSet::complete(LinuxDbusClientId client,
                                       LinuxDbusRequestId id,
                                       LinuxDbusCompletion completion) {
    std::unique_ptr<Impl::Entry> entry;
    {
        std::lock_guard lock{impl_->mutex};
        if (impl_->closing || client == kInvalidLinuxDbusClientId ||
            id == kInvalidLinuxDbusRequestId) {
            return false;
        }
        const auto found = impl_->calls.find(id);
        if (found == impl_->calls.end() || !found->second ||
            found->second->client != client) {
            return false;
        }
        entry = std::move(found->second);
        impl_->calls.erase(found);
    }

    (void)impl_->ledger.release_request(client, id);

    auto dispatcher = std::move(entry->dispatcher);
    auto callback = std::move(entry->callback);
    auto active = std::move(entry->active);
    entry.reset();

    try {
        (void)dispatcher.post(
            [active = std::move(active), callback = std::move(callback),
             completion = std::move(completion)]() mutable {
                if (!active || !active->load(std::memory_order_acquire)) {
                    return;
                }
                callback(std::move(completion));
            });
    } catch (...) {
        // A completion is terminal even when the UI owner is gone or its
        // bounded queue cannot accept the callback. Never execute on the I/O
        // thread as a fallback.
    }
    return true;
}

bool LinuxDbusPendingCallSet::cancel(LinuxDbusClientId client, LinuxDbusRequestId id) {
    return complete(client, id,
                    LinuxDbusCompletion{LinuxDbusErrorCode::Cancelled, {}, {}});
}

void LinuxDbusPendingCallSet::discard_client(LinuxDbusClientId client) noexcept {
    if (client == kInvalidLinuxDbusClientId) {
        return;
    }

    try {
        {
            std::lock_guard lock{impl_->mutex};
            const auto gate = impl_->client_gates.find(client);
            if (gate != impl_->client_gates.end()) {
                if (gate->second) {
                    gate->second->store(false, std::memory_order_release);
                }
                impl_->client_gates.erase(gate);
            }
        }

        for (;;) {
            LinuxDbusRequestId id = kInvalidLinuxDbusRequestId;
            std::unique_ptr<Impl::Entry> discarded;
            {
                std::lock_guard lock{impl_->mutex};
                for (auto it = impl_->calls.begin(); it != impl_->calls.end(); ++it) {
                    if (it->second && it->second->client == client) {
                        id = it->first;
                        discarded = std::move(it->second);
                        impl_->calls.erase(it);
                        break;
                    }
                }
            }
            if (!discarded) {
                break;
            }
            (void)impl_->ledger.release_request(client, id);
        }
    } catch (...) {
        // Teardown is noexcept. Any already-posted callback remains guarded by
        // the per-client gate, which is disabled before pending entries drain.
    }
}

void LinuxDbusPendingCallSet::shutdown() noexcept {
    std::unordered_map<LinuxDbusRequestId, std::unique_ptr<Impl::Entry>> discarded;
    try {
        {
            std::lock_guard lock{impl_->mutex};
            if (impl_->closing) {
                return;
            }
            impl_->closing = true;
            for (auto& [client, gate] : impl_->client_gates) {
                (void)client;
                if (gate) {
                    gate->store(false, std::memory_order_release);
                }
            }
            impl_->client_gates.clear();
            discarded.swap(impl_->calls);
        }

        for (const auto& [id, entry] : discarded) {
            if (entry) {
                (void)impl_->ledger.release_request(entry->client, id);
            }
        }
    } catch (...) {
        // Destruction must not throw. Pending callbacks remain suppressed.
    }
}

std::size_t LinuxDbusPendingCallSet::pending_count() const noexcept {
    std::lock_guard lock{impl_->mutex};
    return impl_->calls.size();
}

struct LinuxDbusTransport::Impl final {
    struct NativePendingCall final {
        LinuxDbusClientId client{};
        DBusPendingCall* pending{};
        std::chrono::steady_clock::time_point deadline{};
    };

    struct NotifyContext final {
        Impl* owner{};
        LinuxDbusClientId client{};
        LinuxDbusRequestId id{};
    };

    struct SignalEntry final {
        LinuxDbusClientId client{};
        ui::Dispatcher dispatcher;
        LinuxDbusSignalMatch match;
        LinuxDbusSignalCallback callback;
        std::string rule;
        std::shared_ptr<std::atomic<bool>> active{std::make_shared<std::atomic<bool>>(false)};
    };

    struct SignalSetupCommand final {
        std::shared_ptr<SignalEntry> entry;
        std::mutex mutex;
        std::condition_variable ready;
        bool done{};
        bool success{};
        bool cancelled{};
    };

    struct ObjectPathEntry final {
        LinuxDbusClientId client{};
        std::string path;
        LinuxDbusObjectPathHandler handler;
        mutable std::mutex mutex;
        std::condition_variable idle;
        bool active{true};
        std::size_t in_flight{};
    };

    struct ObjectPathContext final {
        Impl* owner{};
        LinuxDbusObjectRegistrationId id{};
    };

    struct ObjectInvocationGuard final {
        std::shared_ptr<ObjectPathEntry> entry;

        ~ObjectInvocationGuard() {
            if (!entry) {
                return;
            }
            std::lock_guard lock{entry->mutex};
            if (entry->in_flight > 0) {
                --entry->in_flight;
            }
            if (entry->in_flight == 0) {
                entry->idle.notify_all();
            }
        }
    };

    Impl()
        : calls(ledger) {}

    [[nodiscard]] std::optional<NativePendingCall> take_pending(
        LinuxDbusClientId client,
        LinuxDbusRequestId id,
        DBusPendingCall* expected = nullptr) {
        std::lock_guard lock{pending_mutex};
        const auto found = pending_calls.find(id);
        if (found == pending_calls.end() || found->second.client != client ||
            (expected != nullptr && found->second.pending != expected)) {
            return std::nullopt;
        }
        NativePendingCall result = found->second;
        pending_calls.erase(found);
        return result;
    }

    [[nodiscard]] static LinuxDbusCompletion classify_reply(DBusMessage* reply) {
        if (reply == nullptr) {
            return LinuxDbusCompletion{LinuxDbusErrorCode::LocalProtocolError, {},
                                       "D-Bus pending call completed without a reply"};
        }

        const int type = dbus_message_get_type(reply);
        if (type == DBUS_MESSAGE_TYPE_METHOD_RETURN) {
            LinuxDbusCompletion completion;
            std::string decode_error;
            if (!linux_dbus_decode_values(reply, completion.values, decode_error)) {
                completion.code = LinuxDbusErrorCode::LocalProtocolError;
                completion.message = decode_error.empty()
                    ? "Unable to decode D-Bus method-return values"
                    : std::move(decode_error);
            }
            return completion;
        }
        if (type != DBUS_MESSAGE_TYPE_ERROR) {
            return LinuxDbusCompletion{LinuxDbusErrorCode::LocalProtocolError, {},
                                       "D-Bus method call produced an unexpected message type"};
        }

        LinuxDbusCompletion completion;
        const char* error_name = dbus_message_get_error_name(reply);
        if (error_name != nullptr) {
            completion.remote_error_name = error_name;
        }

        if (completion.remote_error_name == DBUS_ERROR_NO_REPLY) {
            completion.code = LinuxDbusErrorCode::Timeout;
        } else if (completion.remote_error_name == DBUS_ERROR_DISCONNECTED) {
            completion.code = LinuxDbusErrorCode::Disconnected;
        } else {
            completion.code = LinuxDbusErrorCode::RemoteError;
        }

        DBusError error;
        dbus_error_init(&error);
        const char* message = nullptr;
        if (dbus_message_get_args(reply, &error,
                                  DBUS_TYPE_STRING, &message,
                                  DBUS_TYPE_INVALID) != FALSE &&
            message != nullptr) {
            completion.message = message;
        }
        dbus_error_free(&error);
        return completion;
    }

    static void free_notify_context(void* data) {
        delete static_cast<NotifyContext*>(data);
    }

    static void pending_notify(DBusPendingCall* pending, void* data) {
        auto* context = static_cast<NotifyContext*>(data);
        if (context == nullptr || context->owner == nullptr) {
            return;
        }

        Impl* owner = context->owner;
        const LinuxDbusClientId client = context->client;
        const LinuxDbusRequestId id = context->id;
        auto native = owner->take_pending(client, id, pending);
        if (!native) {
            return;
        }

        DBusMessage* reply = dbus_pending_call_steal_reply(pending);
        LinuxDbusCompletion completion = classify_reply(reply);
        if (reply != nullptr) {
            dbus_message_unref(reply);
        }

        (void)owner->calls.complete(client, id, std::move(completion));
        dbus_pending_call_unref(native->pending);
    }

    [[nodiscard]] int io_wait_timeout_ms() const noexcept {
        constexpr int idle_wait_ms = 100;
        int wait_ms = idle_wait_ms;
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock{pending_mutex};
        for (const auto& [id, native] : pending_calls) {
            (void)id;
            if (native.deadline <= now) {
                return 0;
            }
            const auto remaining = native.deadline - now;
            auto rounded = std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
            if (rounded < remaining) {
                rounded += std::chrono::milliseconds{1};
            }
            const auto count = rounded.count();
            if (count > 0 && count < wait_ms) {
                wait_ms = static_cast<int>(count);
            }
        }
        return wait_ms;
    }

    void wake_io() noexcept {
        io_wake_generation.fetch_add(1, std::memory_order_release);
        io_wakeup.notify_one();
    }

    void expire_pending_calls() noexcept {
        for (;;) {
            LinuxDbusRequestId id = kInvalidLinuxDbusRequestId;
            NativePendingCall expired;
            bool found = false;
            const auto now = std::chrono::steady_clock::now();
            {
                std::lock_guard lock{pending_mutex};
                for (auto it = pending_calls.begin(); it != pending_calls.end(); ++it) {
                    if (it->second.deadline <= now) {
                        id = it->first;
                        expired = it->second;
                        pending_calls.erase(it);
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                return;
            }
            if (expired.pending != nullptr) {
                dbus_pending_call_cancel(expired.pending);
                dbus_pending_call_unref(expired.pending);
            }
            try {
                (void)calls.complete(
                    expired.client, id,
                    LinuxDbusCompletion{LinuxDbusErrorCode::Timeout, DBUS_ERROR_NO_REPLY,
                                        "D-Bus method call timed out"});
            } catch (...) {
                // Timeout completion is already terminal; no I/O-thread
                // callback fallback is permitted if marshalling fails.
            }
        }
    }

    [[nodiscard]] static bool install_signal_match(DBusConnection* current_connection,
                                                   const std::string& rule) noexcept {
        if (current_connection == nullptr) {
            return false;
        }

        DBusMessage* message = dbus_message_new_method_call(
            DBUS_SERVICE_DBUS, DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS, "AddMatch");
        if (message == nullptr) {
            return false;
        }
        const char* rule_text = rule.c_str();
        if (dbus_message_append_args(message, DBUS_TYPE_STRING, &rule_text,
                                     DBUS_TYPE_INVALID) == FALSE) {
            dbus_message_unref(message);
            return false;
        }

        DBusError error;
        dbus_error_init(&error);
        DBusMessage* reply = dbus_connection_send_with_reply_and_block(
            current_connection, message, 1'000, &error);
        dbus_message_unref(message);
        const bool success = reply != nullptr && !dbus_error_is_set(&error) &&
                             dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN;
        if (reply != nullptr) {
            dbus_message_unref(reply);
        }
        dbus_error_free(&error);
        return success;
    }

    static void remove_signal_match_async(DBusConnection* current_connection,
                                          const std::string& rule) noexcept {
        if (current_connection != nullptr) {
            dbus_bus_remove_match(current_connection, rule.c_str(), nullptr);
        }
    }

    void process_signal_setup_commands() noexcept {
        std::vector<std::shared_ptr<SignalSetupCommand>> commands;
        try {
            {
                std::lock_guard lock{control_mutex};
                commands.swap(signal_setup_commands);
            }
            for (const auto& command : commands) {
                if (!command || !command->entry) {
                    continue;
                }

                bool cancelled = false;
                {
                    std::lock_guard command_lock{command->mutex};
                    cancelled = command->cancelled;
                }

                bool success = false;
                if (!cancelled && !stop_requested.load(std::memory_order_acquire)) {
                    success = install_signal_match(connection, command->entry->rule);
                }

                {
                    std::lock_guard command_lock{command->mutex};
                    cancelled = command->cancelled;
                    if (success && !cancelled) {
                        command->entry->active->store(true, std::memory_order_release);
                        command->success = true;
                    } else {
                        command->success = false;
                    }
                    command->done = true;
                }
                if (success && cancelled) {
                    remove_signal_match_async(connection, command->entry->rule);
                }
                command->ready.notify_all();
            }
        } catch (...) {
            for (const auto& command : commands) {
                if (!command) {
                    continue;
                }
                {
                    std::lock_guard command_lock{command->mutex};
                    command->success = false;
                    command->done = true;
                }
                command->ready.notify_all();
            }
        }
    }

    void fail_signal_setup_commands() noexcept {
        std::vector<std::shared_ptr<SignalSetupCommand>> commands;
        try {
            {
                std::lock_guard lock{control_mutex};
                commands.swap(signal_setup_commands);
            }
            for (const auto& command : commands) {
                if (!command) {
                    continue;
                }
                {
                    std::lock_guard command_lock{command->mutex};
                    command->success = false;
                    command->done = true;
                }
                command->ready.notify_all();
            }
        } catch (...) {
            // Callers also use a bounded wait, so shutdown cannot depend on
            // allocating or draining this diagnostic control path.
        }
    }

    [[nodiscard]] static std::string copy_message_string(const char* value) {
        return value == nullptr ? std::string{} : std::string{value};
    }

    [[nodiscard]] static bool signal_matches(const LinuxDbusSignalMatch& match,
                                             DBusMessage* message) noexcept {
        if (!match.sender.empty() &&
            dbus_message_has_sender(message, match.sender.c_str()) == FALSE) {
            return false;
        }
        if (!match.path.empty() &&
            dbus_message_has_path(message, match.path.c_str()) == FALSE) {
            return false;
        }
        if (!match.interface.empty() &&
            dbus_message_has_interface(message, match.interface.c_str()) == FALSE) {
            return false;
        }
        if (!match.member.empty() &&
            dbus_message_has_member(message, match.member.c_str()) == FALSE) {
            return false;
        }
        return true;
    }

    static DBusHandlerResult signal_filter(DBusConnection*, DBusMessage* message, void* data) {
        if (message == nullptr || data == nullptr ||
            dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        auto* owner = static_cast<Impl*>(data);
        std::vector<std::shared_ptr<SignalEntry>> targets;
        try {
            std::lock_guard lock{owner->signal_mutex};
            targets.reserve(owner->signals.size());
            for (const auto& [id, entry] : owner->signals) {
                (void)id;
                if (entry && entry->active->load(std::memory_order_acquire) &&
                    signal_matches(entry->match, message)) {
                    targets.push_back(entry);
                }
            }
        } catch (...) {
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }

        if (targets.empty()) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        LinuxDbusSignal signal;
        signal.sender = copy_message_string(dbus_message_get_sender(message));
        signal.path = copy_message_string(dbus_message_get_path(message));
        signal.interface = copy_message_string(dbus_message_get_interface(message));
        signal.member = copy_message_string(dbus_message_get_member(message));
        std::string decode_error;
        if (!linux_dbus_decode_values(message, signal.arguments, decode_error)) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        for (const auto& entry : targets) {
            try {
                auto dispatcher = entry->dispatcher;
                auto callback = entry->callback;
                auto active = entry->active;
                LinuxDbusSignal delivered = signal;
                (void)dispatcher.post(
                    [active = std::move(active), callback = std::move(callback),
                     delivered = std::move(delivered)]() mutable {
                        if (!active->load(std::memory_order_acquire)) {
                            return;
                        }
                        callback(std::move(delivered));
                    });
            } catch (...) {
                // Dispatcher rejection/allocation failure drops this delivery;
                // never run client callbacks on the D-Bus I/O thread.
            }
        }
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    void unregister_all_signals(DBusConnection* current_connection) noexcept {
        std::unordered_map<LinuxDbusSubscriptionId, std::shared_ptr<SignalEntry>> removed;
        bool remove_filter = false;
        try {
            {
                std::lock_guard lock{signal_mutex};
                for (auto& [id, entry] : signals) {
                    (void)id;
                    if (entry) {
                        entry->active->store(false, std::memory_order_release);
                    }
                }
                removed.swap(signals);
                remove_filter = signal_filter_installed;
                signal_filter_installed = false;
            }

            if (current_connection != nullptr && remove_filter) {
                dbus_connection_remove_filter(current_connection, &Impl::signal_filter, this);
            }
            for (const auto& [id, entry] : removed) {
                if (entry) {
                    (void)ledger.release_subscription(entry->client, id);
                }
            }
        } catch (...) {
            // The private connection is closed immediately after I/O shutdown,
            // so no blocking RemoveMatch round-trip is needed during teardown.
        }
    }

    [[nodiscard]] std::shared_ptr<ObjectPathEntry> begin_object_invocation(
        LinuxDbusObjectRegistrationId id) {
        std::shared_ptr<ObjectPathEntry> entry;
        {
            std::lock_guard lock{object_mutex};
            const auto found = object_paths.find(id);
            if (found == object_paths.end()) {
                return {};
            }
            entry = found->second;
        }
        {
            std::lock_guard lock{entry->mutex};
            if (!entry->active) {
                return {};
            }
            ++entry->in_flight;
        }
        return entry;
    }

    [[nodiscard]] static DBusMessage* create_object_reply(
        DBusMessage* request,
        LinuxDbusMethodReply reply) {
        if (reply.is_error) {
            if (reply.error_name.empty() || contains_nul(reply.error_name) ||
                contains_nul(reply.error_message) ||
                dbus_validate_error_name(reply.error_name.c_str(), nullptr) == FALSE) {
                return dbus_message_new_error(
                    request, "org.nativeui.DBus.LocalProtocolError",
                    "Object-path handler returned an invalid D-Bus error");
            }
            return dbus_message_new_error(request, reply.error_name.c_str(),
                                          reply.error_message.c_str());
        }

        DBusMessage* response = dbus_message_new_method_return(request);
        if (response == nullptr) {
            return nullptr;
        }
        std::string encode_error;
        if (!linux_dbus_append_values(response, reply.values, encode_error)) {
            dbus_message_unref(response);
            const char* diagnostic = encode_error.empty()
                ? "Object-path handler returned invalid values"
                : encode_error.c_str();
            return dbus_message_new_error(
                request, "org.nativeui.DBus.LocalProtocolError", diagnostic);
        }
        return response;
    }

    [[nodiscard]] DBusHandlerResult handle_object_message(
        LinuxDbusObjectRegistrationId id,
        DBusConnection* callback_connection,
        DBusMessage* message) {
        if (callback_connection == nullptr || message == nullptr ||
            dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        auto entry = begin_object_invocation(id);
        if (!entry) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        ObjectInvocationGuard invocation{entry};

        LinuxDbusMethodRequest request;
        request.sender = copy_message_string(dbus_message_get_sender(message));
        request.path = copy_message_string(dbus_message_get_path(message));
        request.interface = copy_message_string(dbus_message_get_interface(message));
        request.member = copy_message_string(dbus_message_get_member(message));

        std::string decode_error;
        LinuxDbusMethodReply reply;
        if (!linux_dbus_decode_values(message, request.arguments, decode_error)) {
            reply = LinuxDbusMethodReply::error(
                "org.nativeui.DBus.LocalProtocolError",
                decode_error.empty() ? "Unable to decode D-Bus method arguments"
                                     : std::move(decode_error));
        } else {
            try {
                reply = entry->handler(request);
            } catch (...) {
                reply = LinuxDbusMethodReply::error(
                    "org.nativeui.DBus.HandlerFailed",
                    "Object-path handler threw an exception");
            }
        }

        DBusMessage* response = create_object_reply(message, std::move(reply));
        if (response == nullptr) {
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
        const dbus_bool_t sent = dbus_connection_send(callback_connection, response, nullptr);
        dbus_message_unref(response);
        return sent != FALSE ? DBUS_HANDLER_RESULT_HANDLED
                             : DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    static void object_path_unregistered(DBusConnection*, void* data) {
        delete static_cast<ObjectPathContext*>(data);
    }

    static DBusHandlerResult object_path_message(DBusConnection* callback_connection,
                                                 DBusMessage* message,
                                                 void* data) {
        auto* context = static_cast<ObjectPathContext*>(data);
        if (context == nullptr || context->owner == nullptr) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }
        Impl* owner = context->owner;
        const LinuxDbusObjectRegistrationId id = context->id;
        try {
            return owner->handle_object_message(id, callback_connection, message);
        } catch (...) {
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
    }

    [[nodiscard]] static const DBusObjectPathVTable& object_path_vtable() noexcept {
        static const DBusObjectPathVTable vtable{
            &Impl::object_path_unregistered,
            &Impl::object_path_message,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
        };
        return vtable;
    }

    void unregister_all_object_paths(DBusConnection* current_connection) noexcept {
        for (;;) {
            LinuxDbusObjectRegistrationId id = kInvalidLinuxDbusObjectRegistrationId;
            std::shared_ptr<ObjectPathEntry> entry;
            {
                std::lock_guard lock{object_mutex};
                const auto found = object_paths.begin();
                if (found == object_paths.end()) {
                    object_path_ids.clear();
                    break;
                }
                id = found->first;
                entry = found->second;
                {
                    std::lock_guard entry_lock{entry->mutex};
                    entry->active = false;
                }
                object_path_ids.erase(entry->path);
                object_paths.erase(found);
            }
            if (current_connection != nullptr) {
                (void)dbus_connection_unregister_object_path(
                    current_connection, entry->path.c_str());
            }
            (void)ledger.release_object_path(entry->client, id);
        }
    }

    mutable std::mutex lifecycle_mutex;
    std::condition_variable lifecycle_idle;
    std::condition_variable io_wakeup;
    mutable std::mutex pending_mutex;
    mutable std::mutex signal_mutex;
    mutable std::mutex object_mutex;
    mutable std::mutex control_mutex;
    DBusConnection* connection{};
    std::thread io_thread;
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> running{false};
    std::atomic<std::uint64_t> io_wake_generation{0};
    std::string unique_name;
    bool destroying{};
    bool stopping{};
    bool signal_filter_installed{};
    std::uint64_t next_client_id{1};
    std::unordered_set<LinuxDbusClientId> clients;

    LinuxDbusResourceLedger ledger;
    LinuxDbusPendingCallSet calls;
    std::unordered_map<LinuxDbusRequestId, NativePendingCall> pending_calls;
    std::unordered_map<LinuxDbusSubscriptionId, std::shared_ptr<SignalEntry>> signals;
    std::vector<std::shared_ptr<SignalSetupCommand>> signal_setup_commands;
    std::unordered_map<LinuxDbusObjectRegistrationId,
                       std::shared_ptr<ObjectPathEntry>> object_paths;
    std::unordered_map<std::string, LinuxDbusObjectRegistrationId> object_path_ids;
};

LinuxDbusTransport::LinuxDbusTransport()
    : impl_(std::make_unique<Impl>()) {}

LinuxDbusTransport::~LinuxDbusTransport() {
    {
        std::lock_guard lock{impl_->lifecycle_mutex};
        impl_->destroying = true;
    }
    stop();
    impl_->calls.shutdown();
}

LinuxDbusErrorCode LinuxDbusTransport::start() {
    std::lock_guard lock{impl_->lifecycle_mutex};
    if (impl_->destroying || impl_->stopping) {
        return LinuxDbusErrorCode::Shutdown;
    }
    if (impl_->running.load(std::memory_order_acquire)) {
        return LinuxDbusErrorCode::None;
    }
    if (impl_->connection != nullptr || impl_->io_thread.joinable()) {
        // A previous I/O loop has stopped but has not yet been finalized by
        // stop(). Never overwrite its connection/thread state.
        return LinuxDbusErrorCode::Shutdown;
    }

    if (!linux_dbus_initialize_threads()) {
        return LinuxDbusErrorCode::InitializationFailed;
    }

    DBusError error;
    dbus_error_init(&error);
    DBusConnection* connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (connection == nullptr) {
        dbus_error_free(&error);
        return LinuxDbusErrorCode::BusUnavailable;
    }
    dbus_error_free(&error);

    dbus_connection_set_exit_on_disconnect(connection, FALSE);
    const char* unique_name = dbus_bus_get_unique_name(connection);
    if (unique_name == nullptr || *unique_name == '\0') {
        dbus_connection_close(connection);
        dbus_connection_unref(connection);
        return LinuxDbusErrorCode::LocalProtocolError;
    }

    impl_->connection = connection;
    impl_->unique_name = unique_name;
    impl_->stop_requested.store(false, std::memory_order_release);
    impl_->running.store(true, std::memory_order_release);

    try {
        Impl* state = impl_.get();
        impl_->io_thread = std::thread([state] {
            while (!state->stop_requested.load(std::memory_order_acquire)) {
                const auto wake_generation =
                    state->io_wake_generation.load(std::memory_order_acquire);
                const int wait_ms = state->io_wait_timeout_ms();
                {
                    std::unique_lock control_lock{state->control_mutex};
                    state->io_wakeup.wait_for(
                        control_lock, std::chrono::milliseconds{wait_ms}, [&] {
                            return state->stop_requested.load(std::memory_order_acquire) ||
                                   state->io_wake_generation.load(std::memory_order_acquire) !=
                                       wake_generation ||
                                   !state->signal_setup_commands.empty();
                        });
                }
                if (state->stop_requested.load(std::memory_order_acquire)) {
                    break;
                }
                state->process_signal_setup_commands();
                state->expire_pending_calls();
                if (dbus_connection_read_write_dispatch(state->connection, 0) == FALSE) {
                    break;
                }
                state->expire_pending_calls();
            }
            state->fail_signal_setup_commands();
            state->running.store(false, std::memory_order_release);
        });
    } catch (...) {
        impl_->running.store(false, std::memory_order_release);
        impl_->connection = nullptr;
        impl_->unique_name.clear();
        dbus_connection_close(connection);
        dbus_connection_unref(connection);
        return LinuxDbusErrorCode::InitializationFailed;
    }

    return LinuxDbusErrorCode::None;
}

void LinuxDbusTransport::stop() noexcept {
    bool suppress_completions = false;
    DBusConnection* connection = nullptr;
    std::thread io_thread;

    {
        std::unique_lock lock{impl_->lifecycle_mutex};
        if (impl_->stopping) {
            if (impl_->io_thread.get_id() == std::this_thread::get_id()) {
                impl_->stop_requested.store(true, std::memory_order_release);
                impl_->wake_io();
                return;
            }
            impl_->lifecycle_idle.wait(lock, [&] { return !impl_->stopping; });
            return;
        }

        impl_->stop_requested.store(true, std::memory_order_release);
        suppress_completions = impl_->destroying;

        if (impl_->io_thread.joinable() &&
            impl_->io_thread.get_id() == std::this_thread::get_id()) {
            impl_->wake_io();
            return;
        }

        if (impl_->connection == nullptr && !impl_->io_thread.joinable()) {
            impl_->running.store(false, std::memory_order_release);
            impl_->unique_name.clear();
            impl_->clients.clear();
            impl_->fail_signal_setup_commands();
            return;
        }

        impl_->stopping = true;
        connection = impl_->connection;
        if (impl_->io_thread.joinable()) {
            io_thread = std::move(impl_->io_thread);
        }
    }

    impl_->wake_io();
    if (io_thread.joinable()) {
        io_thread.join();
    }

    impl_->unregister_all_signals(connection);
    impl_->unregister_all_object_paths(connection);

    {
        std::lock_guard lock{impl_->lifecycle_mutex};
        if (impl_->connection == connection && connection != nullptr) {
            dbus_connection_close(connection);
            dbus_connection_unref(connection);
            impl_->connection = nullptr;
        }
        impl_->running.store(false, std::memory_order_release);
        impl_->unique_name.clear();
        impl_->clients.clear();
        impl_->stopping = false;
    }
    impl_->lifecycle_idle.notify_all();

    std::unordered_map<LinuxDbusRequestId, Impl::NativePendingCall> pending;
    {
        std::lock_guard lock{impl_->pending_mutex};
        pending.swap(impl_->pending_calls);
    }

    for (const auto& [id, native] : pending) {
        if (native.pending != nullptr) {
            dbus_pending_call_cancel(native.pending);
            dbus_pending_call_unref(native.pending);
        }
        if (!suppress_completions) {
            (void)impl_->calls.complete(
                native.client, id,
                LinuxDbusCompletion{LinuxDbusErrorCode::Shutdown, {}, {}});
        }
    }

    if (suppress_completions) {
        impl_->calls.shutdown();
    }
}

bool LinuxDbusTransport::running() const noexcept {
    return impl_->running.load(std::memory_order_acquire);
}

std::string LinuxDbusTransport::unique_name() const {
    std::lock_guard lock{impl_->lifecycle_mutex};
    return impl_->unique_name;
}

LinuxDbusClientId LinuxDbusTransport::register_client() {
    std::lock_guard lock{impl_->lifecycle_mutex};
    if (impl_->destroying || impl_->stopping || impl_->connection == nullptr ||
        !impl_->running.load(std::memory_order_acquire) ||
        impl_->stop_requested.load(std::memory_order_acquire)) {
        return kInvalidLinuxDbusClientId;
    }

    try {
        for (;;) {
            const LinuxDbusClientId candidate = impl_->next_client_id;
            ++impl_->next_client_id;
            if (impl_->next_client_id == kInvalidLinuxDbusClientId) {
                impl_->next_client_id = 1;
            }
            if (candidate == kInvalidLinuxDbusClientId || impl_->clients.contains(candidate)) {
                continue;
            }
            impl_->clients.emplace(candidate);
            return candidate;
        }
    } catch (...) {
        return kInvalidLinuxDbusClientId;
    }
}

std::size_t LinuxDbusTransport::client_count() const noexcept {
    std::lock_guard lock{impl_->lifecycle_mutex};
    return impl_->clients.size();
}

LinuxDbusRequestId LinuxDbusTransport::call_method(
    LinuxDbusClientId client,
    ui::Dispatcher dispatcher,
    const LinuxDbusMethodCall& call,
    LinuxDbusCompletionCallback callback) {
    if (client == kInvalidLinuxDbusClientId || !dispatcher.valid() || !callback ||
        !valid_method_call(call)) {
        return kInvalidLinuxDbusRequestId;
    }

    LinuxDbusRequestId id = kInvalidLinuxDbusRequestId;
    {
        std::lock_guard lock{impl_->lifecycle_mutex};
        if (impl_->destroying || impl_->stopping || impl_->connection == nullptr ||
            !impl_->running.load(std::memory_order_acquire) ||
            impl_->stop_requested.load(std::memory_order_acquire) ||
            !impl_->clients.contains(client)) {
            return kInvalidLinuxDbusRequestId;
        }
        id = impl_->calls.begin(client, dispatcher, call.timeout, std::move(callback));
    }
    if (id == kInvalidLinuxDbusRequestId) {
        return id;
    }

    LinuxDbusCompletion immediate_completion;
    bool complete_immediately = false;

    {
        std::lock_guard lock{impl_->lifecycle_mutex};
        if (impl_->destroying || impl_->stopping || impl_->connection == nullptr ||
            !impl_->running.load(std::memory_order_acquire) ||
            impl_->stop_requested.load(std::memory_order_acquire)) {
            immediate_completion.code = LinuxDbusErrorCode::Shutdown;
            complete_immediately = true;
        } else if (!impl_->clients.contains(client)) {
            immediate_completion.code = LinuxDbusErrorCode::Cancelled;
            complete_immediately = true;
        } else {
            DBusMessage* message = dbus_message_new_method_call(
                call.destination.c_str(), call.path.c_str(), call.interface.c_str(),
                call.member.c_str());
            if (message == nullptr) {
                immediate_completion = LinuxDbusCompletion{
                    LinuxDbusErrorCode::LocalProtocolError, {},
                    "Unable to allocate D-Bus method-call message"};
                complete_immediately = true;
            } else {
                std::string encode_error;
                if (!linux_dbus_append_values(message, call.arguments, encode_error)) {
                    dbus_message_unref(message);
                    immediate_completion = LinuxDbusCompletion{
                        LinuxDbusErrorCode::LocalProtocolError, {},
                        encode_error.empty() ? "Unable to encode D-Bus method-call arguments"
                                             : std::move(encode_error)};
                    complete_immediately = true;
                } else {
                    DBusPendingCall* pending = nullptr;
                    const int timeout_ms = static_cast<int>(call.timeout.count());
                    const dbus_bool_t sent = dbus_connection_send_with_reply(
                        impl_->connection, message, &pending, timeout_ms);
                    dbus_message_unref(message);

                    if (sent == FALSE) {
                        immediate_completion = LinuxDbusCompletion{
                            LinuxDbusErrorCode::LocalProtocolError, {},
                            "Unable to queue D-Bus method call"};
                        complete_immediately = true;
                    } else if (pending == nullptr) {
                        immediate_completion = LinuxDbusCompletion{
                            LinuxDbusErrorCode::Disconnected, {},
                            "D-Bus connection disconnected before method call was queued"};
                        complete_immediately = true;
                    } else {
                        auto* context =
                            new (std::nothrow) Impl::NotifyContext{impl_.get(), client, id};
                        if (context == nullptr) {
                            dbus_pending_call_cancel(pending);
                            dbus_pending_call_unref(pending);
                            immediate_completion = LinuxDbusCompletion{
                                LinuxDbusErrorCode::LocalProtocolError, {},
                                "Unable to allocate D-Bus pending-call notification state"};
                            complete_immediately = true;
                        } else {
                            dbus_pending_call_ref(pending);
                            bool inserted = false;
                            try {
                                std::lock_guard pending_lock{impl_->pending_mutex};
                                inserted = impl_->pending_calls
                                               .try_emplace(
                                                   id,
                                                   Impl::NativePendingCall{
                                                       client,
                                                       pending,
                                                       std::chrono::steady_clock::now() +
                                                           call.timeout})
                                               .second;
                            } catch (...) {
                                inserted = false;
                            }

                            if (!inserted) {
                                dbus_pending_call_cancel(pending);
                                dbus_pending_call_unref(pending);
                                dbus_pending_call_unref(pending);
                                delete context;
                                immediate_completion = LinuxDbusCompletion{
                                    LinuxDbusErrorCode::LocalProtocolError, {},
                                    "Unable to retain D-Bus pending-call state"};
                                complete_immediately = true;
                            } else if (dbus_pending_call_set_notify(
                                           pending, &Impl::pending_notify, context,
                                           &Impl::free_notify_context) == FALSE) {
                                auto native = impl_->take_pending(client, id, pending);
                                dbus_pending_call_cancel(pending);
                                if (native) {
                                    dbus_pending_call_unref(native->pending);
                                }
                                dbus_pending_call_unref(pending);
                                delete context;
                                immediate_completion = LinuxDbusCompletion{
                                    LinuxDbusErrorCode::LocalProtocolError, {},
                                    "Unable to register D-Bus pending-call notification"};
                                complete_immediately = true;
                            } else {
                                dbus_pending_call_unref(pending);
                            }
                        }
                    }
                }
            }
        }
    }

    impl_->wake_io();
    if (complete_immediately) {
        (void)impl_->calls.complete(client, id, std::move(immediate_completion));
    }
    return id;
}

bool LinuxDbusTransport::cancel_request(LinuxDbusClientId client, LinuxDbusRequestId id) {
    auto native = impl_->take_pending(client, id);
    if (!native) {
        return false;
    }

    dbus_pending_call_cancel(native->pending);
    dbus_pending_call_unref(native->pending);
    return impl_->calls.cancel(client, id);
}

std::size_t LinuxDbusTransport::pending_request_count() const noexcept {
    return impl_->calls.pending_count();
}

LinuxDbusSubscriptionId LinuxDbusTransport::subscribe_signal(
    LinuxDbusClientId client,
    ui::Dispatcher dispatcher,
    const LinuxDbusSignalMatch& match,
    LinuxDbusSignalCallback callback) {
    if (client == kInvalidLinuxDbusClientId || !dispatcher.valid() || !callback ||
        !valid_signal_match(match)) {
        return kInvalidLinuxDbusSubscriptionId;
    }

    try {
        auto entry = std::make_shared<Impl::SignalEntry>();
        entry->client = client;
        entry->dispatcher = std::move(dispatcher);
        entry->match = match;
        entry->callback = std::move(callback);
        entry->rule = signal_match_rule(match);
        auto setup = std::make_shared<Impl::SignalSetupCommand>();
        setup->entry = entry;

        std::unique_lock lifecycle_lock{impl_->lifecycle_mutex};
        if (impl_->destroying || impl_->stopping || impl_->connection == nullptr ||
            !impl_->running.load(std::memory_order_acquire) ||
            impl_->stop_requested.load(std::memory_order_acquire) ||
            !impl_->clients.contains(client)) {
            return kInvalidLinuxDbusSubscriptionId;
        }
        const auto id = impl_->ledger.acquire_subscription(client);
        if (id == kInvalidLinuxDbusSubscriptionId) {
            return id;
        }

        bool installed_filter_now = false;
        bool inserted_signal = false;
        bool remove_filter_after_insert_failure = false;
        {
            std::lock_guard signal_lock{impl_->signal_mutex};
            if (!impl_->signal_filter_installed) {
                if (dbus_connection_add_filter(impl_->connection, &Impl::signal_filter,
                                               impl_.get(), nullptr) == FALSE) {
                    (void)impl_->ledger.release_subscription(client, id);
                    return kInvalidLinuxDbusSubscriptionId;
                }
                impl_->signal_filter_installed = true;
                installed_filter_now = true;
            }

            try {
                impl_->signals.emplace(id, entry);
                inserted_signal = true;
            } catch (...) {
                if (installed_filter_now && impl_->signals.empty() &&
                    impl_->signal_filter_installed) {
                    impl_->signal_filter_installed = false;
                    remove_filter_after_insert_failure = true;
                }
            }
        }
        if (!inserted_signal) {
            if (remove_filter_after_insert_failure) {
                dbus_connection_remove_filter(impl_->connection, &Impl::signal_filter,
                                              impl_.get());
            }
            (void)impl_->ledger.release_subscription(client, id);
            return kInvalidLinuxDbusSubscriptionId;
        }

        bool setup_success = false;
        if (impl_->io_thread.get_id() == std::this_thread::get_id()) {
            setup_success = Impl::install_signal_match(impl_->connection, entry->rule);
            if (setup_success) {
                entry->active->store(true, std::memory_order_release);
            }
        } else {
            bool setup_enqueued = false;
            try {
                std::lock_guard control_lock{impl_->control_mutex};
                impl_->signal_setup_commands.push_back(setup);
                setup_enqueued = true;
            } catch (...) {
                setup_enqueued = false;
            }

            if (!setup_enqueued) {
                bool remove_filter = false;
                {
                    std::lock_guard signal_lock{impl_->signal_mutex};
                    impl_->signals.erase(id);
                    if (installed_filter_now && impl_->signals.empty() &&
                        impl_->signal_filter_installed) {
                        impl_->signal_filter_installed = false;
                        remove_filter = true;
                    }
                }
                if (remove_filter) {
                    dbus_connection_remove_filter(impl_->connection, &Impl::signal_filter,
                                                  impl_.get());
                }
                (void)impl_->ledger.release_subscription(client, id);
                return kInvalidLinuxDbusSubscriptionId;
            }

            impl_->wake_io();

            std::unique_lock setup_lock{setup->mutex};
            if (!setup->ready.wait_for(setup_lock, std::chrono::milliseconds{1'500},
                                       [&] { return setup->done; })) {
                setup->cancelled = true;
                setup_success = false;
            } else {
                setup_success = setup->success;
            }
        }

        if (setup_success) {
            return id;
        }

        bool remove_filter = false;
        {
            std::lock_guard signal_lock{impl_->signal_mutex};
            impl_->signals.erase(id);
            if (installed_filter_now && impl_->signals.empty() &&
                impl_->signal_filter_installed) {
                impl_->signal_filter_installed = false;
                remove_filter = true;
            }
        }
        if (remove_filter) {
            dbus_connection_remove_filter(impl_->connection, &Impl::signal_filter,
                                          impl_.get());
        }
        (void)impl_->ledger.release_subscription(client, id);
        return kInvalidLinuxDbusSubscriptionId;
    } catch (...) {
        return kInvalidLinuxDbusSubscriptionId;
    }
}

bool LinuxDbusTransport::unsubscribe_signal(LinuxDbusClientId client,
                                            LinuxDbusSubscriptionId id) {
    if (client == kInvalidLinuxDbusClientId || id == kInvalidLinuxDbusSubscriptionId) {
        return false;
    }

    std::shared_ptr<Impl::SignalEntry> entry;
    bool remove_filter = false;
    {
        std::lock_guard lifecycle_lock{impl_->lifecycle_mutex};
        if (impl_->connection == nullptr || impl_->stopping) {
            return false;
        }
        {
            std::lock_guard signal_lock{impl_->signal_mutex};
            const auto found = impl_->signals.find(id);
            if (found == impl_->signals.end() || !found->second ||
                found->second->client != client) {
                return false;
            }
            entry = found->second;
            entry->active->store(false, std::memory_order_release);
            impl_->signals.erase(found);
            if (impl_->signals.empty() && impl_->signal_filter_installed) {
                impl_->signal_filter_installed = false;
                remove_filter = true;
            }
        }

        if (remove_filter) {
            dbus_connection_remove_filter(impl_->connection, &Impl::signal_filter,
                                          impl_.get());
        }
        Impl::remove_signal_match_async(impl_->connection, entry->rule);
    }

    return impl_->ledger.release_subscription(client, id);
}

std::size_t LinuxDbusTransport::subscription_count() const noexcept {
    std::lock_guard lock{impl_->signal_mutex};
    return impl_->signals.size();
}

bool LinuxDbusTransport::send_signal(
    std::string_view path,
    std::string_view interface,
    std::string_view member,
    const std::vector<LinuxDbusValue>& arguments) {
    if (path.empty() || interface.empty() || member.empty() ||
        contains_nul(path) || contains_nul(interface) || contains_nul(member)) {
        return false;
    }

    try {
        const std::string path_text{path};
        const std::string interface_text{interface};
        const std::string member_text{member};
        if (dbus_validate_path(path_text.c_str(), nullptr) == FALSE ||
            dbus_validate_interface(interface_text.c_str(), nullptr) == FALSE ||
            dbus_validate_member(member_text.c_str(), nullptr) == FALSE) {
            return false;
        }
        for (const auto& argument : arguments) {
            std::string signature;
            std::string error;
            if (!linux_dbus_value_signature(argument, signature, error)) {
                return false;
            }
        }

        std::lock_guard lifecycle_lock{impl_->lifecycle_mutex};
        if (impl_->destroying || impl_->stopping || impl_->connection == nullptr ||
            !impl_->running.load(std::memory_order_acquire) ||
            impl_->stop_requested.load(std::memory_order_acquire)) {
            return false;
        }

        DBusMessage* message = dbus_message_new_signal(
            path_text.c_str(), interface_text.c_str(), member_text.c_str());
        if (message == nullptr) {
            return false;
        }
        std::string encode_error;
        if (!linux_dbus_append_values(message, arguments, encode_error)) {
            dbus_message_unref(message);
            return false;
        }
        const dbus_bool_t sent = dbus_connection_send(impl_->connection, message, nullptr);
        dbus_message_unref(message);
        return sent != FALSE;
    } catch (...) {
        return false;
    }
}

LinuxDbusObjectRegistrationId LinuxDbusTransport::register_object_path(
    LinuxDbusClientId client,
    std::string path,
    LinuxDbusObjectPathHandler handler) {
    if (client == kInvalidLinuxDbusClientId || !handler || contains_nul(path) ||
        !linux_dbus_valid_object_path(path)) {
        return kInvalidLinuxDbusObjectRegistrationId;
    }

    try {
        std::lock_guard lifecycle_lock{impl_->lifecycle_mutex};
        if (impl_->destroying || impl_->stopping || impl_->connection == nullptr ||
            !impl_->running.load(std::memory_order_acquire) ||
            impl_->stop_requested.load(std::memory_order_acquire) ||
            !impl_->clients.contains(client)) {
            return kInvalidLinuxDbusObjectRegistrationId;
        }

        std::lock_guard object_lock{impl_->object_mutex};
        if (impl_->object_path_ids.contains(path)) {
            return kInvalidLinuxDbusObjectRegistrationId;
        }

        const auto id = impl_->ledger.acquire_object_path(client);
        if (id == kInvalidLinuxDbusObjectRegistrationId) {
            return id;
        }

        std::shared_ptr<Impl::ObjectPathEntry> entry;
        try {
            entry = std::make_shared<Impl::ObjectPathEntry>();
            entry->client = client;
            entry->path = path;
            entry->handler = std::move(handler);
        } catch (...) {
            (void)impl_->ledger.release_object_path(client, id);
            return kInvalidLinuxDbusObjectRegistrationId;
        }

        auto* context = new (std::nothrow) Impl::ObjectPathContext{impl_.get(), id};
        if (context == nullptr) {
            (void)impl_->ledger.release_object_path(client, id);
            return kInvalidLinuxDbusObjectRegistrationId;
        }

        if (dbus_connection_register_object_path(
                impl_->connection, path.c_str(), &Impl::object_path_vtable(), context) == FALSE) {
            delete context;
            (void)impl_->ledger.release_object_path(client, id);
            return kInvalidLinuxDbusObjectRegistrationId;
        }

        bool inserted_entry = false;
        bool inserted_path = false;
        try {
            inserted_entry = impl_->object_paths.emplace(id, entry).second;
            if (inserted_entry) {
                inserted_path = impl_->object_path_ids.emplace(entry->path, id).second;
            }
        } catch (...) {
            inserted_path = false;
        }
        if (!inserted_entry || !inserted_path) {
            if (inserted_path) {
                impl_->object_path_ids.erase(entry->path);
            }
            if (inserted_entry) {
                impl_->object_paths.erase(id);
            }
            (void)dbus_connection_unregister_object_path(
                impl_->connection, entry->path.c_str());
            (void)impl_->ledger.release_object_path(client, id);
            return kInvalidLinuxDbusObjectRegistrationId;
        }
        return id;
    } catch (...) {
        return kInvalidLinuxDbusObjectRegistrationId;
    }
}

bool LinuxDbusTransport::unregister_object_path(LinuxDbusClientId client,
                                                LinuxDbusObjectRegistrationId id) {
    if (client == kInvalidLinuxDbusClientId ||
        id == kInvalidLinuxDbusObjectRegistrationId) {
        return false;
    }

    std::shared_ptr<Impl::ObjectPathEntry> entry;
    {
        std::lock_guard lifecycle_lock{impl_->lifecycle_mutex};
        if (impl_->connection == nullptr || impl_->stopping) {
            return false;
        }

        std::lock_guard object_lock{impl_->object_mutex};
        const auto found = impl_->object_paths.find(id);
        if (found == impl_->object_paths.end() || found->second->client != client) {
            return false;
        }
        entry = found->second;
        {
            std::lock_guard entry_lock{entry->mutex};
            entry->active = false;
        }

        if (dbus_connection_unregister_object_path(
                impl_->connection, entry->path.c_str()) == FALSE) {
            std::lock_guard entry_lock{entry->mutex};
            entry->active = true;
            return false;
        }

        impl_->object_path_ids.erase(entry->path);
        impl_->object_paths.erase(found);
    }

    (void)impl_->ledger.release_object_path(client, id);

    if (impl_->io_thread.get_id() != std::this_thread::get_id()) {
        std::unique_lock entry_lock{entry->mutex};
        entry->idle.wait(entry_lock, [&] { return entry->in_flight == 0; });
    }
    return true;
}

std::size_t LinuxDbusTransport::object_path_count() const noexcept {
    std::lock_guard lock{impl_->object_mutex};
    return impl_->object_paths.size();
}

void LinuxDbusTransport::release_client(LinuxDbusClientId client) noexcept {
    if (client == kInvalidLinuxDbusClientId) {
        return;
    }

    {
        std::lock_guard lock{impl_->lifecycle_mutex};
        impl_->clients.erase(client);
    }

    impl_->calls.discard_client(client);

    for (;;) {
        Impl::NativePendingCall native;
        bool found = false;
        {
            std::lock_guard lock{impl_->pending_mutex};
            for (auto it = impl_->pending_calls.begin(); it != impl_->pending_calls.end(); ++it) {
                if (it->second.client == client) {
                    native = it->second;
                    impl_->pending_calls.erase(it);
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            break;
        }
        if (native.pending != nullptr) {
            dbus_pending_call_cancel(native.pending);
            dbus_pending_call_unref(native.pending);
        }
    }

    for (;;) {
        LinuxDbusSubscriptionId id = kInvalidLinuxDbusSubscriptionId;
        {
            std::lock_guard lock{impl_->signal_mutex};
            for (const auto& [candidate, entry] : impl_->signals) {
                if (entry && entry->client == client) {
                    id = candidate;
                    break;
                }
            }
        }
        if (id == kInvalidLinuxDbusSubscriptionId || !unsubscribe_signal(client, id)) {
            break;
        }
    }

    for (;;) {
        LinuxDbusObjectRegistrationId id = kInvalidLinuxDbusObjectRegistrationId;
        {
            std::lock_guard lock{impl_->object_mutex};
            for (const auto& [candidate, entry] : impl_->object_paths) {
                if (entry && entry->client == client) {
                    id = candidate;
                    break;
                }
            }
        }
        if (id == kInvalidLinuxDbusObjectRegistrationId || !unregister_object_path(client, id)) {
            break;
        }
    }
}

} // namespace ui::detail
