# ATL Next Stage: Gaming-Ready Translation Layer

## Goals
1. **Minimal GMS shim** — just enough for games to launch (availability checks, stubbed sign-in/IAP/leaderboards)
2. **ARM native lib support via FEX-Emu** — translate ARM `.so` → x86_64 on your x86_64 host (no ARM hardware needed)
3. **Gaming performance** — frame pacing, memory cleanup, input latency < 16ms
4. **Broad Linux compatibility** — RPATH, system install, no wrapper scripts

---

## Phase A: Minimal GMS Shim (Subagent A)

### Scope
Only implement what's strictly required for games to **launch** and **not crash** on GMS checks.

### Implementation
| Component | What to implement | Where |
|-----------|-------------------|-------|
| `GoogleApiAvailability.isGooglePlayServicesAvailable()` | Return `SUCCESS (0)` | `src/api-impl/com/google/android/gms/common/GoogleApiAvailability.java` |
| `GooglePlayServicesUtil.isGooglePlayServicesAvailable()` | Return `SUCCESS (0)` | `src/api-impl/com/google/android/gms/common/GooglePlayServicesUtil.java` |
| `GoogleSignIn.getClient()` / `GoogleSignInAccount` | Stub returning null/empty | `src/api-impl/com/google/android/gms/auth/api/signin/` |
| `Games.getLeaderboardsClient()` / `Games.getAchievementsClient()` | Return stub clients with no-op methods | `src/api-impl/com/google/android/gms/games/` |
| `BillingClient` | Stub: `startConnection()` → callback `onBillingSetupFinished(BillingResult.OK)` | `src/api-impl/com/android/billingclient/api/` |
| `FirebaseAnalytics.getInstance()` | Return no-op instance | `src/api-impl/com/google/firebase/analytics/` |
| `MapsInitializer.initialize()` | No-op | `src/api-impl/com/google/android/gms/maps/` |

### Build integration
- Add new Java files to `src/api-impl/meson.build` (or CMakeLists.txt after rewrite)
- No native code needed — all Java stubs

### Validation
- Test with 5+ GMS-dependent games (launch without "update Play Services" dialog)
- Verify no crashes on sign-in/IAP/leaderboard calls

---

## Phase B: ARM Support via FEX-Emu (Subagent B)

### Why FEX-Emu
- Runs on x86_64 host, translates ARM32/ARM64 → x86_64
- No ARM hardware needed
- Open source, actively maintained, good performance for games

### Integration approach
1. **Build FEX-Emu** as part of CMake build (or download pre-built)
2. **Hook into bionic linker** (`src/libandroid/linker/linker.c`):
   - When loading a `.so`, detect ELF machine type (`EM_ARM` / `EM_AARCH64`)
   - If ARM, invoke FEX-Emu to translate and execute
   - If x86_64, load normally (current path)
3. **Syscall translation** — FEX handles Linux syscall ABI differences
4. **Memory mapping** — FEX manages ARM guest memory in host process

### Files to modify
- `src/libandroid/linker/linker.c` — ARM detection + FEX invocation
- `CMakeLists.txt` — add FEX-Emu as dependency (submodule or fetchcontent)
- `src/main-executable/bionic_compat.c` — ensure bionic syscalls work through FEX

### Complexity: HIGH
- Estimated 2-4 weeks for basic integration
- Alternative: **QEMU user-mode** (simpler, slower) for initial testing

### Validation
- Load `libunity.so` (ARM64) from a Unity game
- Load `libil2cpp.so` (ARM64) from Unreal/Godot game
- Measure overhead: target < 2x native x86_64 performance

---

## Phase C: Gaming Performance (Subagent C)

### C1: Frame Pacing / SurfaceView VSync
**File:** `src/libandroid/native_window.c`
- Hook `ANativeWindow_lock`/`unlockAndPost` to sync with display refresh
- Use `wl_surface_frame` callback (Wayland) or `GLX_EXT_swap_control` (X11)
- Target: steady 60fps, no tearing

