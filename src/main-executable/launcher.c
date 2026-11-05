// launcher.c - Embedded launcher that sets up environment and execs the main binary
// This eliminates the need for wrapper scripts by embedding library paths

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <libgen.h>
#include <stdbool.h>

// Installation paths (can be overridden at build time)
#ifndef INSTALL_PREFIX
#define INSTALL_PREFIX "/usr/local"
#endif

#ifndef INSTALL_LIBDIR
#define INSTALL_LIBDIR "lib"
#endif

// Construct path relative to executable location
static char* get_executable_dir(char *argv0) {
    char *path = realpath(argv0, NULL);
    if (!path) return NULL;
    char *dir = dirname(path);
    char *result = strdup(dir);
    free(path);
    return result;
}

static void setup_library_path(char *exe_dir) {
    // Build LD_LIBRARY_PATH with our private libraries
    const char *old_ld_path = getenv("LD_LIBRARY_PATH");
    
    // Our library directories (relative to executable)
    char lib_art[PATH_MAX];
    char lib_natives[PATH_MAX];
    char lib_bionic[PATH_MAX];
    
    snprintf(lib_art, sizeof(lib_art), "%s/../lib/art", exe_dir);
    snprintf(lib_natives, sizeof(lib_natives), "%s/../lib/java/dex/android_translation_layer/natives", exe_dir);
    snprintf(lib_bionic, sizeof(lib_bionic), "%s/../lib", exe_dir);
    
    // Construct new LD_LIBRARY_PATH
    size_t new_len = strlen(lib_art) + strlen(lib_natives) + strlen(lib_bionic) + 3; // colons + null
    if (old_ld_path) new_len += strlen(old_ld_path) + 1;
    
    char *new_ld_path = malloc(new_len);
    if (!new_ld_path) return;
    
    snprintf(new_ld_path, new_len, "%s:%s:%s%s%s",
             lib_art, lib_natives, lib_bionic,
             old_ld_path ? ":" : "", old_ld_path ? old_ld_path : "");
    
    setenv("LD_LIBRARY_PATH", new_ld_path, 1);
    free(new_ld_path);
}

static void setup_bionic_ld_library_path(char *exe_dir) {
    // BIONIC_LD_LIBRARY_PATH for the bionic linker
    char app_lib_dir[PATH_MAX];
    const char *old_bionic_path = getenv("BIONIC_LD_LIBRARY_PATH");
    
    // We don't know the app data dir here, but we can set the base paths
    // The main executable will add the app-specific paths
    char lib_art[PATH_MAX];
    snprintf(lib_art, sizeof(lib_art), "%s/../lib/art", exe_dir);
    
    size_t new_len = strlen(lib_art) + 3;
    if (old_bionic_path) new_len += strlen(old_bionic_path) + 1;
    
    char *new_bionic_path = malloc(new_len);
    if (!new_bionic_path) return;
    
    snprintf(new_bionic_path, new_len, "%s%s%s",
             lib_art,
             old_bionic_path ? ":" : "", old_bionic_path ? old_bionic_path : "");
    
    setenv("BIONIC_LD_LIBRARY_PATH", new_bionic_path, 1);
    free(new_bionic_path);
}

static void setup_java_paths(char *exe_dir) {
    // Set paths for Java classpath and bootclasspath
    char bootclasspath[PATH_MAX * 3];
    snprintf(bootclasspath, sizeof(bootclasspath),
             "%s/../lib/java/core-all_classes.jar:"
             "%s/../lib/java/core-junit_classes.jar:"
             "%s/../lib/java/junit-runner_classes.jar",
             exe_dir, exe_dir, exe_dir);
    setenv("ATL_BOOTCLASSPATH", bootclasspath, 1);
    
    char api_impl_jar[PATH_MAX];
    snprintf(api_impl_jar, sizeof(api_impl_jar), "%s/../lib/java/dex/android_translation_layer/api-impl.jar", exe_dir);
    setenv("ATL_API_IMPL_JAR", api_impl_jar, 1);
    
    char framework_res[PATH_MAX];
    snprintf(framework_res, sizeof(framework_res), "%s/../lib/java/dex/android_translation_layer/framework-res.apk", exe_dir);
    setenv("ATL_FRAMEWORK_RES", framework_res, 1);
    
    char test_runner_jar[PATH_MAX];
    snprintf(test_runner_jar, sizeof(test_runner_jar), "%s/../lib/java/dex/android_translation_layer/test_runner.jar", exe_dir);
    setenv("ATL_TEST_RUNNER_JAR", test_runner_jar, 1);
    
    char api_impl_natives[PATH_MAX];
    snprintf(api_impl_natives, sizeof(api_impl_natives), "%s/../lib/java/dex/android_translation_layer/natives", exe_dir);
    setenv("ATL_API_IMPL_NATIVES", api_impl_natives, 1);
}

