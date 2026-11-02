# ATL → Workable Translation Engine Plan

## Goal
Make ATL a workable, runnable, robust translation engine with broad game compatibility (Proton/Wine-class quality bar). Target: buildable and runnable on this dev OS, with gaming performance, stability, resource efficiency, and broad compatibility including ARM native libraries (via FEX-Emu) and minimal GMS shim.

## Context
- Repo: `/home/ssit/Projects/Nadrox` (mirror of gitlab ATL)
- No build dir; build tools (cmake, meson, ninja, javac, ant, dx) missing from PATH
- Android SDK at `/opt/android-sdk`, NDK at `/opt/android-ndk`
- JVMs: java-17-openjdk, java-25-openjdk (no java-21 which CMakeLists.txt hardcodes)
- ATL: ART runtime + Java API impl + JNI→GTK4 bridge + bionic shim

## Decision: Scope
**Deliverable = Phases 1-6** (build + run + memory + reliability + performance + broad compatibility). Phase 6 includes FEX-Emu ARM translation and minimal GMS shim — both are implementation targets, not research-only. Full "all games" requires ARM translator + GMS shim; both are now in scope.

## Phase 1: Fix Build System

### 1.1 Install missing build tools
- Install: cmake, JDK (javac) via system package manager
- Note: full CMake rewrite eliminates meson, ninja, ant, make dependencies
- Still needed: dx (for .class→.dex conversion of api-impl.jar, gstub.jar) — from `/opt/android-sdk/build-tools/34.0.0`
- Add `/opt/android-sdk/build-tools/34.0.0` to PATH (dx, aapt)
- Ensure GTK4/Vulkan/WebKit/libportal dev libs present

### 1.2 Fix hardcoded Java home
- `CMakeLists.txt` lines ~227, ~237: replace `JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64` with `/usr/lib/jvm/java-25-openjdk`
- Rationale: user selected java-25; note this is untested with ART standalone's AOSP toolchain — if build fails, fall back to java-17-openjdk

### 1.3 Fix hardcoded x86_64 paths
- art_standalone copy commands use `lib64`, `linux-x86`
- Make architecture detection dynamic based on host CPU (uname -m)

