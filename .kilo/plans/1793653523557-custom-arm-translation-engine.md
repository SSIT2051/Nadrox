# Nadrox ATL — Fully Custom ARMv4/v7 → x86_64 Translation Engine

## Vision
A **from-scratch, zero-dependency** ARM translation engine built specifically for Android games (GTA, complex 3D, 2D, multiplayer) — maximum FPS, minimum resources, best compatibility, smart game-aware optimization.

---

## Core Architecture Decisions (Locked In)

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **ARM ISA** | ARMv4 + ARMv7 (A32/T32/Thumb-2) + VFP/NEON | Covers all 32-bit Android games |
| **Translation Strategy** | **Hybrid JIT/AOT with Game-Aware Optimization** | Profile-guided: AOT cold paths, JIT hot paths, pre-render caching, speculative optimization |
| **IR** | **Custom MIR (Medium IR)** — SSA-based, game-tuned | Single static assignment, ~40 opcodes, built for ARM→x86_64 |
| **Integration** | **Replace bionic linker's ARM path** | Hook at `FLAG_ARM_LIBRARY` → translate → continue native linking |
| **Syscalls** | Full ARM→x86_64 translation (~120 syscalls) | Complete Linux syscall emulation |
| **Memory Model** | Single address space + `mmap` interception | Zero-copy, host-library compatible |
| **JNI/Trampolines** | AAPCS ↔ SysV AMD64 auto-generated | Per-method cached thunks |
| **Exception/Unwind** | ARM EHABI → x86_64 DWARF translation + custom personality | Full C++ exception support |
| **Self-Modifying Code** | `cacheflush`/`__clear_cache` → JIT cache invalidation | Correctness for JIT-compiled game code |

---

## Custom MIR Specification

### Design Goals
- **Minimal**: ~40 opcodes, no bloat
- **Game-tuned**: Opcodes for common game patterns (matrix math, vector ops, texture sampling)
- **SSA-based**: Enables optimization passes
- **ARM→x86_64 biased**: Lowering rules favor x86_64 strengths

### MIR Opcodes (Core Set)

| Category | Opcodes |
|----------|---------|
| **Arithmetic** | `add`, `sub`, `mul`, `div`, `mod`, `neg`, `abs`, `min`, `max` |
| **Bitwise** | `and`, `or`, `xor`, `not`, `shl`, `shr`, `sar`, `ror`, `rol` |
| **Compare** | `cmp_eq`, `cmp_ne`, `cmp_lt`, `cmp_le`, `cmp_gt`, `cmp_ge` (signed/unsigned) |
| **Control Flow** | `br`, `br_if`, `switch`, `call`, `ret`, `indirect_call` |
| **Memory** | `load`, `store`, `load8/16/32/64`, `store8/16/32/64`, `prefetch` |
| **Vector/SIMD** | `vadd`, `vsub`, `vmul`, `vdot`, `vcross`, `vnormalize`, `vswizzle`, `vload`, `vstore` |
| **Conversion** | `i2f`, `f2i`, `i2v`, `v2i`, `zext`, `sext`, `trunc` |
| **Stack/Frame** | `alloc`, `free`, `frame_addr`, `arg`, `local` |
| **Atomic** | `cas`, `xchg`, `fetch_add`, `fence` |
| **Game-Specific** | `mat4_mul`, `mat4_vec4`, `quat_slerp`, `tex_sample`, `fog_compute` |

### MIR Passes (Pipeline)

```
ARM Decode → Raw MIR → Legalize → SSA Construction → 
Game Pattern Match → Optimization Passes → 
Register Allocation → x86_64 Lowering → 
Block Chaining → Code Cache → Runtime
```

**Optimization Passes:**
1. **Constant Propagation** (SSA-based)
2. **Dead Code Elimination**
3. **Common Subexpression Elimination**
4. **Loop Invariant Code Motion**
5. **Strength Reduction** (mul→shift, div→mul)
6. **Vectorization** (scalar → NEON/SIMD patterns)
7. **Game Pattern Fusion** (mat4_mul + vec4 → single sequence)
8. **Speculative Inlining** (hot JNI calls, small functions)
9. **Profile-Guided Reordering** (hot blocks first)
10. **Register Allocation** (linear scan + spill optimization)

