# Plan: Self-Contained ARM Translation Layer (No External Dependencies)

## Clarification: Translation vs. Emulation

This project is a **translation layer**, not an emulator. The distinction:

- **Emulation** (e.g., QEMU interpreter): Runs foreign instructions one-by-one on the host CPU via software interpretation. Slow, no native code execution.
- **Translation** (e.g., Wine, box86, this project): Converts foreign binary code to native host code (JIT or AOT), then executes natively on the host CPU. The `src/translation/` engine already does this: ARM instructions → MIR → x86_64 native code.

The box86/box64 components are also translators (dynamic recompilers), not emulators. But they are **external** components. The user wants a single self-contained translation layer with zero external dependencies — everything handled internally.

## Goal

Remove all external box86/box64 dependencies and extend the custom ARM translation engine (`src/translation/`) to natively load, relocate, and JIT-translate ARM ELF shared libraries (`.so` files) bundled in APKs. The result is one unified translation layer: Android ARM → Linux x86_64, with no external translation components.

## Scope

### 1. Remove box86/box64
- Delete find-package / FetchContent blocks from root `CMakeLists.txt`.
- Remove `BOX86_*` / `BOX64_*` from `atl/CMakeLists.txt` link lists.
- Remove `add_dependencies(all_build box86 box64)`.

### 2. New Translation Subsystem: `src/translation/elf/`
Add ELF loader + dynamic linker + JIT bridge reusing the existing MIR pipeline.

**New files:**
- `elf_loader.h/.c` — ELF32/ELF64 parser, segment loading, memory mapping
- `elf_reloc.h/.c` — Relocation processing, symbol resolution
- `elf_dynlink.h/.c` — Dynamic linker: symbol table, GOT/PLT, `dlopen`/`dlsym`
- `elf_jit.h/.c` — JIT bridge: ARM function bytes → MIR → x86_64 code cache

### 3. ELF Loader
- Parse ELF header (EI_CLASS, EI_DATA, EM_ARM/EM_AARCH64)
- Load PT_LOAD segments into executable memory (mmap PROT_EXEC)
- Parse dynamic section (DT_NEEDED, DT_SYMTAB, DT_STRTAB, DT_RELA/DT_REL, DT_PLTGOT, DT_INIT/DT_FINI)
- Handle SONAME, RPATH/RUNPATH

### 4. Dynamic Linker
- `atl_dlopen`, `atl_dlsym`, `atl_dlclose` — minimal dynamic linker
- Resolve symbols against loaded libraries (libandroid, libart, bionic, etc.)
- GOT/PLT setup with lazy binding via PLT trampolines

### 5. JIT Bridge
- ARM function → MIR (reuse `arm_decode`) → x86_64 (reuse `lower_x86_64`) → code cache (reuse `jit_cache`)
- PLT entries point to JIT stubs; translate on first call

### 6. Integration
- Hook `dlopen`/`dlsym` in `bionic_compat.c` to route ARM .so loads through new ELF loader
- Reuse `signal_emu` for SIGSEGV during translated code execution

### 7. CMake Changes
- Add `elf/` sources to `src/translation/CMakeLists.txt`
- Remove box86/box64 from all CMake files

## Resolved Decisions

