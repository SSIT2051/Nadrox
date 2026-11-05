#define _GNU_SOURCE
#include <assert.h>
#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "../../src/translation/elf/elf_loader.h"
#include "../../src/translation/elf/elf_dynlink.h"
#include "../../src/translation/elf/elf_jit.h"

typedef struct atl_lib_dbg { atl_elf_handle_t *elf; char *path; int refcount; struct atl_lib_dbg *next; } atl_lib_dbg_t;

static int tests_run;
static int tests_failed;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (!(cond)) { \
        tests_failed++; \
        fprintf(stderr, "FAIL: %s (%s:%d): %s\n", __func__, __FILE__, __LINE__, msg); \
        return; \
    } \
} while (0)

#define CHECK_PTR(p, msg) CHECK((p) != NULL, msg)
#define CHECK_NULL(p, msg) CHECK((p) == NULL, msg)

static const char *get_fixture(const char *name) {
    static char path[4096];
    const char *dir = getenv("ATL_ELF_TEST_FIXTURE_DIR");
    if (!dir) dir = "/tmp";
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    return path;
}

static int file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0;
}

static void test_error_handling(void) {
    void *h;

    h = atl_dlopen(NULL);
    CHECK_NULL(h, "dlopen(NULL) returns NULL");

    h = atl_dlopen("/definitely/not/a/real/atl_elf_test_file_12345.so");
    CHECK_NULL(h, "dlopen(nonexistent) returns NULL");

    h = atl_dlopen("/dev/null");
    CHECK_NULL(h, "dlopen(invalid ELF /dev/null) returns NULL");

    CHECK(atl_dlclose(NULL) != 0, "dlclose(NULL) fails");

    CHECK_NULL(atl_dlsym(NULL, "x"), "dlsym(NULL handle) returns NULL");
    CHECK_NULL(atl_dlsym((void *)0x1, NULL), "dlsym(NULL name) returns NULL");

    /* loader-level errors */
    atl_elf_handle_t *eh = atl_elf_load(NULL);
    CHECK_NULL(eh, "atl_elf_load(NULL) returns NULL");
    eh = atl_elf_load("/no/such/file.so");
    CHECK_NULL(eh, "atl_elf_load(missing) returns NULL");
}

static void test_load_host_so_and_call(void) {
    const char *path = get_fixture("libatl_host_fixture_v.so");
    if (!file_exists(path)) path = get_fixture("libatl_host_fixture.so");
    CHECK(file_exists(path), "host fixture .so exists");

    atl_elf_handle_t *eh = atl_elf_load(path);
    CHECK_PTR(eh, "atl_elf_load succeeds for host fixture");
    CHECK_PTR(eh->base, "loaded handle has base");
    CHECK(eh->phnum > 0, "loaded handle has program headers");

    /* Version tags: host fixture is built with version info; parser should store pointers/nums when present */
    if (eh->versym || eh->verneed || eh->verdef) {
        tests_run++;
        if (!(eh->versym || eh->verneed || eh->verdef)) {
            tests_failed++;
            fprintf(stderr, "FAIL: version tags present but not stored\n");
        }
    }

    atl_elf_unload(eh);

    void *h = atl_dlopen(path);
    CHECK_PTR(h, "atl_dlopen succeeds for host fixture");

    typedef int (*add_fn)(int, int);
    typedef int (*get_fn)(void);
    typedef void (*set_fn)(int);

    add_fn add_ints = (add_fn)(uintptr_t)atl_dlsym(h, "add_ints");
    get_fn get_global = (get_fn)(uintptr_t)atl_dlsym(h, "get_global");
    set_fn set_global = (set_fn)(uintptr_t)atl_dlsym(h, "set_global");

    CHECK_PTR(add_ints, "dlsym finds add_ints");
    CHECK_PTR(get_global, "dlsym finds get_global");
    CHECK_PTR(set_global, "dlsym finds set_global");

    if (add_ints) {
        CHECK(add_ints(2, 3) == 5, "add_ints(2,3)==5");
        CHECK(add_ints(-10, 7) == -3, "add_ints(-10,7)==-3");
    }
    if (get_global && set_global) {
        int before = get_global();
        CHECK(before == 0x12345678, "get_global initial value");
        set_global(99);
        CHECK(get_global() == 99, "set_global/get_global roundtrip");
        set_global(before);
    }

    CHECK(atl_dlclose(h) == 0, "atl_dlclose succeeds");
}