int main(int argc, char **argv) {
    // Parse command line arguments
    char *apk_path = NULL;
    bool fullscreen = true;      // Default ON
    bool enable_pc_input = true; // Default ON
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--apk") == 0 && i + 1 < argc) {
            apk_path = argv[++i];
        } else if (strcmp(argv[i], "--fullscreen") == 0) {
            fullscreen = true;
        } else if (strcmp(argv[i], "--no-fullscreen") == 0) {
            fullscreen = false;
        } else if (strcmp(argv[i], "--enable-pc-input") == 0) {
            enable_pc_input = true;
        } else if (strcmp(argv[i], "--disable-pc-input") == 0) {
            enable_pc_input = false;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options] [--apk <path>]\n", argv[0]);
            printf("Options:\n");
            printf("  --apk <path>           Path to APK file\n");
            printf("  --fullscreen           Enable fullscreen (default)\n");
            printf("  --no-fullscreen        Disable fullscreen\n");
            printf("  --enable-pc-input      Enable PC keyboard/mouse input (default)\n");
            printf("  --disable-pc-input     Disable PC input\n");
            printf("  --help                 Show this help\n");
            return 0;
        } else if (argv[i][0] != '-') {
            // Treat positional argument as APK path
            apk_path = argv[i];
        }
    }
    
    // Get executable directory
    char *exe_dir = get_executable_dir(argv[0]);
    if (!exe_dir) {
        fprintf(stderr, "launcher: failed to determine executable directory\n");
        return 1;
    }
    
    // Set up all library paths
    setup_library_path(exe_dir);
    setup_bionic_ld_library_path(exe_dir);
    setup_java_paths(exe_dir);
    
    // Set default locale for Android compatibility
    setenv("LC_ALL", "C.UTF-8", 1);
    
    // Pass APK path and settings via environment
    if (apk_path) {
        setenv("NADROX_APK_PATH", apk_path, 1);
    }
    setenv("NADROX_FULLSCREEN", fullscreen ? "1" : "0", 1);
    setenv("NADROX_PC_INPUT", enable_pc_input ? "1" : "0", 1);
    
    // Build path to the actual main binary
    char main_binary[PATH_MAX];
    snprintf(main_binary, sizeof(main_binary), "%s/android-translation-layer.bin", exe_dir);
    
    // Check if the main binary exists
    struct stat st;
    if (stat(main_binary, &st) != 0) {
        fprintf(stderr, "launcher: main binary not found at %s\n", main_binary);
        free(exe_dir);
        return 1;
    }
    
    // Prepare arguments for execv (pass remaining args to main binary)
    char **new_argv = malloc((argc + 1) * sizeof(char*));
    if (!new_argv) {
        free(exe_dir);
        return 1;
    }
    
    new_argv[0] = main_binary;
    int new_argc = 1;
    for (int i = 1; i < argc; i++) {
        // Skip our parsed args, pass others through
        if (strcmp(argv[i], "--apk") == 0) {
            i++; // skip next arg
        } else if (strcmp(argv[i], "--fullscreen") == 0 ||
                   strcmp(argv[i], "--no-fullscreen") == 0 ||
                   strcmp(argv[i], "--enable-pc-input") == 0 ||
                   strcmp(argv[i], "--disable-pc-input") == 0 ||
                   strcmp(argv[i], "--help") == 0) {
            continue;
        } else if (argv[i][0] != '-' && apk_path && strcmp(argv[i], apk_path) == 0) {
            continue; // skip positional APK path
        } else {
            new_argv[new_argc++] = argv[i];
        }
    }
    new_argv[new_argc] = NULL;
    
    // Exec the main binary
    execv(main_binary, new_argv);
    
    // If we get here, exec failed
    perror("launcher: execv failed");
    free(new_argv);
    free(exe_dir);
    return 1;
}