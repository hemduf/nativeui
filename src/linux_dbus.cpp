#include "detail/linux_dbus.hpp"
#include "detail/linux_dbus_codec.hpp"

#include <dbus/dbus.h>

#include <atomic>
#include <mutex>
#include <new>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>

namespace ui::detail {
namespace {

std::once_flag g_dbus_threads_once;
bool g_dbus_threads_initialized = false;

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
    std::call_once(g_dbus_threads_once, [] {
        g_dbus_threads_initialized = dbus_threads_init_default() != FALSE;
    });
    return g_dbus_threads_initialized;
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
    };

    explicit Impl(LinuxDbusResourceLedger& resource_ledger)
        : ledger(resource_ledger),
          completion_gate(std::make_shared<std::atomic<bool>>(true)) {}

    LinuxDbusResourceLedger& ledger;
    mutable std::mutex mutex;
    std::unordered_map<LinuxDbusRequestId, std::unique_ptr<Entry>> calls;
    std::shared_ptr<std::atomic<bool>> completion_gate;
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
        entry->client = client;
        entry->dispatcher = std::move(dispatcher);
        entry->callback = std::move(callback);

        bool inserted = false;
        {
            std::lock_guard lock{impl_->mutex};
            if (!impl_->closing) {
                auto [it, did_insert] = impl_->calls.try_emplace(id);
                if (did_insert) {
                    it->second = std::move(entry);
                    inserted = true;
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
    std::shared_ptr<std::atomic<bool>> completion_gate;
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
        completion_gate = impl_->completion_gate;
    }

    (void)impl_->ledger.release_request(client, id);

    auto dispatcher = std::move(entry->dispatcher);
    auto callback = std::move(entry->callback);
    entry.reset();

    try {
        (void)dispatcher.post(
            [completion_gate = std::move(completion_gate), callback = std::move(callback),
             completion = std::move(completion)]() mutable {
                if (!completion_gate->load(std::memory_order_acquire)) {
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

void LinuxDbusPendingCallSet::shutdown() noexcept {
    std::unordered_map<LinuxDbusRequestId, std::unique_ptr<Impl::Entry>> discarded;
    try {
        {
            std::lock_guard lock{impl_->mutex};
            impl_->completion_gate->store(false, std::memory_order_release);
            if (impl_->closing) {
                return;
            }
            impl_->closing = true;
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
    };

    struct NotifyContext final {
        Impl* owner{};
        LinuxDbusClientId client{};
        LinuxDbusRequestId id{};
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

    mutable std::mutex lifecycle_mutex;
    mutable std::mutex pending_mutex;
    DBusConnection* connection{};
    std::thread io_thread;
    std::atomic<bool> stop_requested{false};
    std::atomic<bool> running{false};
    std::string unique_name;
    bool destroying{};

    LinuxDbusResourceLedger ledger;
    LinuxDbusPendingCallSet calls;
    std::unordered_map<LinuxDbusRequestId, NativePendingCall> pending_calls;
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
    if (impl_->destroying) {
        return LinuxDbusErrorCode::Shutdown;
    }
    if (impl_->running.load(std::memory_order_acquire)) {
        return LinuxDbusErrorCode::None;
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
                if (dbus_connection_read_write_dispatch(state->connection, 100) == FALSE) {
                    break;
                }
            }
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
    {
        std::lock_guard lock{impl_->lifecycle_mutex};
        impl_->stop_requested.store(true, std::memory_order_release);
        suppress_completions = impl_->destroying;

        if (impl_->io_thread.joinable()) {
            impl_->io_thread.join();
        }

        if (impl_->connection != nullptr) {
            dbus_connection_close(impl_->connection);
            dbus_connection_unref(impl_->connection);
            impl_->connection = nullptr;
        }

        impl_->running.store(false, std::memory_order_release);
        impl_->unique_name.clear();
    }

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

LinuxDbusRequestId LinuxDbusTransport::call_method(
    LinuxDbusClientId client,
    ui::Dispatcher dispatcher,
    const LinuxDbusMethodCall& call,
    LinuxDbusCompletionCallback callback) {
    if (client == kInvalidLinuxDbusClientId || !dispatcher.valid() || !callback ||
        !valid_method_call(call)) {
        return kInvalidLinuxDbusRequestId;
    }

    {
        std::lock_guard lock{impl_->lifecycle_mutex};
        if (impl_->destroying || impl_->connection == nullptr ||
            !impl_->running.load(std::memory_order_acquire) ||
            impl_->stop_requested.load(std::memory_order_acquire)) {
            return kInvalidLinuxDbusRequestId;
        }
    }

    const LinuxDbusRequestId id =
        impl_->calls.begin(client, dispatcher, call.timeout, std::move(callback));
    if (id == kInvalidLinuxDbusRequestId) {
        return id;
    }

    LinuxDbusCompletion immediate_completion;
    bool complete_immediately = false;

    {
        std::lock_guard lock{impl_->lifecycle_mutex};
        if (impl_->destroying || impl_->connection == nullptr ||
            !impl_->running.load(std::memory_order_acquire) ||
            impl_->stop_requested.load(std::memory_order_acquire)) {
            immediate_completion.code = LinuxDbusErrorCode::Shutdown;
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
                                                   Impl::NativePendingCall{client, pending})
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

} // namespace ui::detail