---

## Hybrid Translation Strategy

### Three-Tier Execution Model

| Tier | When | What | Cache Policy |
|------|------|------|--------------|
| **Tier 0: Interpreter** | First execution, cold code | Fast ARM decoder → direct x86_64 emit | No cache |
| **Tier 1: Basic Block JIT** | Function entered 2-5 times | MIR + basic optimizations → block cache | LRU, 64MB default |
| **Tier 2: Optimized Traces** | Hot loops (>1000 hits), game hotspots | Full MIR pipeline + PGO + speculative | Persistent, 256MB default |

### Game-Aware Optimizations

| Optimization | Trigger | Benefit |
|--------------|---------|---------|
| **Pre-render Cache** | Game startup / level load | Translate + optimize all library code ahead of time |
| **Shader/Uniform Cache** | GLES/Vulkan calls detected | Cache translated math sequences (mat4, quat) |
| **Audio Mixer Optimization** | OpenAL/SoundPool detected | SIMD-optimized mixing kernels |
| **Physics Step Fusion** | Box2D/Bullet patterns | Fuse collision → integrate → step |
| **Network Packet Decode** | Multiplayer detected | Pre-translate protobuf/flatbuffer decode |
| **Asset Decompression** | zlib/lz4 detected | SIMD decompression stubs |

### Caching Strategy

```
┌────────────────────────────────────────────────────────────┐
│  Persistent Disk Cache (~1-2GB)                            │
│  - Key: ARM library hash + optimization tier + game ID     │
│  - Value: x86_64 machine code + metadata                   │
│  - Invalidated on: library update, engine version change   │
├────────────────────────────────────────────────────────────┤
│  Runtime Memory Cache (LRU)                                │
│  - Tier 1: 64MB (basic blocks)                             │
│  - Tier 2: 256MB (optimized traces)                        │
│  - Tier 0: Interpreter (no cache)                          │
└────────────────────────────────────────────────────────────┘
```

---

## Integration with Existing NADROX Linker

### Hook Point: `apkenv_load_library()` → `FLAG_ARM_LIBRARY`

```c
// In linker.c, after ARM detection sets FLAG_ARM_LIBRARY:
if (si->flags & FLAG_ARM_LIBRARY) {
    // NEW: Translate ARM library to x86_64
    void *translated_handle = atl_arm_translate_library(
        si->fullpath,           // ARM .so path
        si->base,               // Preferred load address
        &si->translated_base,   // OUT: x86_64 base
        &si->translated_size    // OUT: x86_64 size
    );
    if (translated_handle) {
        si->flags |= FLAG_TRANSLATED;  // New flag
        si->translated_handle = translated_handle;
        // Continue with x86_64 linking...
    }
}
```

### New File: `src/translation/atl_arm_translate.c`

**Exports:**
- `atl_arm_translate_library()` — Main entry
- `atl_arm_translate_function()` — Single function (for JIT)
- `atl_arm_invalidate_cache()` — Self-modifying code
- `atl_arm_get_stats()` — Profiling/debug

---

## Custom MIR Implementation Plan