1. **32-bit first**: ELF32 (armeabi-v7a/Thumb-2). 64-bit (arm64-v8a) in later phase.
2. **Code cache**: 256MB initial mmap region, growable. On-demand translation, not all-at-once.
3. **Lazy translation**: PLT trampolines trigger JIT on first call (matches Wine's approach).
4. **Thumb-2**: Already supported by existing decoder (`arm_decode.c`).
5. **No external deps**: Only bionic_translation and art_standalone stubs remain (these are API translation, not CPU emulation).
6. **Integration point**: Hook dlopen/dlsym in bionic_compat.c.

## Out of Scope
- 64-bit ELF (arm64-v8a)
- TLS (thread-local storage)
- Symbol versioning (VERSYM/VERDEF/VERNEED)
- GNU hash tables (use standard ELF hash)
- C++ exception handling across translated code
- Debug info (DWARF)
- GNU indirect functions (IFUNC)

## Validation Plan
1. Build succeeds with zero box86/box64 references.
2. Load a simple ARM .so (armeabi-v7a) and call a function.
3. Load a real APK .so (Temple Run's libTR.so) and reach main game loop without SIGILL.
4. Existing translation tests (`tests/arm_translation/`) still pass.

## Goal
Remove all external box86/box64 dependencies and extend the custom ARM translation engine (`src/translation/`) to natively load, relocate, and JIT-translate ARM ELF shared libraries (`.so` files) bundled in APKs. This makes the layer fully self-contained — no external emulation components.

## Scope

### 1. Remove box86/box64
- Delete the box86/box64 find-package / FetchContent blocks from `CMakeLists.txt` (root).
- Remove `BOX86_*` / `BOX64_*` variables from `atl/CMakeLists.txt` and from `target_link_libraries` lists.
- Remove `add_dependencies(all_build box86 box64)`.

### 2. New Translation Subsystem: `src/translation/elf/`
Add a new ELF loader + dynamic linker + JIT bridge that reuses the existing MIR pipeline.

**New files:**
- `src/translation/elf/elf_loader.h/.c` — ELF32/ELF64 parser, section/segment loading, memory mapping
- `src/translation/elf/elf_reloc.h/.c` — Relocation processing (RELA/REL), symbol resolution
- `src/translation/elf/elf_dynlink.h/.c` — Dynamic linker: symbol table, GOT/PLT setup, `dlopen`/`dlsym` emulation
- `src/translation/elf/elf_jit.h/.c` — JIT bridge: takes ARM function bytes → MIR → x86_64 code cache
- `src/translation/elf/elf_cache.h/.c` — Code cache for translated .so functions (reuses `jit_cache` infrastructure)

### 3. ELF Loader Design
- Parse ELF header (EI_CLASS 32/64, EI_DATA little-endian, EM_ARM/EM_AARCH64)
- Load PT_LOAD segments into executable memory (mmap with PROT_EXEC)
- Parse dynamic section (DT_NEEDED, DT_SYMTAB, DT_STRTAB, DT_RELA/DT_REL, DT_PLTGOT, DT_INIT/DT_FINI)
- Handle SONAME, RPATH/RUNPATH for dependency resolution

### 4. Dynamic Linker Design
- Implement a minimal dynamic linker (`atl_dlopen`, `atl_dlsym`, `atl_dlclose`) that:
  - Loads ARM .so files into the process
  - Resolves symbols against already-loaded libraries (libandroid, libart, bionic, etc.)
  - Sets up GOT entries pointing to translated function stubs
  - Handles lazy binding via PLT trampolines that trigger JIT on first call

### 5. JIT Bridge Design
- For each ARM function in the .so:
  - Decode ARM instructions → MIR (reuse `arm_decode`)
  - Lower MIR → x86_64 (reuse `lower_x86_64`)
  - Emit executable code into code cache (reuse `jit_cache`)
- PLT entries point to JIT stubs; on first call, translate and patch GOT

### 6. Integration with Existing Engine
- Hook `dlopen`/`dlsym` in bionic compat layer (`src/main-executable/bionic_compat.c`) to route ARM .so loads through the new ELF loader instead of the host's `dlopen`
- Register translated functions in the existing JNI table / syscall table as needed
- Reuse `signal_emu` for SIGSEGV handling during translated code execution

### 7. CMake Changes
- Add `elf/` sources to `src/translation/CMakeLists.txt`
- Add include directories for `elf/`
- Remove `box86`/`box64` from `atl/CMakeLists.txt` link lists (already empty after removal)
- Update `all_build` target to not depend on box86/box64

## Risk Areas
- **ELF complexity**: Full ELF dynamic linking is large (relocations, symbol versioning, TLS, init/fini arrays). Scope to what APK .so files actually need.
- **ARM instruction coverage**: The existing decoder must cover all ARM instructions used by real game .so files (NEON, VFP, Thumb-2). Gaps will cause SIGILL.
- **Performance**: JIT caching is critical; cold-start translation of large .so files (e.g., GTA's 50MB libGTASA.so) will be slow.
- **Memory**: Code cache must be sized appropriately; use mmap with MAP_JIT on macOS (not needed on Linux).

## Validation Plan
1. Build succeeds with no box86/box64 references.
2. Load a simple ARM .so (e.g., `libhello.so` compiled for armeabi-v7a) and call a function.
3. Load a real APK's .so (e.g., Temple Run's `libTR.so`) and verify it reaches main game loop without SIGILL.
4. Run existing translation tests (`tests/arm_translation/`) — must still pass.

## Resolved Decisions

1. **32-bit first**: Start with ELF32 (armeabi-v7a/Thumb-2). 64-bit (arm64-v8a) added in a later phase.
2. **Code cache sizing**: Use mmap with 256MB initial region, growable. Large .so files (50MB+) will have their code sections translated on-demand, not all at once.
3. **Lazy translation**: Yes — PLT trampolines trigger JIT on first call. This matches Wine's approach and minimizes startup time.
4. **Thumb-2**: Already supported by existing decoder (confirmed in `arm_decode.c`).
5. **Integration point**: Hook `dlopen`/`dlsym` in `bionic_compat.c` to route ARM .so loads through the new ELF loader.
6. **No external deps**: Remove box86/box64 entirely. The only remaining external deps are bionic_translation and art_standalone (stubs).

## Phased Implementation

### Phase 1: Remove box86/box64
- Clean CMakeLists.txt (root + atl)
- Remove BOX86/BOX64 references
- Verify build still succeeds

### Phase 2: ELF Loader Core
- Implement ELF32 parser (header, segments, sections)
- Load PT_LOAD segments into executable memory
- Parse dynamic section
- Basic `atl_dlopen` / `atl_dlclose`

### Phase 3: Dynamic Linker
- Symbol table parsing (DT_SYMTAB, DT_STRTAB)
- Relocation processing (R_ARM_JUMP_SLOT, R_ARM_GLOB_DAT, R_ARM_RELATIVE)
- GOT/PLT setup
- `atl_dlsym` implementation
- Dependency resolution (DT_NEEDED)

### Phase 4: JIT Bridge
- ARM function → MIR → x86_64 code cache
- PLT trampolines for lazy translation
- Integration with existing `jit_cache`

### Phase 5: Integration & Testing
- Hook dlopen/dlsym in bionic_compat.c
- Test with simple ARM .so
- Test with real APK .so (Temple Run)
- Run existing translation tests

## Out of Scope (for now)
- 64-bit ELF (arm64-v8a)
- TLS (thread-local storage)
- Symbol versioning (VERSYM/VERDEF/VERNEED)
- GNU hash tables (use standard ELF hash)
- C++ exception handling across translated code
- Debug info (DWARF)
- GNU indirect functions (IFUNC)
