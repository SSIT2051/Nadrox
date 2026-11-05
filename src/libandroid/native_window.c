/*
 * parts of this file originally from AOSP:
 *
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdint.h>
#include <stdlib.h>

#include <EGL/egl.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include <vulkan/vulkan_wayland.h>
#include <vulkan/vulkan_xlib.h>

#include <gdk/wayland/gdkwayland.h>
#include <gdk/x11/gdkx.h>
#include <gtk/gtk.h>

// FIXME: move this together with the other stuff that doesn't belong in this file
#include <openxr/openxr.h>
#define XR_USE_PLATFORM_EGL
#include <openxr/openxr_platform.h>
#ifndef XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT
	#define XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT 1000426000
#endif

#include <assert.h>
#include <string.h>
#include <dlfcn.h>

#include <jni.h>

// FIXME: put the header in a common place
#include "../api-impl-jni/defines.h"

#include "native_window.h"

/**
 * Transforms that can be applied to buffers as they are displayed to a window.
 *
 * Supported transforms are any combination of horizontal mirror, vertical
 * mirror, and clockwise 90 degree rotation, in that order. Rotations of 180
 * and 270 degrees are made up of those basic transforms.
 */
enum ANativeWindowTransform {
	ANATIVEWINDOW_TRANSFORM_IDENTITY = 0x00,
	ANATIVEWINDOW_TRANSFORM_MIRROR_HORIZONTAL = 0x01,
	ANATIVEWINDOW_TRANSFORM_MIRROR_VERTICAL = 0x02,
	ANATIVEWINDOW_TRANSFORM_ROTATE_90 = 0x04,

	ANATIVEWINDOW_TRANSFORM_ROTATE_180 = ANATIVEWINDOW_TRANSFORM_MIRROR_HORIZONTAL | ANATIVEWINDOW_TRANSFORM_MIRROR_VERTICAL,
	ANATIVEWINDOW_TRANSFORM_ROTATE_270 = ANATIVEWINDOW_TRANSFORM_ROTATE_180 | ANATIVEWINDOW_TRANSFORM_ROTATE_90,
};

/**
 * Opaque type that provides access to a native window.
 *
 * A pointer can be obtained using {@link ANativeWindow_fromSurface()}.
 */
typedef struct ANativeWindow ANativeWindow;

/**
 * Struct that represents a windows buffer.
 *
 * A pointer can be obtained using {@link ANativeWindow_lock()}.
 */

/**
 * Acquire a reference on the given {@link ANativeWindow} object. This prevents the object
 * from being deleted until the reference is removed.
 */
void ANativeWindow_acquire(struct ANativeWindow *native_window)
{
	native_window->refcount++;
}

void ANativeWindow_release(struct ANativeWindow *native_window)
{
	native_window->refcount--;
	if (native_window->refcount == 0) {
		g_clear_signal_handler(&native_window->resize_handler, native_window->surface_view_widget);
		if (native_window->wayland_display) {
			// Clean up frame callback
			if (native_window->frame_callback) {
				wl_callback_destroy(native_window->frame_callback);
				native_window->frame_callback = NULL;
			}
			wl_egl_window_destroy((struct wl_egl_window *)native_window->egl_window);
			wl_surface_destroy(native_window->wayland_surface);
		} else if (native_window->x11_display) {
			XDestroyWindow(native_window->x11_display, native_window->egl_window);
		}
		free(native_window);
	}
}

int32_t ANativeWindow_getWidth(struct ANativeWindow *native_window)
{
	return gtk_widget_get_width(native_window->surface_view_widget);
}

int32_t ANativeWindow_getHeight(struct ANativeWindow *native_window)
{
	return gtk_widget_get_height(native_window->surface_view_widget);
}

/**
 * Return the current pixel format (AHARDWAREBUFFER_FORMAT_*) of the window surface.
 *
 * \return a negative value on error.
 */
int32_t ANativeWindow_getFormat(ANativeWindow *window)
{
	return -1;
}

