# ATL Gaming-Ready Independent Fork — Implementation Plan

## Goal
Run Android games from APKs directly (`atl game.apk`) with:
- **GMS shim**: Games launch without "update Play Services" dialog
- **ARM support**: Unity/Unreal/Godot games via FEX-Emu ARM→x86_64 translation
- **Gaming performance**: Frame pacing, <16ms input latency, clean memory on exit
- **Technical independence**: Pure CMake build, no wrapper scripts, own CI/CD, documented sync strategy

---

## Current State (Done)
- ✅ Phase 2: True Unified CMake Build System
  - wolfSSL (native CMake)
  - bionic_translation (ported from meson)
  - art_standalone (wrapped Makefile)
  - android_translation_layer (ported from meson)
  - `cmake -B build && cmake --build build` works

---

## Phase 3: Runtime Polish (Foundation — 1-2 weeks)
*Must complete before gaming stack works reliably*

### 3.1 RPATH + Eliminate Wrapper Scripts
- [ ] Add `-Wl,-rpath,$ORIGIN/../lib` to all link flags in `atl/CMakeLists.txt`
- [ ] Create `src/main-executable/launcher.c` that sets up env and execs main binary
- [ ] Install to `bin/atl` with embedded library paths
- [ ] Verify: `atl app.apk` works from any directory without `LD_LIBRARY_PATH`

### 3.2 Auto SDK Detection
- [ ] Parse `AndroidManifest.xml` for `minSdkVersion`/`targetSdkVersion` in `main.c`
- [ ] Default to SDK 28 if missing
- [ ] Remove `--sdk-int` requirement

### 3.3 Memory & Reliability Cleanup
- [ ] Implement `app_exit()` in `main.c`: dlclose all extracted `.so`, `DeleteGlobalRef()` for all JNI refs, destroy GTK widgets, reset globals
- [ ] Replace busy-wait in `native_window.c` with GTK realize/map signal
- [ ] Remove `printf("XXXXX:")` debug output
- [ ] Fix input threading: replace global `fixme_ugly_current_event` with GTK event controllers, add multitouch support

---

## Phase 4A: Minimal GMS Shim (1 week)
*Java stubs only — no native code*

### 4A.1 Core Availability Checks
- [ ] `GoogleApiAvailability.isGooglePlayServicesAvailable()` → return `SUCCESS (0)`
- [ ] `GooglePlayServicesUtil.isGooglePlayServicesAvailable()` → return `SUCCESS (0)`

### 4A.2 Auth & Games
- [ ] `GoogleSignIn.getClient()` / `GoogleSignInAccount` → stub returning null/empty
- [ ] `Games.getLeaderboardsClient()` / `Games.getAchievementsClient()` → stub clients with no-op methods

### 4A.3 Billing & Analytics
- [ ] `BillingClient.startConnection()` → callback `onBillingSetupFinished(BillingResult.OK)`
- [ ] `FirebaseAnalytics.getInstance()` → no-op instance
- [ ] `MapsInitializer.initialize()` → no-op

### 4A.4 Build Integration
- [ ] Add new Java files to `atl/CMakeLists.txt` compile_java_to_jar()
- [ ] Place under `src/api-impl/com/google/android/gms/**`, `src/api-impl/com/android/billingclient/**`, `src/api-impl/com/google/firebase/**`

### 4A.5 Validation
- [ ] Test 5+ GMS-dependent games launch without "update Play Services" dialog
- [ ] Verify no crashes on sign-in/IAP/leaderboard calls

---

## Phase 4B: ARM Support via FEX-Emu (3-5 weeks)
*Enables 95%+ of Android games (Unity, Unreal, Godot, Cocos)*

### 4B.1 FEX-Emu Integration
- [ ] Add `FetchContent` for FEX-Emu in root `CMakeLists.txt`
- [ ] Build FEX as part of CMake configure (self-contained, no submodule)
- [ ] Export `fex::fex` CMake target

### 4B.2 Bionic Linker Hook
- [ ] Modify `src/libandroid/linker/linker.c`:
  - Detect ELF machine type (`EM_ARM` / `EM_AARCH64`) when loading `.so`
  - If ARM: invoke FEX-Emu to translate and execute
  - If x86_64: load normally (current path)
- [ ] Map bionic ABI through FEX's syscall layer to host Linux
- [ ] Handle ARM guest memory in host process

