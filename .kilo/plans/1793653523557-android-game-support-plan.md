# Nadrox ATL — Android Game Support Implementation Plan

## Goal
Enable running **32-bit ARM Android games (e.g., GTA)** with **functional Google Play Services (GMS)** and **modern graphics (GLES 3.x / Vulkan)** on Linux x86_64.

---

## Executive Summary of Required Work

| Area | Current State | Target | Effort |
|------|---------------|--------|--------|
| **ARM Translation** | FEX-Emu builds but wrong direction (x86_64→aarch64) | **box86/box64** for ARM→x86_64 user-space translation | High |
| **GMS** | Minimal mock stubs (always return SUCCESS/test data) | **microG** integration for functional Play Services | High |
| **Graphics** | GLES 1.x via Android GLES10 wrapper; Vulkan stubs | **GLES 3.x + Vulkan** via Mesa/VirGL + ANGLE | High |
| **Linker/Loader** | ARM detection removed; no translation hook | Restore ARM ELF detection + box86/box64 invocation | Medium |
| **Input/Window** | GTK4 + Wayland works | Extend for gamepad, multi-touch, raw input | Low-Medium |

---

## 1. ARM Translation Layer — Decision: **box86 + box64**

### Why not FEX-Emu?
FEX-Emu translates **x86_64 guest → aarch64 host**. It cannot translate ARM→x86_64 on an x86_64 host.

### Why box86/box64?
- **box86**: 32-bit ARM (armeabi-v7a) → x86_64 user-space translation
- **box64**: 64-bit ARM (arm64-v8a) → x86_64 user-space translation
- Mature, actively maintained, used by Lutris/Boxtron/Proton
- LGPL-2.1 (compatible with dynamic linking)

### Integration Approach
```
APK native lib (ARM) 
    → linker detects ELF machine = EM_ARM/EM_AARCH64
    → sets FLAG_ARM_LIBRARY 
    → invokes box86/box64 to translate & execute
    → returns translated x86_64 code to process
```

### Tasks
1. **Add box86/box64 as FetchContent dependencies** (or system packages)
2. **Restore ARM ELF detection in `linker.c:apkenv_load_library()`** (lines 1377-1430 were removed)
3. **Implement `xr_lazy_call` replacement** → call box86/box64 translation API
4. **Add box86/box64 library loading** in `bionic_translation` init
5. **Handle ARM syscalls / signal translation** (box86 does most; hook remaining)

---

## 2. Google Play Services — Decision: **microG**

### Why microG?
- Free/open-source implementation of GMS Core
- Provides: GSF (Google Services Framework), UnifiedNlp (location), Maps API v1/v2 stubs
- Used by /e/OS, LineageOS, GrapheneOS
- Apache-2.0 license

### Alternative: Re-implement from scratch
- Not recommended: millions of lines, constant API changes
- microG already implements: Sign-in, Games, Maps, Firebase, GCM/FCM, Nearby, Drive, Wallet, etc.

### Integration Approach
```
ATL JNI stubs (current mocks)
    → Replace with microG JNI bindings
    → microG talks to its own backend (or Google's if user signs in)
    → Returns real tokens / data to app
```

### Tasks
1. **Add microG as Git submodule** (or FetchContent from `https://github.com/microg/GmsCore`)
2. **Build microG native libraries** (libmicrog_jni.so, etc.) via its Gradle/Gradle-native build
3. **Replace `src/api-impl/com/google/android/gms/**` stubs** with microG JNI wrappers
4. **Implement microG backend services** (UnifiedNlp for location, GSF for GCM/FCM)
5. **Add microG settings UI** (account management, location providers)
6. **Handle signature spoofing** (microG needs `FAKE_SIGNATURE` permission)

---

## 3. Graphics Stack — Decision: **Mesa + VirGL + ANGLE**

### Current State
- GLES 1.x only via Android's `GLES10` Java wrapper
- `GLImpl.java` has 1981 lines of `native` methods, mostly unimplemented
- Vulkan linked but no ICD/loader integration
- EGL/Wayland: basic window surface only

### Target Architecture
```
Game (GLES 3.x / Vulkan)
    → ATL JNI (GLImpl / EGLImpl)
    → ANGLE (GLES → Vulkan translation) OR native Mesa GLES
    → VirGL (virtio-GPU) OR native DRM/KMS
    → Host GPU (Mesa drivers: radv, radeonsi, intel, nouveau)
```