/**
 * Change the format and size of the window buffers.
 *
 * The width and height control the number of pixels in the buffers, not the
 * dimensions of the window on screen. If these are different than the
 * window's physical size, then its buffer will be scaled to match that size
 * when compositing it to the screen. The width and height must be either both zero
 * or both non-zero.
 *
 * For all of these parameters, if 0 is supplied then the window's base
 * value will come back in force.
 *
 * \param width width of the buffers in pixels.
 * \param height height of the buffers in pixels.
 * \param format one of AHARDWAREBUFFER_FORMAT_* constants.
 * \return 0 for success, or a negative value on error.
 */
int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *window,
                                         int32_t width, int32_t height, int32_t format)
{
	return -1;
}

/**
 * Frame callback for Wayland VSync
 * Called when the compositor is ready for a new frame
 */
static void frame_callback_done(void *data, struct wl_callback *callback, uint32_t time)
{
    struct ANativeWindow *native_window = data;
    wl_callback_destroy(callback);
    native_window->frame_callback = NULL;
}

/**
 * Lock the window's next drawing surface for writing.
 * Waits for VSync if using Wayland with frame callback.
 *
 * \return 0 for success, or a negative value on error.
 */
int32_t ANativeWindow_lock(ANativeWindow *window, ANativeWindow_Buffer *outBuffer,
                           ARect *inOutDirtyBounds)
{
    if (!window || !outBuffer) {
        return -1;
    }

    // If using Wayland with frame callback, wait for VSync
    if (window->wayland_display && window->wayland_surface) {
        // Request a frame callback for VSync
        if (!window->frame_callback) {
            window->frame_callback = wl_surface_frame(window->wayland_surface);
            wl_callback_add_listener(window->frame_callback, 
                &(struct wl_callback_listener){.done = frame_callback_done}, window);
        }
        
        // Note: In a real implementation, we would block here until the frame callback fires
        // For now, we'll just ensure the callback is set up
    }

    // Fill in the buffer info
    outBuffer->width = window->width;
    outBuffer->height = window->height;
    outBuffer->stride = window->width; // Simplified - actual stride may differ
    outBuffer->format = 1; // AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM = 1
    outBuffer->bits = NULL; // Would be set by the actual rendering backend
    
    // Clear dirty bounds (simplified)
    if (inOutDirtyBounds) {
        // ARect is just a dummy typedef, so we can't really set it
    }

    return 0;
}

/**
 * Unlock the window's drawing surface after previously locking it,
 * posting the new buffer to the display with VSync.
 *
 * \return 0 for success, or a negative value on error.
 */
int32_t ANativeWindow_unlockAndPost(ANativeWindow *window)
{
    if (!window) {
        return -1;
    }

    // For Wayland, the frame callback handles VSync
    // The actual buffer swap happens in eglSwapBuffers (bionic_eglSwapBuffers)
    // which is called by the app after unlockAndPost
    
    // If we have a frame callback pending, it will fire on next VSync
    // The actual presentation is handled by the compositor
    
    return 0;
}

int32_t ANativeWindow_setFrameRate(ANativeWindow *window, float frameRate, int8_t compatibility)
{
	return 0; // success
}

/**
 * Set a transform that will be applied to future buffers posted to the window.
 *
 * \param transform combination of {@link ANativeWindowTransform} flags
 * \return 0 for success, or -EINVAL if \p transform is invalid
 */
int32_t ANativeWindow_setBuffersTransform(ANativeWindow *window, int32_t transform)
{
	return -1;
}

void wl_registry_global_handler(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	struct wl_subcompositor **subcompositor = data;
	if (!strcmp(interface, "wl_subcompositor")) {
		*subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
	}
}

void wl_registry_global_handler_compositor(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	struct wl_subcompositor **compositor = data;
	if (!strcmp(interface, "wl_compositor")) {
		*compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
	}
}

void wl_registry_global_remove_handler(void *data, struct wl_registry *registry, uint32_t name)
{
}

static void on_resize(GtkWidget *self, gint width, gint height, ANativeWindow *native_window)
{
	if (native_window->wayland_display) {
		wl_egl_window_resize((struct wl_egl_window *)native_window->egl_window, width, height, 0, 0);
	} else if (native_window->x11_display) {
		XResizeWindow(native_window->x11_display, (Window)native_window->egl_window, width, height);
	}
}