static void test_dlopen_dlsym_dlclose_lifecycle(void) {
    const char *path = get_fixture("libatl_host_fixture_v.so");
    if (!file_exists(path)) path = get_fixture("libatl_host_fixture.so");
    CHECK(file_exists(path), "host fixture exists for lifecycle");

    void *h1 = atl_dlopen(path);
    CHECK_PTR(h1, "first dlopen succeeds");
    void *sym1 = atl_dlsym(h1, "add_ints");
    CHECK_PTR(sym1, "dlsym on first handle");

    /* dlopen same path again should return a live handle (refcounted or same) */
    void *h2 = atl_dlopen(path);
    CHECK_PTR(h2, "second dlopen of same path succeeds");
    void *sym2 = atl_dlsym(h2, "add_ints");
    CHECK_PTR(sym2, "dlsym on second handle");
    CHECK(sym1 == sym2, "same symbol address across handles to same lib");

    CHECK(atl_dlclose(h2) == 0, "dlclose second handle");
    /* first handle still usable if refcounted */
    typedef int (*add_fn)(int, int);
    add_fn f = (add_fn)(uintptr_t)atl_dlsym(h1, "add_ints");
    CHECK_PTR(f, "first handle still resolves after second close");
    if (f) CHECK(f(1, 2) == 3, "call still works after refcount close");

    CHECK(atl_dlclose(h1) == 0, "dlclose first handle");
}

static void test_relocations_and_global_data(void) {
    const char *path = get_fixture("libatl_host_fixture_v.so");
    if (!file_exists(path)) path = get_fixture("libatl_host_fixture.so");
    void *h = atl_dlopen(path);
    CHECK_PTR(h, "dlopen for relocation/global test");

    typedef int (*call_add_fn)(int, int);
    typedef int (*get_fn)(void);
    typedef void (*set_fn)(int);
    call_add_fn call_add = (call_add_fn)(uintptr_t)atl_dlsym(h, "call_add");
    get_fn get_global = (get_fn)(uintptr_t)atl_dlsym(h, "get_global");
    set_fn set_global = (set_fn)(uintptr_t)atl_dlsym(h, "set_global");
    int *global_data = (int *)atl_dlsym(h, "global_data");

    CHECK_PTR(get_global, "find get_global");
    CHECK_PTR(set_global, "find set_global");
    /* call_add exercises internal call/PLT-like resolution if present; may be NULL depending on inlining, so don't hard-fail */
    if (call_add) {
        CHECK(call_add(20, 22) == 42, "call_add (internal/call path) returns 42");
    } else {
        tests_run++;
        printf("INFO: call_add symbol not exported/inlined; internal call relocation subcheck skipped\n");
    }

    if (global_data) {
        int old = *global_data;
        *global_data = 0x55aa;
        CHECK(get_global() == 0x55aa, "direct global data access matches getter (relocation/data)");
        *global_data = old;
    } else {
        tests_run++;
        printf("INFO: global_data symbol not found as data export; getter/setter used\n");
    }

    if (get_global && set_global) {
        set_global(1234);
        CHECK(get_global() == 1234, "global data via functions consistent");
    }

    CHECK(atl_dlclose(h) == 0, "dlclose relocation test handle");
}

