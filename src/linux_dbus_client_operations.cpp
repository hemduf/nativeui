#include "detail/linux_dbus_client_operations.hpp"
#include "detail/linux_dbus_codec.hpp"

#include <dbus/dbus.h>

#include <utility>

namespace ui::detail {
namespace {

[[nodiscard]] bool contains_nul(std::string_view text) noexcept {
    return text.find('\0') != std::string_view::npos;
}

[[nodiscard]] bool valid_method_call_input(const LinuxDbusMethodCall& call) noexcept {
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

[[nodiscard]] bool valid_signal_match_input(const LinuxDbusSignalMatch& match) noexcept {
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

[[nodiscard]] bool valid_signal_send_input(
    std::string_view path,
    std::string_view interface,
    std::string_view member,
    const std::vector<LinuxDbusValue>& arguments) noexcept {
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
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

LinuxDbusClientOperations::~LinuxDbusClientOperations() {
    stop();
}

LinuxDbusErrorCode LinuxDbusClientOperations::start() {
    std::lock_guard lock{mutex_};
    if (closing_) {
        return LinuxDbusErrorCode::Shutdown;
    }
    return transport_.start();
}

void LinuxDbusClientOperations::stop() noexcept {
    std::vector<LinuxDbusClientId> clients;
    try {
        {
            std::lock_guard lock{mutex_};
            if (closing_) {
                return;
            }
            closing_ = true;
            clients.reserve(clients_.size());
            for (const auto client : clients_) {
                clients.push_back(client);
            }
            clients_.clear();
            object_paths_.clear();
            object_path_names_.clear();
        }

        // Never hold the client-surface mutex across raw teardown. An in-flight
        // object-path handler is user/client code and may re-enter this surface.
        for (const auto client : clients) {
            transport_.release_client(client);
        }
        transport_.stop();
    } catch (...) {
        // Destruction/shutdown is noexcept. The raw transport owns the final
        // join/connection cleanup and its destructor repeats stop idempotently.
        transport_.stop();
    }
}

bool LinuxDbusClientOperations::running() const noexcept {
    return transport_.running();
}

std::string LinuxDbusClientOperations::unique_name() const {
    return transport_.unique_name();
}

LinuxDbusClientRegistrationResult LinuxDbusClientOperations::register_client() {
    std::lock_guard lock{mutex_};
    if (closing_ || !transport_.running()) {
        return {LinuxDbusErrorCode::Shutdown, kInvalidLinuxDbusClientId};
    }

    const auto client = transport_.register_client();
    if (client == kInvalidLinuxDbusClientId) {
        return {transport_.running() ? LinuxDbusErrorCode::LocalProtocolError
                                     : LinuxDbusErrorCode::Disconnected,
                kInvalidLinuxDbusClientId};
    }

    try {
        clients_.emplace(client);
    } catch (...) {
        transport_.release_client(client);
        return {LinuxDbusErrorCode::LocalProtocolError, kInvalidLinuxDbusClientId};
    }
    return {LinuxDbusErrorCode::None, client};
}

bool LinuxDbusClientOperations::release_client(LinuxDbusClientId client) noexcept {
    if (client == kInvalidLinuxDbusClientId) {
        return false;
    }

    bool found = false;
    try {
        {
            std::lock_guard lock{mutex_};
            const auto client_it = clients_.find(client);
            if (client_it == clients_.end()) {
                return false;
            }
            clients_.erase(client_it);
            found = true;

            for (auto it = object_paths_.begin(); it != object_paths_.end();) {
                if (it->second.client == client) {
                    object_path_names_.erase(it->second.path);
                    it = object_paths_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        transport_.release_client(client);
    } catch (...) {
        if (found) {
            transport_.release_client(client);
        }
    }
    return found;
}

std::size_t LinuxDbusClientOperations::client_count() const noexcept {
    std::lock_guard lock{mutex_};
    return clients_.size();
}

LinuxDbusRequestStartResult LinuxDbusClientOperations::call_method(
    LinuxDbusClientId client,
    ui::Dispatcher dispatcher,
    const LinuxDbusMethodCall& call,
    LinuxDbusCompletionCallback callback) {
    std::lock_guard lock{mutex_};
    if (closing_ || !transport_.running()) {
        return {LinuxDbusErrorCode::Shutdown, kInvalidLinuxDbusRequestId};
    }
    if (!has_client_locked(client) || !dispatcher.valid() || !callback ||
        !valid_method_call_input(call)) {
        return {LinuxDbusErrorCode::InvalidArgument, kInvalidLinuxDbusRequestId};
    }
    if (transport_.pending_request_count() >= kLinuxDbusMaxPendingCalls) {
        return {LinuxDbusErrorCode::ResourceLimit, kInvalidLinuxDbusRequestId};
    }

    const auto id = transport_.call_method(client, std::move(dispatcher), call,
                                           std::move(callback));
    if (id == kInvalidLinuxDbusRequestId) {
        return {transport_.running() ? LinuxDbusErrorCode::LocalProtocolError
                                     : LinuxDbusErrorCode::Disconnected,
                kInvalidLinuxDbusRequestId};
    }
    return {LinuxDbusErrorCode::None, id};
}

bool LinuxDbusClientOperations::cancel_request(LinuxDbusClientId client,
                                               LinuxDbusRequestId id) {
    std::lock_guard lock{mutex_};
    return !closing_ && has_client_locked(client) &&
           transport_.cancel_request(client, id);
}

std::size_t LinuxDbusClientOperations::pending_request_count() const noexcept {
    return transport_.pending_request_count();
}

LinuxDbusSubscriptionResult LinuxDbusClientOperations::subscribe_signal(
    LinuxDbusClientId client,
    ui::Dispatcher dispatcher,
    const LinuxDbusSignalMatch& match,
    LinuxDbusSignalCallback callback) {
    std::lock_guard lock{mutex_};
    if (closing_ || !transport_.running()) {
        return {LinuxDbusErrorCode::Shutdown, kInvalidLinuxDbusSubscriptionId};
    }
    if (!has_client_locked(client) || !dispatcher.valid() || !callback ||
        !valid_signal_match_input(match)) {
        return {LinuxDbusErrorCode::InvalidArgument, kInvalidLinuxDbusSubscriptionId};
    }
    if (transport_.subscription_count() >= kLinuxDbusMaxSubscriptions) {
        return {LinuxDbusErrorCode::ResourceLimit, kInvalidLinuxDbusSubscriptionId};
    }

    const auto id = transport_.subscribe_signal(client, std::move(dispatcher), match,
                                                std::move(callback));
    if (id == kInvalidLinuxDbusSubscriptionId) {
        return {transport_.running() ? LinuxDbusErrorCode::LocalProtocolError
                                     : LinuxDbusErrorCode::Disconnected,
                kInvalidLinuxDbusSubscriptionId};
    }
    return {LinuxDbusErrorCode::None, id};
}

bool LinuxDbusClientOperations::unsubscribe_signal(LinuxDbusClientId client,
                                                   LinuxDbusSubscriptionId id) {
    std::lock_guard lock{mutex_};
    return !closing_ && has_client_locked(client) &&
           transport_.unsubscribe_signal(client, id);
}

std::size_t LinuxDbusClientOperations::subscription_count() const noexcept {
    return transport_.subscription_count();
}

LinuxDbusObjectPathResult LinuxDbusClientOperations::register_object_path(
    LinuxDbusClientId client,
    std::string path,
    LinuxDbusObjectPathHandler handler) {
    std::lock_guard lock{mutex_};
    if (closing_ || !transport_.running()) {
        return {LinuxDbusErrorCode::Shutdown, kInvalidLinuxDbusObjectRegistrationId};
    }
    if (!has_client_locked(client) || !handler || contains_nul(path) ||
        !linux_dbus_valid_object_path(path) || object_path_names_.contains(path)) {
        return {LinuxDbusErrorCode::InvalidArgument, kInvalidLinuxDbusObjectRegistrationId};
    }
    if (transport_.object_path_count() >= kLinuxDbusMaxObjectPaths) {
        return {LinuxDbusErrorCode::ResourceLimit, kInvalidLinuxDbusObjectRegistrationId};
    }

    const std::string retained_path = path;
    const auto id = transport_.register_object_path(client, std::move(path),
                                                    std::move(handler));
    if (id == kInvalidLinuxDbusObjectRegistrationId) {
        return {transport_.running() ? LinuxDbusErrorCode::LocalProtocolError
                                     : LinuxDbusErrorCode::Disconnected,
                kInvalidLinuxDbusObjectRegistrationId};
    }

    try {
        object_path_names_.emplace(retained_path);
        object_paths_.emplace(id, ObjectPathOwner{client, retained_path});
    } catch (...) {
        object_path_names_.erase(retained_path);
        object_paths_.erase(id);
        (void)transport_.unregister_object_path(client, id);
        return {LinuxDbusErrorCode::LocalProtocolError,
                kInvalidLinuxDbusObjectRegistrationId};
    }
    return {LinuxDbusErrorCode::None, id};
}

bool LinuxDbusClientOperations::unregister_object_path(
    LinuxDbusClientId client,
    LinuxDbusObjectRegistrationId id) {
    ObjectPathOwner owner;
    {
        std::lock_guard lock{mutex_};
        if (closing_ || !has_client_locked(client)) {
            return false;
        }
        const auto found = object_paths_.find(id);
        if (found == object_paths_.end() || found->second.client != client) {
            return false;
        }
        owner = found->second;
        object_path_names_.erase(owner.path);
        object_paths_.erase(found);
    }

    // Raw unregister may wait for an in-flight object handler. Do not hold the
    // client-surface mutex while that client code is finishing/re-entering.
    if (transport_.unregister_object_path(client, id)) {
        return true;
    }

    try {
        std::lock_guard lock{mutex_};
        if (!closing_ && has_client_locked(client)) {
            object_path_names_.emplace(owner.path);
            object_paths_.emplace(id, std::move(owner));
        }
    } catch (...) {
        // The raw transport still owns the registration if unregister failed;
        // release_client()/stop() remain the fail-safe teardown path.
    }
    return false;
}

std::size_t LinuxDbusClientOperations::object_path_count() const noexcept {
    return transport_.object_path_count();
}

LinuxDbusErrorCode LinuxDbusClientOperations::send_signal(
    LinuxDbusClientId client,
    std::string_view path,
    std::string_view interface,
    std::string_view member,
    const std::vector<LinuxDbusValue>& arguments) {
    std::lock_guard lock{mutex_};
    if (closing_ || !transport_.running()) {
        return LinuxDbusErrorCode::Shutdown;
    }
    if (!has_client_locked(client) ||
        !valid_signal_send_input(path, interface, member, arguments)) {
        return LinuxDbusErrorCode::InvalidArgument;
    }
    if (transport_.send_signal(path, interface, member, arguments)) {
        return LinuxDbusErrorCode::None;
    }
    return transport_.running() ? LinuxDbusErrorCode::LocalProtocolError
                                : LinuxDbusErrorCode::Disconnected;
}

bool LinuxDbusClientOperations::has_client_locked(LinuxDbusClientId client) const noexcept {
    return client != kInvalidLinuxDbusClientId && clients_.contains(client);
}

} // namespace ui::detail
