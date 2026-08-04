# AtariStudio

AtariStudio is a C++20 reverse-engineering library and command-line
analysis tool for Atari 8-bit XEX software.

## Current capabilities

- transactional XEX loading into a 64 KiB Atari memory image;
- complete decoding of the 151 official NMOS 6502 opcodes;
- assembly listings, symbols, cross-references, and code/data regions;
- routine discovery, basic blocks, control-flow graphs, dominators,
  post-dominators, and natural-loop analysis;
- branch/flag semantics and structured `if`, loop, and condition recovery;
- structured expression building and readable pseudo-C generation;
- ANTIC display-list discovery and decoding, including LMS screen-memory
  references, scrolling flags, DLI, JMP, and JVB;
- CHBASE/CHBAS character-set discovery with mode-aware 64- and
  128-character glyph decoding;
- relocation detection for common 6502 copy loops;
- automated core, CPU, loader, CFG, and structured-output regressions.

Illegal/undocumented opcodes are preserved as data bytes. Generated
pseudo-C is a semantic representation and uses helper intrinsics for
6502-specific flag and memory behavior; it is not intended to compile as
a standalone C program without a runtime layer.

## Requirements

- Visual Studio with the C++ desktop workload;
- CMake 3.21 or newer;
- a compiler with C++20 support.

## Build and test

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Configure a library-only build without the diagnostic CLI or tests:

```powershell
cmake -S . -B build-library `
    -DBUILD_TESTING=OFF `
    -DATARISTUDIO_BUILD_TESTS=OFF `
    -DATARISTUDIO_BUILD_TOOLS=OFF
cmake --build build-library --config Release
```

Build and run the same suite with AddressSanitizer:

```powershell
cmake -S . -B build-asan -DATARISTUDIO_ENABLE_ASAN=ON
cmake --build build-asan --config Debug
ctest --test-dir build-asan -C Debug --output-on-failure
```

On MSVC, the required `clang_rt.asan` runtime DLLs are copied beside
each AtariStudio executable automatically, so tests and `TestApp` can
also be launched directly from the build tree.

When AtariStudio is included with `add_subdirectory()`, linking the
`AtariStudio` target propagates its C++20 requirement and the MSVC
synchronous exception model to the consuming target. When AddressSanitizer
is enabled, its compile and link requirements are propagated as well,
and the MSVC runtime-copy helper also supports external executables. The
test suite configures, builds, and runs a separate embedded-consumer
project in both regular and sanitized builds to keep this CMake contract
covered. Tools and AtariStudio's own tests default to enabled only when
AtariStudio is the top-level project, so adding the library as a
subdirectory does not inject `TestApp` or internal test targets into the
parent build.

Run the analysis application against an XEX file:

```powershell
.\build\ToolsApp\Debug\TestApp.exe .\hello.xex
.\build\ToolsApp\Debug\TestApp.exe --help
```

Emit a self-contained C++20 translation unit with the generated
6502 runtime prelude:

```powershell
.\build\ToolsApp\Debug\TestApp.exe .\hello.xex --emit-cpp-only .\hello.generated.cpp
```

CLI arguments are validated before loading and analyzing the XEX.
Unknown or extra arguments are rejected, and the input XEX cannot be
used as the generated output path.

The emitted module embeds the final memory image after all overlapping
XEX segments are applied. Call `initialize_image6502()` to restore the
image and CPU state, then `run_entry6502()` to invoke the recovered
RUNAD routine. External OS calls and unresolved control-flow decisions
remain configurable through generated hooks.

CTest also compiles and executes the generated module. The runtime
regression covers address wrapping, stack behavior, status packing,
binary and decimal ADC/SBC, wide BCD carry/borrow propagation,
compare/BIT behavior, shifts, processor flags, and BRK/RTI stack/vector
semantics. Interrupt transfers are exposed through a generated hook.
Dedicated memory and XEX regressions cover little-endian word wrapping,
the complete 64 KiB address range, repeated XEX separators, overlapping
segments, RUNAD/INITAD recovery, and malformed or truncated input.
Core-model regressions cover all segment statistics categories,
full-range and reversed sizes, `XexFile` state reset, and `Result`
success/failure semantics. An end-to-end repeatability regression runs
the same analysis engine twice and verifies stable control flow,
symbols, counters, generated output, executable-address classification,
and preservation of the loaded memory image and XEX segment metadata.
Decoder regressions also cover operand fetches and positive or negative
relative branches that cross the 16-bit address-space boundary, exact
NMOS instruction/addressing-mode conformance for all 256 opcode values,
and defensive formatting of externally supplied instruction records.
Display-list regressions cover OS and ANTIC pointer discovery, blank and
mode-line decoding, LMS addresses, JMP/JVB control flow, malformed operands,
cycles, instruction limits, and ANTIC's 1 KiB display-list boundary.
Character-set regressions cover CHBASE alignment, 512-byte and 1 KiB layouts,
one-bit and two-bit glyph pixels, partial memory images, address-space bounds,
and integration with decoded text-mode display lists.
Deterministic arbitrary-input scenarios exercise analysis near every
major address-space boundary and verify stable output and internal
invariants. The complete native test suite is kept clean under MSVC
AddressSanitizer.
The library and header-only analysis pipeline are also clean under the
MSVC Native Recommended static-analysis rules. Full-address-space
visitation maps and the CLI memory image use heap storage rather than
large per-call stack frames.
This runtime is a
decompiler support model rather than a cycle-accurate Atari emulator;
hardware registers and external OS routines must be supplied through
the generated integration hooks.

A separate generated-code matrix emits and compiles all 151 official
NMOS 6502 opcodes across their declared addressing modes. This protects
the translation-unit backend from missing helpers and invalid operand
syntax that may not occur in the bundled `hello.xex` sample.
Every public header is also compiled in an isolated translation unit,
preventing accidental reliance on transitive includes.

The repository can also be opened directly as a CMake project in Visual
Studio.

## Roadmap

Graphical viewers for decoded display lists and charsets, screens, resources,
and a plugin architecture remain future application-layer work. The current
repository provides the analysis core and diagnostic CLI.