### C2: Complete App-Exit Cleanup
**File:** `src/main-executable/main.c`
- `dlclose()` all extracted `.so` libs on `app_exit()`
- Clear JNI global refs: `DeleteGlobalRef()` for all cached classes/objects
- Destroy GTK widgets, free `app_data_dir` strings
- Reset global state (`window`, handle cache, input queue)

### C3: Input Latency
**File:** `src/libandroid/input.c`
- Replace global `fixme_ugly_current_event` with per-event allocation
- Implement multitouch: track `pointer_id` per touch point
- Use GTK `GtkGestureTouch` / `GtkEventControllerMotion` for low-latency path

### C4: Memory Profiling
- Add `mallinfo()` / `/proc/self/status` logging on app exit
- Verify RSS returns to baseline (< 50MB overhead)

---

## Phase D: Distribution & Compatibility (Subagent D)

### D1: RPATH + System Install
- Add `-Wl,-rpath,$ORIGIN/../lib` to all link flags (CMakeLists.txt / meson.build)
- `cmake --install build` → `/usr/local` with:
  - `bin/atl` (launcher with embedded paths)
  - `lib/atl/*.so`, `lib/atl/java/dex/...`
  - `share/applications/atl.desktop`

### D2: Auto SDK Detection
**File:** `src/main-executable/main.c`
- Parse `AndroidManifest.xml` (`minSdkVersion`, `targetSdkVersion`)
- Default to 28 if missing

### D3: Cross-Distro Testing
- Test on: Arch, Debian/Ubuntu, Fedora, Alpine
- Package: `.deb`, `.rpm`, Flatpak, AppImage (CI)

---

## Subagent Division

| Subagent | Scope | Key Files | Dependencies |
|----------|-------|-----------|--------------|
| **A: GMS** | Minimal GMS shim | `src/api-impl/com/google/android/gms/**`, `src/api-impl/com/android/billingclient/**`, meson.build | None |
| **B: ARM/FEX** | FEX-Emu integration + bionic linker hook | `src/libandroid/linker/linker.c`, CMakeLists.txt, bionic_compat.c | Phase A (for GMS in ARM games) |
| **C: Perf** | Frame pacing, cleanup, input | `native_window.c`, `main.c`, `input.c` | Phase A (test with games) |
| **D: Distro** | RPATH, install, auto-SDK, packaging | CMakeLists.txt, main.c, CI configs | Phase C (stable base) |

---

## Execution Order

1. **Subagent A** (GMS) — 1 week, unblocks testing with real games
2. **Subagent B** (ARM/FEX) — 2-4 weeks, parallel with C
3. **Subagent C** (Performance) — 1-2 weeks, parallel with B
4. **Subagent D** (Distribution) — 1 week, after C stable

---

## Decision: ARM vs x86-only

**RESOLVED — FEX-Emu via FetchContent.** Translates ARM32/ARM64 → x86_64 on your x86_64 host. Enables 95%+ of Android games (Unity, Unreal, Cocos, Godot). No ARM hardware needed.

## Decision: GMS Scope

**RESOLVED — Minimal shim with game-driven additions.** Core: availability checks (SUCCESS), stubbed sign-in/IAP/leaderboards/Maps/Firebase. If specific games fail on missing GMS APIs, add those stubs on-demand.

## Decision: FEX-Emu Build Integration

**RESOLVED — CMake FetchContent.** Auto-downloads and builds FEX-Emu as part of CMake configure. Self-contained, no manual submodule management.

---

## Open Questions (Need Resolution)

1. **Frame pacing priority**: RESOLVED — Wayland-first (wl_surface_frame). X11 fallback later if needed.
2. **Distribution target**: RESOLVED — Native packages first (.deb, .rpm, AppImage). Flatpak later.

---

## Validation Gates

After each subagent:
- Build from clean checkout: `cmake -B build && cmake --build build -j8`
- Run test suite: Gravity Defied, gles3jni, + 3 GMS games, + 2 ARM games (if Phase B done)
- Measure: FPS (steady 60), RSS delta on exit (< 50MB), input latency (< 16ms)
- Run clang-format on changes