### 4B.3 Validation
- [ ] Load `libunity.so` (ARM64) from Unity game
- [ ] Load `libil2cpp.so` (ARM64) from Unreal/Godot game
- [ ] Measure overhead: target < 2x native x86_64 performance
- [ ] Test 3+ ARM-only games launch and render

---

## Phase 4C: Gaming Performance (1-2 weeks)

### 4C.1 Frame Pacing / SurfaceView VSync
- [ ] Hook `ANativeWindow_lock`/`unlockAndPost` in `native_window.c` to sync with display refresh
- [ ] Use `wl_surface_frame` callback (Wayland) or `GLX_EXT_swap_control` (X11 fallback)
- [ ] Target: steady 60fps, no tearing

### 4C.2 Complete App-Exit Cleanup
- [ ] `dlclose()` all extracted `.so` libs on `app_exit()`
- [ ] Clear JNI global refs: `DeleteGlobalRef()` for all cached classes/objects
- [ ] Destroy GTK widgets, free `app_data_dir` strings
- [ ] Reset global state (window, handle cache, input queue)
- [ ] Verify RSS returns to baseline (< 50MB overhead)

### 4C.3 Input Latency
- [ ] Replace global `fixme_ugly_current_event` with per-event allocation
- [ ] Implement multitouch: track `pointer_id` per touch point
- [ ] Use GTK `GtkGestureTouch` / `GtkEventControllerMotion` for low-latency path
- [ ] Target: < 16ms input latency

---

## Phase 4D: Distribution & Technical Independence (1 week)

### 4D.1 System Install + Packaging
- [ ] `cmake --install build` → `/usr/local` with `bin/atl`, `lib/atl/*.so`, `share/applications/atl.desktop`
- [ ] GitHub Actions CI: build on Ubuntu, test APK suite, publish `.deb`, `.rpm`, AppImage
- [ ] Cross-distro test: Arch, Debian/Ubuntu, Fedora, Alpine

### 4D.2 Documentation & Sync Strategy
- [ ] `SYNC_STRATEGY.md`: How to merge upstream `master` regularly, document fork-specific changes
- [ ] `COMPATIBILITY.md`: Game compatibility matrix (pass/fail with notes)
- [ ] `CONTRIBUTING.md`: How to add API stubs, widget implementations
- [ ] `ARCHITECTURE.md`: Updated for unified CMake build

---

## Phase 5: Inspiration Polish (Ongoing)
*Make the fork something others want to build on*

### 5.1 Developer Experience
- [ ] Improved logcat-style output with filtering
- [ ] Crash report generation with native stack traces
- [ ] GDB/LLDB debugging documentation

### 5.2 Upstream Contribution Back
- [ ] Identify bug fixes and API implementations suitable for upstream
- [ ] Submit PRs to gitlab.com/android_translation_layer/android_translation_layer

---

## Execution Order & Dependencies

```
Phase 3 (Runtime) → Phase 4A (GMS) → Phase 4B (FEX) → Phase 4C (Perf) → Phase 4D (Distro)
                           ↘ Phase 4B (FEX) ↗
```

- Phase 3 unblocks all gaming work
- Phase 4A (GMS) and 4B (FEX) can run in parallel after Phase 3
- Phase 4C needs stable base from 3+4A+4B
- Phase 4D last, after stable gaming experience

---

## Validation Gates (After Each Phase)

1. **Clean build**: `cmake -B build && cmake --build build -j8`
2. **Test suite**: Gravity Defied, gles3jni, + GMS games, + ARM games (when ready)
3. **Metrics**: FPS (steady 60), RSS delta on exit (< 50MB), input latency (< 16ms)
4. **Code quality**: `clang-format` on changes

---

## Key Files to Modify

| Area | Files |
|------|-------|
| Runtime | `src/main-executable/main.c`, `src/main-executable/launcher.c`, `atl/CMakeLists.txt` |
| Linker/ARM | `src/libandroid/linker/linker.c`, `CMakeLists.txt` (FetchContent) |
| GMS | `src/api-impl/com/google/android/gms/**`, `src/api-impl/com/android/billingclient/**`, `atl/CMakeLists.txt` |
| Performance | `src/libandroid/native_window.c`, `src/libandroid/input.c`, `src/main-executable/main.c` |
| Packaging | `.github/workflows/`, `packaging/` |

---

## Out of Scope (Future)
- Google Play Services full implementation
- ARM32 (armeabi-v7a) — FEX handles both ARM32/ARM64
- macOS/BSD support
- Full GMS (Maps, Drive, Fit, etc.) — game-driven stub additions only