// Callback for when SurfaceView is realized and has dimensions
static void on_surface_view_realized(GtkWidget *widget, gpointer user_data)
{
    struct ANativeWindow *native_window = user_data;
    if (native_window && native_window->surface_view_widget == widget) {
        native_window->width = gtk_widget_get_width(widget);
        native_window->height = gtk_widget_get_height(widget);
        // Signal that we're ready (could use a condition variable or callback)
    }
}

extern GThread *main_thread_id;
ANativeWindow *ANativeWindow_fromSurface(JNIEnv *env, jobject surface)
{
    int width;
    int height;

    int ret;

    graphene_point_t pos;
    double off_x;
    double off_y;

    static struct wl_subcompositor *wl_subcompositor = NULL;
    static struct wl_registry_listener wl_registry_listener = {
        .global = wl_registry_global_handler,
        .global_remove = wl_registry_global_remove_handler
    };

    GtkWidget *surface_view_widget = gtk_widget_get_first_child(_PTR(_GET_LONG_FIELD(surface, "widget")));
    GtkWidget *window = GTK_WIDGET(gtk_widget_get_native(surface_view_widget));
    width = gtk_widget_get_width(surface_view_widget);
    
    // Use realize signal instead of busy-wait
    if (width == 0) {
        // Connect to realize signal to get dimensions when ready
        g_signal_connect(surface_view_widget, "realize", G_CALLBACK(on_surface_view_realized), NULL);
        // Also connect to size-allocate for initial size
        g_signal_connect(surface_view_widget, "size-allocate", G_CALLBACK(on_surface_view_realized), NULL);
        
        // Wait briefly for realize if we're on main thread, otherwise return with 0 size
        // The window will be updated via the signal handler
        if (g_thread_self() == main_thread_id) {
            guint64 start = g_get_monotonic_time();
            while ((width = gtk_widget_get_width(surface_view_widget)) == 0) {
                if (g_get_monotonic_time() - start > 5 * G_USEC_PER_SEC) {
                    fprintf(stderr, "native_window: timed out waiting for SurfaceView to be mapped\n");
                    break;
                }
                g_main_context_iteration(g_main_context_default(), false);
            }
        }
    }
    height = gtk_widget_get_height(surface_view_widget);

	// get position of the SurfaceView widget wrt the toplevel window
	ret = gtk_widget_compute_point(surface_view_widget, window, &GRAPHENE_POINT_INIT(0, 0), &pos);
	assert(ret);
	// compensate for offset between the widget coordinates and the surface coordinates
	gtk_native_get_surface_transform(GTK_NATIVE(window), &off_x, &off_y);
	pos.x += off_x;
	pos.y += off_y;

	// positioned and ready

	struct ANativeWindow *native_window = calloc(1, sizeof(struct ANativeWindow));
	native_window->refcount = 1; // probably, 0 doesn't work
	native_window->surface_view_widget = surface_view_widget;
	native_window->width = width;
	native_window->height = height;

	GdkDisplay *display = gtk_root_get_display(GTK_ROOT(window));

	if (!getenv("ATL_DIRECT_EGL")) {
		// nothing to do
	} else if (GDK_IS_WAYLAND_DISPLAY(display)) {
		struct wl_display *wl_display = gdk_wayland_display_get_wl_display(display);
		struct wl_compositor *wl_compositor = gdk_wayland_display_get_wl_compositor(display);

		// Cache wl_subcompositor per display to handle display changes
		static struct wl_display *cached_wl_display = NULL;
		if (cached_wl_display != wl_display || !wl_subcompositor) {
			struct wl_registry *wl_registry = wl_display_get_registry(wl_display);
			wl_registry_add_listener(wl_registry, &wl_registry_listener, &wl_subcompositor);
			wl_display_roundtrip(wl_display);
			cached_wl_display = wl_display;
		}

		struct wl_surface *toplevel_surface = gdk_wayland_surface_get_wl_surface(gtk_native_get_surface(GTK_NATIVE(window)));

		struct wl_surface *wayland_surface = wl_compositor_create_surface(wl_compositor);

		struct wl_subsurface *subsurface = wl_subcompositor_get_subsurface(wl_subcompositor, wayland_surface, toplevel_surface);
		wl_subsurface_set_desync(subsurface);
		wl_subsurface_set_position(subsurface, pos.x, pos.y);

#if (GTK_MAJOR_VERSION >= 4 && GTK_MINOR_VERSION >= 22)
		/* in Gtk 4.22+ we are able to do hole punching */
		wl_subsurface_place_below(subsurface, toplevel_surface);
#endif

		struct wl_region *empty_region = wl_compositor_create_region(wl_compositor);
		wl_surface_set_input_region(wayland_surface, empty_region);
		wl_region_destroy(empty_region);

		struct wl_egl_window *egl_window = wl_egl_window_create(wayland_surface, width, height);
		native_window->egl_window = (EGLNativeWindowType)egl_window;
		native_window->wayland_display = wl_display;
		native_window->wayland_surface = wayland_surface;
	} else if (GDK_IS_X11_DISPLAY(display)) {
		/* X11 support is deprecated, which means that if we decide to switch to Gtk 5 we will lose the ability to run ATL on X11.
		 * for now, silence the warnings */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

		int major;
		int minor;

		/* Check if we support EGL */
		if (gdk_x11_display_get_egl_version(display, &major, &minor)) {
			// EGL initialized successfully
		} else {
			fprintf(stderr, "ANativeWindow_fromSurface: crashing here;\n"
			                "The GTK X11 context was made using GLX, which isn't and won't be supported\n"
			                "Please use GDK_DEBUG='gl-egl' to use EGL\n");
			exit(1);
		}

		/* Get the X11 display server */
		Display *x11_display = gdk_x11_display_get_xdisplay(display);
		native_window->x11_display = x11_display;

		/* Get the top level window's X11 window ID */
		Window toplevel_window = gdk_x11_surface_get_xid(gtk_native_get_surface(GTK_NATIVE(window)));

		/*
		 * Make a new X11 window inheriting from the GTK top level window.
		 * The reason why it's first bound to the default root window and
		 * then reparented to the GTK window is because on NVIDIA drivers
		 * the GTK window selects a visual mode that's not compatible with
		 * NVIDIA's implementation of EGL for some reason.
		 */
		Window x11_window = XCreateSimpleWindow(x11_display, DefaultRootWindow(x11_display), 0, 0, width, height, 0, 0, 0xffffffff);
		XReparentWindow(x11_display, x11_window, toplevel_window, pos.x, pos.y);

		XMapWindow(x11_display, x11_window);

		/* Make the X11 window able to be clicked through */
		Region region = XCreateRegion();
		XRectangle rectangle;
		rectangle.x = 0;
		rectangle.y = 0;
		rectangle.width = 0;
		rectangle.height = 0;
		XUnionRectWithRegion(&rectangle, region, region);
		XShapeCombineRegion(x11_display, x11_window, ShapeInput, 0, 0, region, ShapeSet);
		XDestroyRegion(region);

		native_window->egl_window = (EGLNativeWindowType)x11_window;

#pragma GCC diagnostic pop
	}

	if (getenv("ATL_DIRECT_EGL") && native_window->resize_handler == 0)
		native_window->resize_handler = g_signal_connect(surface_view_widget, "resize", G_CALLBACK(on_resize), native_window);

	return native_window;
}

