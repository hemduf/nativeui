# Linux linker comparison

This opt-in GitHub Actions workflow measures the cost of relinking NativeUI's
headless CTest executables with the default GNU linker and with `mold`.

## Usage

Open the `Linux Linker Benchmark` workflow in GitHub Actions and dispatch it
against the intended branch. The workflow also qualifies its own implementation
when the benchmark workflow or script changes in a pull request. It does not
run on normal feature PR updates.

The benchmark:

1. Configures and builds a fixed Release CMake/Ninja test tree once.
2. Reconfigures the same build tree with the common
   `CMAKE_EXE_LINKER_FLAGS=-Wl,--build-id=sha1` option, forcing every
   executable to relink using the original system linker.
3. Times this pass with one Ninja build slot and records executable identity,
   link count, and any unexpected C/C++ compilation.
4. Changes only the executable linker selection by using
   `-Wl,--build-id=sha1 -fuse-ld=mold` and repeats the relink.
5. Refuses comparison if target identities/counts differ, no executables
   were linked, or either pass recompiled object files.
6. Runs the normal headless unit CTest suite on the mold-linked binaries.

The JSON timing artifact and GitHub step summary record the evidence.
A faster link time does not automatically justify changing the default
toolchain: test reliability, ABIs, runtime integration and cold-cache results
must also be assessed. This workflow does not affect macOS, Windows, release
packaging, regular PR CI, or NativeUI's default linker configuration.
