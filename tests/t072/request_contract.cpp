#include "detail/linux_dbus.hpp"

#include <nativeui/detail/dispatcher_owner.hpp>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <vector>

int main() {
    using namespace std::chrono_literals;
    using namespace ui::detail;

    constexpr LinuxDbusClientId client_a = 1;
    constexpr LinuxDbusClientId client_b = 2;

    LinuxDbusResourceLedger ledger;
    LinuxDbusPendingCallSet calls{ledger};
    DispatcherOwner owner;
    const auto dispatcher = owner.dispatcher();

    std::vector<LinuxDbusErrorCode> completions;
    const auto first = calls.begin(
        client_a, dispatcher, 30s,
        [&](LinuxDbusCompletion result) { completions.push_back(result.code); });
    if (first == kInvalidLinuxDbusRequestId || calls.pending_count() != 1 ||
        ledger.pending_request_count() != 1) {
        return EXIT_FAILURE;
    }

    if (!calls.complete(
            client_a, first,
            LinuxDbusCompletion{LinuxDbusErrorCode::None, {}, {}}) ||
        calls.pending_count() != 0 || ledger.pending_request_count() != 0 ||
        !completions.empty()) {
        return EXIT_FAILURE;
    }
    if (owner.checkpoint() != 1 || completions.size() != 1 ||
        completions.front() != LinuxDbusErrorCode::None ||
        calls.complete(
            client_a, first,
            LinuxDbusCompletion{LinuxDbusErrorCode::RemoteError, {}, {}}) ||
        calls.cancel(client_a, first)) {
        return EXIT_FAILURE;
    }

    const auto cancelled = calls.begin(
        client_a, dispatcher, 30s,
        [&](LinuxDbusCompletion result) { completions.push_back(result.code); });
    if (cancelled == kInvalidLinuxDbusRequestId || calls.cancel(client_b, cancelled) ||
        !calls.cancel(client_a, cancelled) || completions.size() != 1 ||
        owner.checkpoint() != 1 || completions.size() != 2 ||
        completions.back() != LinuxDbusErrorCode::Cancelled) {
        return EXIT_FAILURE;
    }

    if (calls.begin(kInvalidLinuxDbusClientId, dispatcher, 30s, [](LinuxDbusCompletion) {}) !=
            kInvalidLinuxDbusRequestId ||
        calls.begin(client_a, ui::Dispatcher{}, 30s, [](LinuxDbusCompletion) {}) !=
            kInvalidLinuxDbusRequestId ||
        calls.begin(client_a, dispatcher, 0ms, [](LinuxDbusCompletion) {}) !=
            kInvalidLinuxDbusRequestId ||
        calls.begin(client_a, dispatcher, 301s, [](LinuxDbusCompletion) {}) !=
            kInvalidLinuxDbusRequestId ||
        calls.begin(client_a, dispatcher, 30s, {}) != kInvalidLinuxDbusRequestId) {
        return EXIT_FAILURE;
    }

    std::vector<LinuxDbusRequestId> saturated;
    saturated.reserve(kLinuxDbusMaxPendingCalls);
    for (std::size_t i = 0; i < kLinuxDbusMaxPendingCalls; ++i) {
        const auto id = calls.begin(client_a, dispatcher, 30s, [](LinuxDbusCompletion) {});
        if (id == kInvalidLinuxDbusRequestId) {
            return EXIT_FAILURE;
        }
        saturated.push_back(id);
    }
    if (calls.pending_count() != kLinuxDbusMaxPendingCalls ||
        ledger.pending_request_count() != kLinuxDbusMaxPendingCalls ||
        calls.begin(client_b, dispatcher, 30s, [](LinuxDbusCompletion) {}) !=
            kInvalidLinuxDbusRequestId) {
        return EXIT_FAILURE;
    }

    if (!calls.complete(
            client_a, saturated.front(),
            LinuxDbusCompletion{LinuxDbusErrorCode::None, {}, {}}) ||
        calls.pending_count() != kLinuxDbusMaxPendingCalls - 1 ||
        ledger.pending_request_count() != kLinuxDbusMaxPendingCalls - 1) {
        return EXIT_FAILURE;
    }
    const auto replacement =
        calls.begin(client_b, dispatcher, 30s, [](LinuxDbusCompletion) {});
    if (replacement == kInvalidLinuxDbusRequestId ||
        calls.pending_count() != kLinuxDbusMaxPendingCalls) {
        return EXIT_FAILURE;
    }

    calls.shutdown();
    if (calls.pending_count() != 0 || ledger.pending_request_count() != 0) {
        return EXIT_FAILURE;
    }

    LinuxDbusResourceLedger rejected_ledger;
    LinuxDbusPendingCallSet rejected_calls{rejected_ledger};
    DispatcherOwner rejected_owner;
    const auto rejected_dispatcher = rejected_owner.dispatcher();
    bool rejected_callback_ran = false;
    const auto rejected = rejected_calls.begin(
        client_a, rejected_dispatcher, 30s,
        [&](LinuxDbusCompletion) { rejected_callback_ran = true; });
    if (rejected == kInvalidLinuxDbusRequestId) {
        return EXIT_FAILURE;
    }
    rejected_owner.shutdown();
    if (!rejected_calls.complete(
            client_a, rejected,
            LinuxDbusCompletion{LinuxDbusErrorCode::None, {}, {}}) ||
        rejected_callback_ran || rejected_ledger.pending_request_count() != 0) {
        return EXIT_FAILURE;
    }

    LinuxDbusResourceLedger teardown_ledger;
    DispatcherOwner teardown_owner;
    const auto teardown_dispatcher = teardown_owner.dispatcher();
    bool teardown_callback_ran = false;
    {
        LinuxDbusPendingCallSet teardown_calls{teardown_ledger};
        const auto teardown_id = teardown_calls.begin(
            client_a, teardown_dispatcher, 30s,
            [&](LinuxDbusCompletion) { teardown_callback_ran = true; });
        if (teardown_id == kInvalidLinuxDbusRequestId ||
            !teardown_calls.complete(
                client_a, teardown_id,
                LinuxDbusCompletion{LinuxDbusErrorCode::None, {}, {}}) ||
            teardown_ledger.pending_request_count() != 0) {
            return EXIT_FAILURE;
        }
        teardown_calls.shutdown();
    }
    if (teardown_owner.checkpoint() != 1 || teardown_callback_ran) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