### Directory Structure
```
src/translation/
├── mir/
│   ├── mir.h                 # Public API
│   ├── mir_types.h           # SSA value, block, function, module
│   ├── mir_opcodes.h         # Opcode definitions
│   ├── mir_builder.c         # MIR construction API
│   ├── mir_verify.c          # SSA verification
│   ├── mir_print.c           # Debug printing
│   ├── passes/
│   │   ├── pass_manager.c
│   │   ├── const_prop.c
│   │   ├── dce.c
│   │   ├── cse.c
│   │   ├── licm.c
│   │   ├── strength_reduce.c
│   │   ├── vectorize.c
│   │   ├── game_fusion.c
│   │   ├── inlining.c
│   │   ├── regalloc.c        # Linear scan
│   │   └── scheduling.c
│   ├── lowering/
│   │   ├── lower_x86_64.c    # MIR → x86_64
│   │   ├── reg_alloc.c
│   │   └── asm_emit.c        # Raw byte emission
│   ├── arm_decode/
│   │   ├── decode_a32.c      # ARM instruction decode
│   │   ├── decode_t32.c      # Thumb-2 decode
│   │   ├── decode_neon.c     # NEON/VFP decode
│   │   └── arm_to_mir.c      # ARM → Raw MIR
│   ├── runtime/
│   │   ├── jit_cache.c       # Tier 0/1/2 cache
│   │   ├── block_chain.c     # Direct block linking
│   │   ├── profiler.c        # Edge/profile counters
│   │   ├── disk_cache.c      # Persistent cache
│   │   └── game_detect.c     # Game pattern recognition
│   ├── syscall/
│   │   ├── syscall_table.c   # ARM# → x86_64# + arg map
│   │   ├── syscall_emu.c     # Emulation stubs
│   │   └── signal_emu.c      # Signal handling
│   ├── jni/
│   │   ├── thunk_gen.c       # AAPCS ↔ SysV trampolines
│   │   ├── jni_table.c       # JNI method cache
│   │   └── jni_mir.c         # JNI-specific MIR patterns
│   ├── unwind/
│   │   ├── ehabi_decode.c    # ARM .ARM.exidx parser
│   │   ├── dwarf_emit.c      # x86_64 .eh_frame emit
│   │   └── personality.c     # Custom personality routine
│   └── atl_arm_translate.c   # Main entry (hooks linker)
```

---

## Syscall Translation Layer

### ARM → x86_64 Mapping Strategy

| ARM Syscall | x86_64 Syscall | Arg Mapping |
|-------------|----------------|-------------|
| `read` (3) | `read` (0) | r0→rdi, r1→rsi, r2→rdx |
| `write` (4) | `write` (1) | r0→rdi, r1→rsi, r2→rdx |
| `open` (5) | `openat` (257) | r0→rdi(AT_FDCWD), r1→rsi, r2→rdx, r3→r10 |
| `mmap` (192) | `mmap` (9) | r0→rdi, r1→rsi, r2→rdx, r3→r10, r4→r8, r5→r9 |
| `munmap` (191) | `munmap` (11) | r0→rdi, r1→rsi |
| `clone` (120) | `clone` (56) | Complex: stack, flags, ptid, tls, ctid |
| `futex` (240) | `futex` (202) | r0→rdi, r1→rsi, r2→rdx, r3→r10, r4→r8 |
| `rt_sigaction` (174) | `rt_sigaction` (13) | Structure translation |
| `rt_sigreturn` (173) | `rt_sigreturn` (15) | Context restore |

**Total: ~120 syscalls** — full coverage for games.

---

## JNI Trampoline System

### AAPCS (ARM) → System V AMD64 (x86_64)

| ARM (AAPCS) | x86_64 (SysV) | Notes |
|-------------|---------------|-------|
| r0-r3 (args 0-3) | rdi, rsi, rdx, rcx | First 4 args |
| r4-r11 (callee-saved) | rbx, rbp, r12-r15 | Preserve across calls |
| r12 (IP) | — | Scratch |
| r13 (SP) | rsp | Stack pointer |
| r14 (LR) | — | Return address in [rsp] |
| r15 (PC) | rip | Instruction pointer |
| CPSR | RFLAGS | Condition codes |

**Trampoline Template:**
```assembly
; ARM JNI function entry
jni_trampoline_<method>:
    ; Save x86_64 callee-saved
    push rbx; push rbp; push r12; push r13; push r14; push r15
    ; Move args: rdi/rsi/rdx/rcx/r8/r9 → r0-r5 (ARM convention)
    mov r0, rdi; mov r1, rsi; mov r2, rdx; mov r3, rcx; mov r4, r8; mov r5, r9
    ; Stack args: [rsp+48] → r6, [rsp+56] → r7
    mov r6, [rsp+48]; mov r7, [rsp+56]
    ; Call ARM function (via translated code)
    call translated_arm_function
    ; Return value in r0 → rax
    mov rax, r0
    ; Restore
    pop r15; pop r14; pop r13; pop r12; pop rbp; pop rbx
    ret
```

