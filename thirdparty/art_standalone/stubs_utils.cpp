/*
 * Link-time-only stubs for selected libutils/android base symbols.
 * These provide placeholder definitions to satisfy unresolved references when
 * linking against the standalone ART stub tree; they are not runtime-correct
 * implementations.
 */

#include <stddef.h>
#include <stdint.h>

namespace android {

class RefBase {
public:
    void incStrong(const void* id) const;
    void decStrong(const void* id) const;
};

void RefBase::incStrong(const void* id) const { (void)id; }
void RefBase::decStrong(const void* id) const { (void)id; }

class LooperCallback { public: virtual ~LooperCallback() {} };
template <typename T> class sp { public: T* mPtr; sp() : mPtr(0) {} sp(const sp&) : mPtr(0) {} template<typename U> sp(const sp<U>&):mPtr(0){} ~sp(){} };

typedef int (*Looper_callbackFunc)(int fd, int events, void* data);

class Looper {
public:
    static sp<Looper> prepare(int opts);
    static sp<Looper> getForThread();
    int pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData);
    int pollAll(int timeoutMillis, int* outFd, int* outEvents, void** outData);
    void wake();
    int addFd(int fd, int ident, int events, Looper_callbackFunc callback, void* data);
    int addFd(int fd, int ident, int events, const sp<LooperCallback>& callback, void* data);
    int removeFd(int fd);
    bool isPolling() const;
};

static Looper g_looper;
static sp<Looper> g_sp;

sp<Looper> Looper::prepare(int opts) { (void)opts; g_sp.mPtr = &g_looper; return g_sp; }
sp<Looper> Looper::getForThread() { g_sp.mPtr = &g_looper; return g_sp; }
int Looper::pollOnce(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    (void)timeoutMillis; if (outFd) *outFd = 0; if (outEvents) *outEvents = 0; if (outData) *outData = 0; return 0;
}
int Looper::pollAll(int timeoutMillis, int* outFd, int* outEvents, void** outData) {
    (void)timeoutMillis; if (outFd) *outFd = 0; if (outEvents) *outEvents = 0; if (outData) *outData = 0; return 0;
}
void Looper::wake() {}
int Looper::addFd(int fd, int ident, int events, Looper_callbackFunc callback, void* data) {
    (void)fd; (void)ident; (void)events; (void)callback; (void)data; return 1;
}
int Looper::addFd(int fd, int ident, int events, const sp<LooperCallback>& callback, void* data) {
    (void)fd; (void)ident; (void)events; (void)callback; (void)data; return 1;
}
int Looper::removeFd(int fd) { (void)fd; return 1; }
bool Looper::isPolling() const { return false; }

} // namespace android