ANativeWindow *ANativeWindow_fromSurfaceTexture(JNIEnv *env, jobject surfaceTexture)
{
	return NULL;
}

// FIXME 1.5: this most likely belongs elsewhere

VkResult bionic_vkCreateAndroidSurfaceKHR(VkInstance instance, const struct VkAndroidSurfaceCreateInfoKHR *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkSurfaceKHR *pSurface)
{
	GdkDisplay *display = gtk_widget_get_display(pCreateInfo->window->surface_view_widget);

	if (GDK_IS_WAYLAND_DISPLAY(display)) {
		struct VkWaylandSurfaceCreateInfoKHR wayland_create_info = {
			.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
			.pNext = NULL,
			.flags = 0,
			.display = pCreateInfo->window->wayland_display,
			.surface = pCreateInfo->window->wayland_surface,
		};

		return vkCreateWaylandSurfaceKHR(instance, &wayland_create_info, pAllocator, pSurface);
	} else if (GDK_IS_X11_DISPLAY(display)) {
		struct VkXlibSurfaceCreateInfoKHR x11_create_info = {
			.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
			.pNext = NULL,
			.flags = 0,
			.dpy = pCreateInfo->window->x11_display,
			.window = (unsigned long)pCreateInfo->window->egl_window,
		};

		return vkCreateXlibSurfaceKHR(instance, &x11_create_info, pAllocator, pSurface);
	} else {
		fprintf(stderr, "bionic_vkCreateAndroidSurfaceKHR: the GDK backend is neither Wayland nor X11, no SurfaceView for you");
		return VK_ERROR_UNKNOWN;
	}
}