---

## Memory Model & mmap Interception

### Single Address Space Design

```
0x0000000000000000 - 0x00007FFFFFFFFFFF: User space (shared)
  ├── Host x86_64 libraries (libc, Mesa, etc.)
  ├── Translated ARM code (Tier 2 cache)
  ├── ARM library mappings (intercepted mmap → same VA)
  ├── JIT code cache (Tier 1/2)
  └── Game heap/stack
0xFFFF800000000000+: Kernel space
```

### mmap Interception

```c
// In translation runtime
void *atl_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off) {
    if (flags & MAP_ANONYMOUS) {
        // Game heap/stack — map normally
        return mmap(addr, len, prot, flags, fd, off);
    }
    // ARM library mapping — intercept
    if (is_arm_library(fd, off)) {
        // Translate and map x86_64 version at SAME virtual address
        void *translated = atl_arm_get_translated_mapping(fd, off, len);
        if (translated) return translated;
    }
    return mmap(addr, len, prot, flags, fd, off);
}
```

---

## Exception Handling / Unwinding

### ARM EHABI → x86_64 DWARF

1. **Parse `.ARM.exidx`** — entries: (prelude, personality, handler)
2. **Translate personality** — ARM personality → x86_64 personality
3. **Emit `.eh_frame`** — DWARF CFI for each function
4. **Custom personality routine** — Handles ARM cleanups, C++ exceptions

```c
// Custom personality for translated ARM code
_Unwind_Reason_Code atl_arm_personality(
    int version, _Unwind_Action actions, uint64_t exceptionClass,
    _Unwind_Exception *exc, struct _Unwind_Context *ctx
) {
    // 1. Find ARM function via PC
    // 2. Look up .ARM.exidx entry
    // 3. Execute ARM cleanup (via translated cleanup code)
    // 4. If C++ exception, re-raise via __cxa_rethrow
    // 5. Return _URC_CONTINUE_UNWIND or _URC_INSTALL_CONTEXT
}
```

---

## Testing & Validation Strategy

### Tier 1: Unit Tests (No Games)
| Test | Target |
|------|--------|
| ARM decode round-trip | All ARMv4/v7 + Thumb-2 + NEON |
| MIR SSA verification | All passes preserve SSA |
| Syscall emulation | All 120 syscalls vs native |
| JNI trampoline | 50+ JNI signatures |
| Unwind | C++ throw/catch across ARM/x86_64 |

### Tier 2: Integration Tests
| Test | Target |
|------|--------|
| Static ARM hello-world | Full translate + run |
| Dynamic ARM libc.so | syscalls + TLS + dlopen |
| ARM liblog.so | JNI thunks + logging |
| ARM libEGL.so | Graphics path |

### Tier 3: Game Validation (GTA Focus)
| Game | Validation |
|------|------------|
| **GTA: San Andreas** | Launch, menu, gameplay, missions, audio, multiplayer (SA-MP) |
| **GTA: Vice City** | Same |
| **Simple 2D (SDL2 test)** | Render loop, input, audio |
| **Unity 2D game** | IL2CPP + native libs |
| **Multiplayer (SA-MP/VC-MP)** | Network + sync |

### Performance Targets (GTA:SA on mid-range x86_64)
| Metric | Target |
|--------|--------|
| **FPS** | ≥60 FPS @ 1080p |
| **Frame Time** | ≤16.6ms (99th percentile) |
| **Memory** | ≤2GB total (engine + game) |
| **Startup** | ≤5s to playable |
| **Compatibility** | 100% main story missions |

---

## Implementation Phases

### Phase 1: Foundation (Core Infrastructure)
- [ ] MIR type system + builder + verification
- [ ] ARM decoder (A32 + T32 + NEON) → Raw MIR
- [ ] MIR → x86_64 lowering + linear scan regalloc
- [ ] Basic block JIT cache (Tier 1)
- [ ] Hook `apkenv_load_library()` at `FLAG_ARM_LIBRARY`