### 1.4 Unified CMake build (full rewrite)
- Convert wolfSSL from autotools to CMake native (use wolfSSL's built-in CMakeLists.txt)
- Convert bionic_translation from meson to CMakeLists.txt
- Convert art_standalone from Makefile to CMakeLists.txt (use CMake's UseJava module)
- Convert ATL from meson to CMakeLists.txt
- Use `add_subdirectory()` for all components — no ExternalProject/shell wrappers
- Export proper CMake targets: `wolfssl::wolfssl`, `unwind::unwind`, `bionic::translation`, `art::standalone`, `atl::runtime`
- Remove autotools, meson, make dependencies entirely
- Incremental rebuilds work correctly via CMake target dependencies

## Phase 2: Make It Runnable

### 2.1 Fix RPATH / eliminate wrapper scripts
- Add `-Wl,-rpath,$ORIGIN/../lib` to link flags in meson.build
- Create `/usr/local/bin/atl` launcher (or use existing launch-atl.sh as template)
- Remove `LD_LIBRARY_PATH` dependency from run-atl.sh

### 2.2 Auto SDK detection
- Parse `AndroidManifest.xml` for `minSdkVersion`/`targetSdkVersion` in main.c
- Default to SDK 28 if not found

### 2.3 Validation
- `./atl app.apk` runs without wrapper scripts or LD_LIBRARY_PATH
- Installed binary works from any directory

## Phase 3: Memory & Reliability

### 3.1 Implement app-exit cleanup
- `app_exit()` in main.c: dlclose extracted .so libs, clear JNI global refs, destroy GTK widgets
- Currently `// TODO: do more cleanup?`

### 3.2 Fix busy-wait and debug output
- native_window.c: replace `while (width == 0)` spin loop with GTK realize/map signal
- Remove `printf("XXXXX:")` debug output
- Fix `wl_subcompositor` static assumption

### 3.3 Fix input threading
- Replace global `fixme_ugly_current_event` pipe-based event with proper GTK event controllers
- Fix multitouch support

### 3.4 Validation
- Memory returns to baseline after app exit
- No leaked GTK widgets, JNI refs, or dlopen'd libs
- No busy-wait loops or debug printf in hot paths

## Phase 4: Security & Stability

### 4.1 APK signature validation
- Enable `ATL_VALIDATE_CERTS` by default (or at least warn if disabled)

### 4.2 Crash reporting
- Add SIGSEGV/SIGABRT handlers for native lib crashes
- Better "Class not found" messages with suggestions

### 4.3 Validation
- 10+ diverse APKs launch without crashes
- Document compatibility results

## Phase 5: Performance (Gaming)

### 5.1 Rendering
- Default `ATL_DIRECT_EGL` on Wayland
- Frame pacing for SurfaceView
- AOT compile DEX via dex2oat at install time

### 5.2 Input
- Low-latency input path via GTK event controllers

### 5.3 Validation
- gles3jni runs at ≥ 30fps
- Input latency < 16ms

## Phase 6: Broad Game Compatibility (Implementation Target)

### 6.1 ARM native library support via FEX-Emu
- **Blocker**: Most games ship ARM .so (armeabi-v7a, arm64-v8a); ATL only loads host-arch libs
- **Solution**: Integrate FEX-Emu as ARM→x86_64 translation layer for native .so loading
- Integration: embed FEX as a loader; map bionic ABI through FEX's syscall layer to host Linux
- This enables Unity/Unreal/Cocos games with ARM native libraries
- Complexity: high — requires FEX build integration, ABI mapping, and testing

### 6.2 Google Play Services (GMS) — minimal shim
- **Scope**: Minimal shim (user decision). Return SUCCESS from `isGooglePlayServicesAvailable()`, stub sign-in/IAP/leaderboards/push/maps
- GMS features silently no-op; games that depend on them will launch but features won't work
- This lets many games launch that currently fail on GMS checks
- Much smaller effort than full GMS implementation

### 6.3 API coverage expansion
- Implement missing core APIs (PackageInstaller, JobScheduler, NotificationManager, AccountManager, Settings)
- RecyclerView, ViewPager, ConstraintLayout, Material Design widgets
- Camera2, MediaCodec improvements, ExoPlayer compat
- Room DB, ContentProvider, scoped storage, SAF

### 6.4 Validation
- Compatibility matrix: test 50+ games, document pass/fail
- Target: 80%+ of simple 2D games, 50%+ of 3D games (with FEX-Emu ARM translation)

## Data Flow
- User runs `atl app.apk` → launcher sets up env → main executable starts
- main.c → creates GtkApplication → parses cmdline → open() callback
- open() → builds classpath → JNI_CreateJavaVM → loads libtranslation_layer_main.so → sets up handle cache → creates Application → creates Activity → starts Activity
- Activity renders via GTK4 widgets; native .so libs loaded via bionic shim linker
- Input flows: GTK event → JNI → Android input pipeline → app

## Failure Modes
| Failure | Symptom | Mitigation |
|---------|---------|------------|
| Missing build tool | cmake/meson/ninja not found | Install via package manager; add SDK build-tools to PATH |
| Wrong Java version | JNI_CreateJavaVM fails or dx errors | Fall back to java-17-openjdk |
| Missing GTK4/Vulkan dev libs | pkg-config fails | Install dev packages per doc/Build.md |
| ART build fails | Make errors in art_standalone | Check Makefile arch detection; may need ARCH=x86_64 |
| App crashes on launch | NoSuchMethodError/NoSuchClassError | Decompile with jadx, add stubs |
| Native lib won't load | UnsatisfiedLinkError | Check if .so is ARM (not supported) |
| Memory leak on exit | RAM not freed | Implement app_exit cleanup |
| Busy-wait hang | CPU at 100% | Replace spin loops with signals |

## Migration Path
1. Current state: no build, no install, wrapper scripts required
2. After Phase 1: `cmake -B build && cmake --build build` works
3. After Phase 2: `atl app.apk` works without wrapper scripts
4. After Phase 3-4: stable, no leaks, no busy-waits
5. After Phase 5: performant for gaming
6. Phase 6+: implementation (FEX-Emu ARM, GMS shim)

## Implementation Order
1. Phase 1 (Build) → unblocks everything
2. Phase 2 (Runtime) → makes it usable
3. Phase 3-4 (Memory/Reliability) → makes it stable
4. Phase 5 (Performance) → makes it fast
5. Phase 6 (Compatibility) → implementation (FEX-Emu ARM + GMS)

## Open Questions (Need Resolution)

1. **JVM version**: RESOLVED — user selected java-25-openjdk. Risk: untested with ART standalone's AOSP toolchain. If build fails, fall back to java-17-openjdk.

2. **Build approach**: RESOLVED — user chose full CMake rewrite. Convert all components (wolfSSL, bionic, art_standalone, ATL) to native CMake with add_subdirectory. Eliminates autotools/meson/make dependencies.

3. **ARM strategy**: RESOLVED — user chose FEX-Emu for ARM→x86_64 translation. This enables running Unity/Unreal/Cocos games with ARM .so libs. Integration requires significant engineering: embed FEX as a loader for ARM native libs, map bionic ABI through FEX's syscall layer to host Linux. Mark as Phase 6 implementation (not research-only).

4. **GMS scope**: RESOLVED — user chose minimal shim. Return SUCCESS for availability checks, stub sign-in/IAP/leaderboards/push/maps. Games launch but GMS features no-op.

## Affected Files & Boundaries

### Phase 1 (Build)
- `CMakeLists.txt` — full rewrite to native CMake
- `thirdparty/wolfssl/` — CMakeLists.txt (use upstream's)
- `thirdparty/bionic_translation/` — new CMakeLists.txt (port from meson)
- `thirdparty/art_standalone/` — new CMakeLists.txt (port from Makefile)
- `meson.build` — replaced by CMakeLists.txt
- `src/api-impl/meson.build` — replaced by CMakeLists.txt

### Phase 2 (Runtime)
- `src/main-executable/main.c` — RPATH, auto SDK detection, launcher
- `meson.build` — RPATH link flags (or CMake equivalent)

### Phase 3-4 (Memory/Reliability)
- `src/main-executable/main.c` — app_exit cleanup
- `src/libandroid/native_window.c` — busy-wait removal, debug output, wl_subcompositor fix
- `src/libandroid/input.c` — input threading fix, multitouch

### Phase 5 (Performance)
- `src/main-executable/main.c` — ATL_DIRECT_EGL default
- `src/libandroid/native_window.c` — frame pacing

### Phase 6 (Compatibility)
- `src/api-impl/` — new Java API implementations
- `src/api-impl-jni/` — new JNI bridges
- New: FEX-Emu integration layer for ARM .so loading
- New: minimal GMS shim (gstub.jar enhancement)

## Subagent Division

| Subagent | Scope | Files | Dependencies |
|----------|-------|-------|-------------|
| **A: Build System** | Phase 1 | CMakeLists.txt, thirdparty/*/CMakeLists.txt | None (first) |
| **B: Runtime** | Phase 2 | main.c, launcher | Phase 1 |
| **C: Memory & Reliability** | Phase 3-4 | main.c, native_window.c, input.c | Phase 1 |
| **D: Performance** | Phase 5 | main.c, native_window.c | Phase 3 |
| **E: Compatibility** | Phase 6 | api-impl/, api-impl-jni/, FEX, GMS | Phase 1-4 |

### Execution Order
1. Subagent A first (unblocks all others)
2. Subagents B, C in parallel (independent after A)
3. Subagent D after C (depends on input/rendering fixes)
4. Subagent E last (depends on stable base)

### Interface Contracts
- A→B: CMake targets exported (`atl::runtime`, `art::standalone`)
- A→C: build produces runnable binary for testing
- C→D: input/rendering APIs stabilized
- All→E: stable base for API expansion

## Validation Plan
After each phase:
- Build from clean checkout
- Run test APK suite (Gravity Defied, gles3jni)
- Measure memory before/after app exit
- Run clang-format on changed files
- Document compatibility changes
