# Linux platform prerequisites

NativeUI Linux/X11 platform builds use system development packages for the native platform boundary. The D-Bus transport introduced by T072 uses the system `libdbus-1` C API directly; it does not bundle or require GLib/GIO, QtDBus, sd-bus, sdbus-c++, or another D-Bus wrapper.

## Required packages

A Linux platform build requires the existing X11/OpenGL/font dependencies plus:

- `pkg-config`;
- the `libdbus-1` development headers and library, exposed through the `dbus-1` pkg-config module.

On Debian/Ubuntu, the D-Bus prerequisites are:

```sh
sudo apt-get install pkg-config libdbus-1-dev
```

Equivalent distribution packages are acceptable as long as this succeeds:

```sh
pkg-config --exists dbus-1
```

## Scope

`libdbus-1` is a Linux platform prerequisite only. macOS and Windows builds do not discover or link it.

NativeUI uses a private session-bus connection owned by the internal Linux transport. System-bus access and a public NativeUI D-Bus API are outside the v1 scope.

## Explicit bus addresses

The internal Linux transport can also be started against one caller-supplied D-Bus address instead of the process session bus. The explicit mode uses `dbus_connection_open_private()` plus `dbus_bus_register()` and keeps the exact same private-connection ownership, single I/O thread, bounded dispatch loop, resource limits (1,024 pending calls, 256 subscriptions, 256 object paths), timeout validation and error taxonomy as the default session path.

- The address must be non-empty and syntactically valid. Empty, NUL-containing or malformed addresses are rejected with `InvalidArgument` before any connection, thread, ledger slot or file descriptor is acquired.
- A connection-open or `dbus_bus_register()` failure returns `BusUnavailable` and leaves the transport stopped with no leaked connection, thread, ledger slot or file descriptor.
- `stop()`/destructor stay idempotent and no-throw after a successful start, a failed start and a partial start; a later start behaves exactly like a fresh transport. The explicit address is never retained as process-global or transport-global state.

## Accessibility-bus discovery

The internal one-shot discovery helper resolves the AT-SPI2 accessibility-bus address:

- a non-empty `AT_SPI_BUS_ADDRESS` environment variable wins with no bus round-trip;
- otherwise exactly one bounded `org.a11y.Bus.GetAddress` call is made on the session bus. A caller-supplied running session transport is borrowed when available; otherwise one private session connection is opened only for that call and closed before the helper returns.

There is no retry loop, no polling, no background re-discovery and no process-global address cache. Malformed or empty `GetAddress` replies, remote errors, timeouts and an unavailable session bus each return a bounded error with an empty address so the accessibility bridge disables once with a single diagnostic. The blocking call is bounded by the frozen timeout validation (`kLinuxDbusMinTimeout`..`kLinuxDbusMaxTimeout`, default `kLinuxDbusDefaultTimeout`).

## Runtime environment

The default path requires a reachable session bus, normally provided through `DBUS_SESSION_BUS_ADDRESS`. Accessibility discovery additionally expects the `org.a11y.Bus` launcher to be reachable on that session bus; the discovered accessibility address is then passed to the explicit-address start mode. No additional dependency, environment variable or system-bus access is introduced by the transport.
