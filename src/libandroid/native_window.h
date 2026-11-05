
#include <EGL/egl.h>
#include <GL/gl.h>
#include <X11/Xlib.h>
#include <gtk/gtk.h>
#include <jni.h>
#include <wayland-client.h>

#ifndef ARect_DEFINED
#define ARect_DEFINED
typedef struct ARect {
    int32_t left;
    int32_t top;
    int32_t right;
    int32_t bottom;
} ARect;
#endif

typedef struct ANativeWindow_Buffer {
	// The number of pixels that are show horizontally.
	int32_t width;

	// The number of pixels that are shown vertically.
	int32_t height;

	// The number of *pixels* that a line in the buffer takes in
	// memory. This may be >= width.
	int32_t stride;

	// The format of the buffer. One of AHARDWAREBUFFER_FORMAT_*
	int32_t format;

	// The actual bits.
	void *bits;

	// Do not touch.
	uint32_t reserved[6];
} ANativeWindow_Buffer;


typedef struct ANativeWindow ANativeWindow;

struct ANativeWindow {
	EGLNativeWindowType egl_window;
	GtkWidget *surface_view_widget;
	struct wl_display *wayland_display;
	struct wl_surface *wayland_surface;
	struct wl_callback *frame_callback;
	Display *x11_display;
	gulong resize_handler;
	int refcount;
	int width;
	int height;
};

struct ANativeWindow *ANativeWindow_fromSurface(JNIEnv *env, jobject surface);
EGLSurface bionic_eglCreateWindowSurface(EGLDisplay display, EGLConfig config, struct ANativeWindow *native_window, EGLint const *attrib_list);
EGLBoolean bionic_eglDestroySurface(EGLDisplay display, EGLSurface surface);
EGLDisplay bionic_eglGetDisplay(NativeDisplayType native_display);
EGLBoolean bionic_eglMakeCurrent(EGLDisplay display, EGLSurface draw, EGLSurface read, EGLContext context);
EGLBoolean bionic_eglSwapBuffers(EGLDisplay display, EGLSurface surface);
void bionic_glBindFramebuffer(GLenum target, GLuint framebuffer);
void ANativeWindow_acquire(struct ANativeWindow *native_window);
void ANativeWindow_release(struct ANativeWindow *native_window);

// Frame pacing functions
int32_t ANativeWindow_lock(ANativeWindow *window, ANativeWindow_Buffer *outBuffer, ARect *inOutDirtyBounds);
int32_t ANativeWindow_unlockAndPost(struct ANativeWindow *window);