### Phase 2: Syscalls + JNI + Runtime
- [ ] 120 syscall translations
- [ ] JNI trampoline generator + cache
- [ ] TLS + `mmap` interception
- [ ] ARM EHABI → DWARF + personality routine
- [ ] Signal handling + `rt_sigreturn`

### Phase 3: Optimization Pipeline
- [ ] SSA construction + core passes (const prop, DCE, CSE, LICM)
- [ ] Strength reduction + vectorization
- [ ] Game pattern fusion (mat4, quat, tex)
- [ ] Speculative inlining + PGO infrastructure
- [ ] Tier 2 optimized traces + block chaining

### Phase 4: Caching + Game Awareness
- [ ] Persistent disk cache (library hash + tier + game ID)
- [ ] Game detection (GTA, Unity, Unreal, generic)
- [ ] Pre-render cache (startup/level load)
- [ ] Shader/uniform/audio/physics optimizations

### Phase 5: GTA Validation + Polish
- [ ] GTA:SA launch → menu → gameplay
- [ ] Audio (OpenAL), input, multiplayer
- [ ] Performance profiling + optimization
- [ ] Edge cases: self-modifying code, dlopen, JNI callbacks

---

## File Map (New/Modified)

| File | Status | Purpose |
|------|--------|---------|
| `src/translation/mir/*.c/.h` | **NEW** | Custom MIR implementation |
| `src/translation/atl_arm_translate.c` | **NEW** | Main translation entry |
| `thirdparty/bionic_translation/linker/linker.c` | **MODIFY** | Hook `FLAG_ARM_LIBRARY` |
| `thirdparty/bionic_translation/linker/linker.h` | **MODIFY** | New flags: `FLAG_TRANSLATED` |
| `src/translation/runtime/jit_cache.c` | **NEW** | Tier 0/1/2 cache |
| `src/translation/runtime/disk_cache.c` | **NEW** | Persistent cache |
| `src/translation/runtime/game_detect.c` | **NEW** | Game pattern recognition |
| `src/translation/syscall/*.c` | **NEW** | Syscall translation |
| `src/translation/jni/*.c` | **NEW** | JNI trampolines |
| `src/translation/unwind/*.c` | **NEW** | Exception handling |
| `src/translation/arm_decode/*.c` | **NEW** | ARM → MIR decode |
| `CMakeLists.txt` (root) | **MODIFY** | Add translation subdirectory |
| `atl/CMakeLists.txt` | **MODIFY** | Link translation engine |

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| **ARM decode complexity** | Auto-generate decoder from ARM XML spec; test exhaustively |
| **MIR pass bugs** | SSA verification after every pass; property-based testing |
| **Syscall edge cases** | Compare against native strace; automated regression |
| **JNI trampoline correctness** | Per-signature unit tests; fuzzing |
| **Unwind/C++ exceptions** | Test with libc++abi; validate against Itanium ABI |
| **Self-modifying code** | `cacheflush` interception + cache invalidation tests |
| **Performance regression** | Continuous benchmarking (GTA FPS) in CI |
| **Game-specific hacks** | Configurable per-game profiles; avoid hardcoding |

---

## Validation Checklist (Plan Complete When)

- [ ] ARMv4/v7 + Thumb-2 + NEON decode → MIR 100% coverage
- [ ] MIR passes preserve SSA + semantics (verified)
- [ ] 120 syscalls translate + emulate correctly
- [ ] JNI trampolines work for 50+ signatures
- [ ] C++ exceptions unwind across ARM/x86_64 boundary
- [ ] Static ARM hello-world runs
- [ ] Dynamic ARM libc.so loads + works
- [ ] GTA:SA launches, renders, plays audio, accepts input
- [ ] 60 FPS sustained in GTA:SA gameplay
- [ ] Multiplayer (SA-MP) connects + syncs
- [ ] Persistent cache works across restarts
- [ ] Memory ≤2GB, startup ≤5s

---

*Plan saved to: `.kilo/plans/1793653523557-custom-arm-translation-engine.md`*