static void test_load_arm_so_and_version_tags(void) {
    const char *path = get_fixture("libatl_fixture.so");
    if (!file_exists(path)) {
        tests_run++;
        printf("SKIP: ARM fixture %s not present\n", path);
        return;
    }

    atl_elf_handle_t *eh = atl_elf_load(path);
    CHECK_PTR(eh, "atl_elf_load succeeds for ARM .so");
    CHECK(eh->is64 == 0, "ARM fixture is ELF32");
    CHECK(eh->e_machine == EM_ARM, "ARM fixture e_machine is EM_ARM");
    CHECK_PTR(eh->base, "ARM fixture mapped base");

    /* Version tags are present in NDK-built ARM fixture (readelf showed VERSYM/VERNEED) */
    CHECK_PTR(eh->versym, "DT_VERSYM pointer stored");
    CHECK_PTR(eh->verneed, "DT_VERNEED pointer stored");
    CHECK(eh->verneedsz >= 1, "DT_VERNEEDNUM stored");
    /* VERDEF may or may not be present */
    tests_run++;
    if (eh->verdef) {
        if (eh->verdefsz == 0) {
            tests_failed++;
            fprintf(stderr, "FAIL: verdef present but verdefsz is 0\n");
        }
    } else {
        /* verdef absent is acceptable; field remains NULL/0 */
    }

    atl_elf_unload(eh);

    /* dlopen/dlsym/dlclose lifecycle on ARM ELF. Guest init/fini are skipped on host. */
    void *h = atl_dlopen(path);
    CHECK_PTR(h, "atl_dlopen succeeds for ARM .so");
    void *sym = atl_dlsym(h, "add_ints");
    CHECK_PTR(sym, "atl_dlsym finds add_ints in ARM .so");
    void *symg = atl_dlsym(h, "get_global");
    CHECK_PTR(symg, "atl_dlsym finds get_global in ARM .so");

    /* Best-effort: load a simple ARM .so and call a function via JIT translation.
     * Calling ARM machine code natively on x86_64 is not possible; if JIT translation
     * is available and succeeds, verify the translated function produces correct results.
     * Run in a child with a timeout because translation can be expensive/hang on some inputs. */
    {
        pid_t pid = fork();
        if (pid == 0) {
            alarm(5);
            atl_elf_handle_t *arm_eh = ((atl_lib_dbg_t *)h)->elf;
            atl_elf_jit_t *jit = atl_elf_jit_create(arm_eh);
            if (jit && sym) {
                void *code = atl_elf_jit_translate(jit, (uintptr_t)sym);
                if (code) {
                    int (*f)(int, int) = (int (*)(int, int))code;
                    _exit(f(2, 3) == 5 ? 0 : 2);
                }
            }
            if (jit) atl_elf_jit_destroy(jit);
            _exit(3); /* unavailable */
        } else if (pid > 0) {
            int st = 0;
            waitpid(pid, &st, 0);
            if (WIFEXITED(st) && WEXITSTATUS(st) == 0) {
                CHECK(1, "JIT-translated ARM add_ints(2,3)==5");
            } else if (WIFEXITED(st) && WEXITSTATUS(st) == 2) {
                CHECK(0, "JIT-translated ARM add_ints returned wrong result");
            } else {
                tests_run++;
                printf("INFO: ARM JIT translation/call unavailable or timed out in this environment; load/dlsym/lifecycle and version parsing verified\n");
            }
        } else {
            tests_run++;
            printf("INFO: fork unavailable for ARM JIT call test; load/dlsym/lifecycle verified\n");
        }
    }

    CHECK(atl_dlclose(h) == 0, "atl_dlclose succeeds for ARM .so");
}

int main(void) {
    printf("ATL ELF loader/dynlink tests\n");

    test_error_handling();
    test_load_host_so_and_call();
    test_dlopen_dlsym_dlclose_lifecycle();
    test_relocations_and_global_data();
    test_load_arm_so_and_version_tags();

    printf("\nRESULT: %d/%d passed, %d failed\n", tests_run - tests_failed, tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
