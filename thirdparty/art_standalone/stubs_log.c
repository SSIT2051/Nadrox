#include <stdarg.h>
#include <stddef.h>

/* Link-time-only stubs for Android logging APIs used by ART standalone builds. */

int __android_log_write(int prio, const char* tag, const char* msg) {
    (void)prio;
    (void)tag;
    (void)msg;
    return 0;
}

int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
    (void)prio;
    (void)tag;
    (void)fmt;
    return 0;
}

int __android_log_vprint(int prio, const char* tag, const char* fmt, va_list ap) {
    (void)prio;
    (void)tag;
    (void)fmt;
    (void)ap;
    return 0;
}

int __android_log_buf_write(int bufID, int prio, const char* tag, const char* msg) {
    (void)bufID;
    (void)prio;
    (void)tag;
    (void)msg;
    return 0;
}

int __android_log_assert(const char* cond, const char* tag, const char* fmt, ...) {
    (void)cond;
    (void)tag;
    (void)fmt;
    return 0;
}

/* Some code paths reference this internal/buffer variant name. */
int __android_log_assert_write(int prio, const char* tag, const char* msg) {
    (void)prio;
    (void)tag;
    (void)msg;
    return 0;
}