### Technology Choices
| Layer | Choice | Rationale |
|-------|--------|-----------|
| **GLES 3.x** | **ANGLE** (GLES → Vulkan) | Proven in Chrome/Android; supports ES 3.2 |
| **Vulkan** | **Native Mesa ICD** | Direct Vulkan 1.3 support; no translation needed |
| **EGL** | **Mesa EGL + Wayland platform** | Already have Wayland; Mesa EGL is mature |
| **Window System** | **Wayland + DRM/KMS** | Current GTK4/Wayland setup works |

### Tasks
1. **Add ANGLE as FetchContent** (from `https://chromium.googlesource.com/angle/angle`)
2. **Replace `GLImpl.java` native methods** → call ANGLE EGL/GLES entry points
3. **Implement `EGLImpl.java`** → Mesa EGL + Wayland platform extensions
4. **Add Vulkan ICD loader** → `vulkan-1.so` → Mesa `radv`/`radeonsi`/etc.
5. **Implement `ANativeWindow` → Wayland surface** mapping (partially done in `native_window.c`)
6. **Add frame pacing** → `wl_surface_frame` + presentation feedback (partially done)
7. **Handle multi-window / surface management** for games

---

## 4. Linker / Loader Fixes (Prerequisite for ARM Translation)

### Issues from Code Review
- `apkenv_load_library()`: ARM detection code **removed** (lines 1377-1430)
- `FLAG_ARM_LIBRARY` defined but never set/checked
- `native_window.c`: `xr_func` typedef invalid; `__builtin_va_arg_pack` unsupported
- `linker.c`: `bytes` memory leak on fail paths (fixed but verify)

### Tasks
1. **Restore ARM ELF detection** in `apkenv_load_library()`:
   ```c
   ElfW(Ehdr) *ehdr = (ElfW(Ehdr) *)bytes;
   bool is_arm = (ehdr->e_machine == EM_ARM || ehdr->e_machine == EM_AARCH64);
   if (is_arm) si->flags |= FLAG_ARM_LIBRARY;
   ```
2. **Hook `FLAG_ARM_LIBRARY`** → invoke box86/box64 translation before `apkenv_init_library()`
3. **Fix `native_window.c:510`** → `typedef XrResult (*xr_func)(void *, ...);`
4. **Fix `native_window.c:523`** → Remove `__builtin_va_arg_pack` (already done, verify)
5. **Verify `free(bytes)`** in all fail paths

---

## 5. Input / Window System Enhancements

### Current
- GTK4 `GtkGestureClick` for touch (single-touch only)
- `GtkEventControllerMotion` for mouse hover
- Wayland keyboard via `gtk4-layer-shell` (optional)

### Needed for Games
- **Multi-touch** (GtkGestureSingle or raw libinput)
- **Gamepad/joystick** (libinput + SDL2 gamecontrollerdb)
- **Raw mouse** (relative motion for FPS camera)
- **Virtual keyboard** for text input (already have `gtk4-layer-shell`)

### Tasks
1. Replace `GtkGestureClick` → `GtkGestureSingle` for multi-touch
2. Add `libinput` raw event handling for gamepads
3. Add SDL2 gamecontrollerdb mapping
4. Implement relative mouse mode (pointer lock)

---

## 6. Build System & Dependencies

### New FetchContent Dependencies
```cmake
# box86/box64
FetchContent_Declare(box86 GIT_REPOSITORY https://github.com/box86/box86 GIT_TAG v0.3.0)
FetchContent_Declare(box64 GIT_REPOSITORY https://github.com/box86/box64 GIT_TAG v0.3.0)

# microG (GmsCore)
FetchContent_Declare(microg GIT_REPOSITORY https://github.com/microg/GmsCore GIT_TAG <stable-tag>)

# ANGLE
FetchContent_Declare(angle GIT_REPOSITORY https://chromium.googlesource.com/angle/angle GIT_TAG <chromium-milestone>)

# Mesa (if not system)
# Usually system package; only need if custom build required
```

### System Package Requirements (Arch/CachyOS)
```bash
pacman -S \
  box86 box64 \
  mesa vulkan-radeon vulkan-intel vulkan-nouveau \
  angle \
  microg-gmscore microg-unifiednlp \
  libinput sdl2 gtk4-layer-shell \
  wayland-protocols vulkan-headers \
  libxml2 libportal webkitgtk-6.0
```

---

## 7. Implementation Phases

### Phase 1: Foundation (Weeks 1-2)
- [ ] Restore ARM detection in linker
- [ ] Add box86/box64 FetchContent + build integration
- [ ] Hook `FLAG_ARM_LIBRARY` → box86/box64 translation
- [ ] Verify ARM .so loading works (hello-arm test)