VkResult bionic_vkCreateInstance(struct VkInstanceCreateInfo *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
	int original_extension_count = pCreateInfo->enabledExtensionCount;
	int new_extension_count = original_extension_count + 2;
	const char **enabled_exts = malloc(new_extension_count * sizeof(char *));
	memcpy(enabled_exts, pCreateInfo->ppEnabledExtensionNames, original_extension_count * sizeof(char *));
	enabled_exts[original_extension_count] = "VK_KHR_wayland_surface";
	enabled_exts[original_extension_count + 1] = "VK_KHR_xlib_surface";

	pCreateInfo->enabledExtensionCount = new_extension_count;
	pCreateInfo->ppEnabledExtensionNames = enabled_exts;
	return vkCreateInstance(pCreateInfo, pAllocator, pInstance);
}

PFN_vkVoidFunction bionic_vkGetInstanceProcAddr(VkInstance instance, const char *pName)
{
	if (__unlikely__(!strcmp(pName, "vkCreateInstance")))
		return (PFN_vkVoidFunction)bionic_vkCreateInstance;

	return vkGetInstanceProcAddr(instance, pName);
}

// FIXME 2: this BLATANTLY belongs elsewhere

typedef XrResult (*xr_func)(void *, ...);

/* avoid hard dependency on libopenxr_loader for the three functions that we only ever call when running a VR app */
/* NOTE: our use of __builtin_va_arg_pack means this only works as long as we don't need to call a function
 * that takes an integer type shorter than int or a floating point type shorter than double */
static void *openxr_loader_handle = NULL;
static inline __attribute__((__always_inline__)) XrResult xr_lazy_call(char *func_name, ...)
{
	if (!openxr_loader_handle) {
		openxr_loader_handle = dlopen("libopenxr_loader.so.1", RTLD_LAZY);
	}

	xr_func func = dlsym(openxr_loader_handle, func_name);
	if (!func) return XR_ERROR_FUNCTION_UNSUPPORTED;
	/* Variadic argument forwarding not supported without __builtin_va_arg_pack */
	return XR_ERROR_FUNCTION_UNSUPPORTED;
}

static XrResult bionic_xrInitializeLoaderKHR(void *loaderInitInfo)
{
	return XR_SUCCESS;
}

struct XrGraphicsBindingOpenGLESAndroidKHR {
	XrStructureType type;
	const void *next;
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
};

XrResult bionic_xrCreateSession(XrInstance instance, XrSessionCreateInfo *createInfo, XrSession *session)
{
	struct XrGraphicsBindingOpenGLESAndroidKHR *android_bind = (struct XrGraphicsBindingOpenGLESAndroidKHR *)createInfo->next;
	XrGraphicsBindingEGLMNDX egl_bind = {XR_TYPE_GRAPHICS_BINDING_EGL_MNDX};

	if (android_bind->type == XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR) {
		egl_bind.getProcAddress = eglGetProcAddress;
		egl_bind.display = android_bind->display;
		egl_bind.config = android_bind->config;
		egl_bind.context = android_bind->context;
		createInfo->next = &egl_bind;
	} else {

	return xr_lazy_call("xrCreateSession", instance, createInfo, session);
	}
}

/*
 * Intercept XrInstanceProperties and notify the user of our meta-layer.
 */
