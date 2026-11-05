#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>

__thread int tls_var = 42;

__attribute__((visibility("default")))
int test_add(int a, int b) {
    return a + b;
}

__attribute__((visibility("default")))
int test_multiply(int a, int b) {
    return a * b;
}

__attribute__((visibility("default")))
const char* test_get_string(void) {
    return "Hello from ARM shared library!";
}

__attribute__((visibility("default")))
void test_write_output(void) {
    const char *msg = "ARM library wrote this!\n";
    write(STDOUT_FILENO, msg, strlen(msg));
}

__attribute__((visibility("default")))
int test_tls_read(void) {
    return tls_var;
}

__attribute__((visibility("default")))
void test_tls_write(int val) {
    tls_var = val;
}

__attribute__((visibility("default")))
int test_syscall_read_write(void) {
    char buf[64];
    const char *msg = "test\n";
    int written = write(STDOUT_FILENO, msg, strlen(msg));
    return written;
}

__attribute__((visibility("default")))
void* test_dlopen_self(void) {
    void *h = dlopen(NULL, RTLD_LAZY);
    if (h) {
        dlclose(h);
        return h;
    }
    return NULL;
}