### Phase 2: Graphics Stack (Weeks 3-5)
- [ ] Add ANGLE FetchContent
- [ ] Implement GLES 3.x via ANGLE in `GLImpl.java`/`egl.c`
- [ ] Implement Vulkan ICD loader + Mesa integration
- [ ] Fix `ANativeWindow` → Wayland surface + swapchain
- [ ] Test with `gles3jni` / `vulkan-basic` samples

### Phase 3: microG Integration (Weeks 6-8)
- [ ] Add microG FetchContent + build native libs
- [ ] Replace GMS stubs with microG JNI bindings
- [ ] Implement UnifiedNlp (location) + GSF (push)
- [ ] Add account management UI
- [ ] Test with Play Games / Sign-in sample apps

### Phase 4: Input & Polish (Weeks 9-10)
- [ ] Multi-touch via GtkGestureSingle
- [ ] Gamepad via libinput + SDL2
- [ ] Relative mouse mode
- [ ] Frame pacing + vsync tuning
- [ ] Memory/performance profiling

### Phase 5: Game Testing (Week 11+)
- [ ] Test 32-bit ARM game (GTA:SA, etc.)
- [ ] Test 64-bit ARM game (Genshin, etc.)
- [ ] Test GMS-dependent games (leaderboards, IAP)
- [ ] Regression testing

---

## 8. Open Questions (Need User Decision)

| # | Question | Recommendation |
|---|----------|----------------|
| 1 | **Target Android API level?** | API 28 (Android 9) — good balance of compatibility + modern APIs |
| 2 | **Target ABI?** | Both `armeabi-v7a` (box86) + `arm64-v8a` (box64) |
| 3 | **Distribution format?** | Flatpak (sandboxed, Wayland-native) or AppImage |
| 4 | **Signing / signature spoofing?** | microG needs `FAKE_SIGNATURE` — use `microg/GmsCore` patches |
| 5 | **Location backend for UnifiedNlp?** | Mozilla Location Service (deprecated) → use **Apple WPS / Deutsche Telekom / OpenCellID** |
| 6 | **Push (FCM/GCM) backend?** | microG uses **Firebase Cloud Messaging** — needs Firebase project |
| 7 | **License compliance?** | box86/box64 (LGPL-2.1) → dynamic link only; microG (Apache-2.0) OK; ANGLE (BSD) OK |
| 8 | **Hardware acceleration requirement?** | Require Vulkan 1.1+ capable GPU (all modern Mesa drivers) |

---

## 9. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| box86/box64 game compatibility | Medium | High | Test early; maintain compatibility list |
| microG build complexity | High | High | Use pre-built microG binaries if needed |
| ANGLE GLES 3.x completeness | Low | Medium | Fallback to native Mesa GLES for unsupported extensions |
| Wayland vs X11 games | Medium | Medium | Implement XWayland fallback |
| ART version skew | Low | Low | Pin art_standalone to known-good AOSP tag |

---

## 10. Validation Criteria

| Milestone | Test |
|-----------|------|
| **ARM Translation** | `box86 /lib/arm-linux-gnueabihf/ld-linux.so.3 --list` works; ARM hello-world runs |
| **GLES 3.x** | `gles3jni` sample renders triangle with shaders |
| **Vulkan** | `vulkaninfo` shows ICD; `vulkan-cube` renders |
| **microG** | `GoogleSignIn` returns real OAuth token; `Games` client connects |
| **Full Game** | GTA:SA (ARMv7) launches, renders, accepts input |

---

## 11. Files to Modify (High-Level)

| File | Changes |
|------|---------|
| `CMakeLists.txt` (root) | Add box86/box64/microG/ANGLE FetchContent |
| `thirdparty/bionic_translation/linker/linker.c` | Restore ARM detection; hook translation |
| `thirdparty/bionic_translation/linker/linker.h` | Ensure `FLAG_ARM_LIBRARY` used |
| `src/libandroid/native_window.c` | Fix xr_func; ANativeWindow→Wayland |
| `src/libandroid/input.c` | Multi-touch + gamepad |
| `src/api-impl/com/google/android/gms/**` | Replace with microG JNI |
| `src/api-impl/com/google/android/gles_jni/GLImpl.java` | ANGLE GLES 3.x impl |
| `src/api-impl/com/google/android/gles_jni/EGLImpl.java` | Mesa EGL + Wayland |
| `atl/CMakeLists.txt` | Link new deps; build flags |

---

## Next Steps

1. **User confirms/rejects recommendations** above (especially: box86/box64, microG, ANGLE, API level)
2. **I finalize plan** with accepted decisions
3. **Switch to implementation agent** to execute Phase 1

---

*Plan saved to: `.kilo/plans/1793653523557-android-game-support-plan.md`*