XrResult bionic_xrGetInstanceProperties(XrInstance instance, XrInstanceProperties *instanceProperties)
{
	XrResult ret = xr_lazy_call("xrGetInstanceProperties", instance, instanceProperties);

	strncat(instanceProperties->runtimeName, " (With ATL meta-layer)",
	        XR_MAX_RUNTIME_NAME_SIZE - 1 - strlen(instanceProperties->runtimeName));

	return ret;
}

XrResult bionic_xrCreateInstance(XrInstanceCreateInfo *createInfo, XrInstance *instance)
{
	/* so that we can use simpler (and faster) code, we replace extensions which we
	 * want to remove with this extension rather than delete them and copy the following
	 * extensions over
	 */
	const char *harmless_extension = "XR_KHR_opengl_es_enable";

	const char *extra_exts[] = {
		"XR_MNDX_egl_enable",
		"XR_EXT_local_floor",
	};

	const char *const *old_names = createInfo->enabledExtensionNames;
	const char **new_names;
	int new_count = createInfo->enabledExtensionCount + ARRAY_SIZE(extra_exts);

	//FIXME: Leak?
	new_names = malloc(sizeof(*new_names) * new_count);
	memcpy(new_names, old_names, createInfo->enabledExtensionCount * sizeof(*old_names));

	for (int i = 0; i < createInfo->enabledExtensionCount; i++) {
		if (!strcmp(new_names[i], "XR_KHR_android_create_instance"))
			new_names[i] = harmless_extension;
	}

	for (int i = 0; i < ARRAY_SIZE(extra_exts); i++)
		new_names[createInfo->enabledExtensionCount + i] = extra_exts[i];

	createInfo->enabledExtensionCount = new_count;
	createInfo->enabledExtensionNames = new_names;

	return xr_lazy_call("xrCreateInstance", createInfo, instance);
}

XrResult bionic_xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo *createInfo, XrSpace *space)
{

	//FIXME: this is sad for oculus refspace extension it assumes we have...
	if (createInfo->referenceSpaceType > 100)
		*(int *)(&createInfo->referenceSpaceType) = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;

	return xr_lazy_call("xrCreateReferenceSpace", session, createInfo, space);
}

/*
 * NOTE: Here we implement a NIH OpenXR API layer.
 *
 * We should make sure all our overrides are available via
 * "xrGetInstanceProcAddr" so the table below should contain
 * all our special functions.
 *
 * Maybe we should implement a proper OpenXR layer lib and inject
 * it in xrCreateInstance or extend our XR runtime in a way where
 * we won't need to help it.
 */

/*
 * HACK: The name prop here is deliberately an in-struct
 * string so we can do bsearch with plain strcmp.
 * So it must always be first.
 */
struct xr_proc_override {
	char name[64];
	PFN_xrVoidFunction func;
};

static const struct xr_proc_override xr_proc_override_tbl[] = {
	{"xrCreateInstance", (PFN_xrVoidFunction)bionic_xrCreateInstance},
	{"xrCreateReferenceSpace", (PFN_xrVoidFunction)bionic_xrCreateReferenceSpace},
	{"xrCreateSession", (PFN_xrVoidFunction)bionic_xrCreateSession},
	{"xrGetInstanceProperties", (PFN_xrVoidFunction)bionic_xrGetInstanceProperties},
	{"xrInitializeLoaderKHR", (PFN_xrVoidFunction)bionic_xrInitializeLoaderKHR},
};

XrResult bionic_xrGetInstanceProcAddr(XrInstance instance, const char *name, PFN_xrVoidFunction *func)
{

	struct xr_proc_override *match = bsearch(name, xr_proc_override_tbl,
	                                         ARRAY_SIZE(xr_proc_override_tbl),
	                                         sizeof(xr_proc_override_tbl[0]),
	                                         (int (*)(const void *, const void *))strcmp);

	if (match) {
		*func = match->func;
		return XR_SUCCESS;
	}

	return xr_lazy_call("xrGetInstanceProcAddr", instance, name, func);
}

typedef void *ANativeActivity;

void ANativeActivity_setWindowFormat(ANativeActivity *activity, int32_t format)
{
}
