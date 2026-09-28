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
