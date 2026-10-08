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
2. Changes only the modification timestamp of the already compiled
   `libnativeui_core.a` to trigger a fresh baseline relink.
3. Times that relink with a single Ninja build slot, recording how many
   executable links and unexpected C/C++ compilations occurred.
4. Reconfigures the *same build tree* with
   `CMAKE_EXE_LINKER_FLAGS=-fuse-ld=mold`, then times its relink with the
   same single build slot.
5. Refuses a numerical comparison if linker invocation counts differ, no
   executables were linked, or either relink recompiled object files.
6. Runs the normal headless unit CTest suite on the mold-linked binaries.

The JSON timing artifact and GitHub step summary record the evidence.
A faster link time does not automatically justify changing the default
toolchain: test reliability, ABIs, runtime integration and cold-cache results
must also be assessed. This workflow does not affect macOS, Windows, release
packaging, regular PR CI, or NativeUI's default linker configuration.
