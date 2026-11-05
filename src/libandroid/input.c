#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <graphene.h>

#include "looper.h"

enum {
	AINPUT_EVENT_TYPE_KEY = 1,
	AINPUT_EVENT_TYPE_MOTION = 2,
	AINPUT_EVENT_TYPE_FOCUS = 3,
	AINPUT_EVENT_TYPE_CAPTURE = 4,
	AINPUT_EVENT_TYPE_DRAG = 5,
	AINPUT_EVENT_TYPE_TOUCH_MODE = 6
};

enum {
	AINPUT_SOURCE_CLASS_MASK = 0x000000ff,
	AINPUT_SOURCE_CLASS_NONE = 0x00000000,
	AINPUT_SOURCE_CLASS_BUTTON = 0x00000001,
	AINPUT_SOURCE_CLASS_POINTER = 0x00000002,
	AINPUT_SOURCE_CLASS_NAVIGATION = 0x00000004,
	AINPUT_SOURCE_CLASS_POSITION = 0x00000008,
	AINPUT_SOURCE_CLASS_JOYSTICK = 0x00000010
};

enum {
	AINPUT_SOURCE_UNKNOWN = 0x00000000,
	AINPUT_SOURCE_KEYBOARD = 0x00000100 | AINPUT_SOURCE_CLASS_BUTTON,
	AINPUT_SOURCE_DPAD = 0x00000200 | AINPUT_SOURCE_CLASS_BUTTON,
	AINPUT_SOURCE_GAMEPAD = 0x00000400 | AINPUT_SOURCE_CLASS_BUTTON,
	AINPUT_SOURCE_TOUCHSCREEN = 0x00001000 | AINPUT_SOURCE_CLASS_POINTER,
	AINPUT_SOURCE_MOUSE = 0x00002000 | AINPUT_SOURCE_CLASS_POINTER,
	AINPUT_SOURCE_STYLUS = 0x00004000 | AINPUT_SOURCE_CLASS_POINTER,
	AINPUT_SOURCE_BLUETOOTH_STYLUS = 0x00008000 | AINPUT_SOURCE_STYLUS,
	AINPUT_SOURCE_TRACKBALL = 0x00010000 | AINPUT_SOURCE_CLASS_NAVIGATION,
	AINPUT_SOURCE_MOUSE_RELATIVE = 0x00020000 | AINPUT_SOURCE_CLASS_NAVIGATION,
	AINPUT_SOURCE_TOUCHPAD = 0x00100000 | AINPUT_SOURCE_CLASS_POSITION,
	AINPUT_SOURCE_TOUCH_NAVIGATION = 0x00200000 | AINPUT_SOURCE_CLASS_NONE,
	AINPUT_SOURCE_JOYSTICK = 0x01000000 | AINPUT_SOURCE_CLASS_JOYSTICK,
	AINPUT_SOURCE_HDMI = 0x02000000 | AINPUT_SOURCE_CLASS_BUTTON,
	AINPUT_SOURCE_SENSOR = 0x04000000 | AINPUT_SOURCE_CLASS_NONE,
	AINPUT_SOURCE_ROTARY_ENCODER = 0x00400000 | AINPUT_SOURCE_CLASS_NONE,
	AINPUT_SOURCE_ANY = 0xffffff00
};

enum {
	AMOTION_EVENT_ACTION_MASK = 0xff,
	AMOTION_EVENT_ACTION_POINTER_INDEX_MASK = 0xff00,
	AMOTION_EVENT_ACTION_DOWN = 0,
	AMOTION_EVENT_ACTION_UP = 1,
	AMOTION_EVENT_ACTION_MOVE = 2,
	AMOTION_EVENT_ACTION_CANCEL = 3,
	AMOTION_EVENT_ACTION_OUTSIDE = 4,
	AMOTION_EVENT_ACTION_POINTER_DOWN = 5,
	AMOTION_EVENT_ACTION_POINTER_UP = 6,
	AMOTION_EVENT_ACTION_HOVER_MOVE = 7,
	AMOTION_EVENT_ACTION_SCROLL = 8,
	AMOTION_EVENT_ACTION_HOVER_ENTER = 9,
	AMOTION_EVENT_ACTION_HOVER_EXIT = 10,
	AMOTION_EVENT_ACTION_BUTTON_PRESS = 11,
	AMOTION_EVENT_ACTION_BUTTON_RELEASE = 12
};

// Per-event allocation with multitouch support
struct AInputEvent {
    int32_t type;
    int32_t source;
    int32_t action;
    int32_t action_button;
    int64_t event_time;
    int32_t pointer_count;
    int32_t *pointer_ids;
    float *pointer_x;
    float *pointer_y;
    float *pointer_pressure;
    float *pointer_size;
    // Key event fields
    int32_t key_code;
    int32_t meta_state;
    int32_t repeat_count;
};

typedef void AInputQueue;

struct ALooper;
typedef int (*Looper_callbackFunc)(int fd, int events, void *data);

struct android_poll_source {
    int32_t id;
    struct android_app *app;
    void (*process)(struct android_app *app, struct android_poll_source *source);
};

// Input queue with event ring buffer
struct input_queue {
    int fd;
    GtkEventController *touch_controller;
    GtkEventController *motion_controller;
    GtkEventController *key_controller;
    GtkEventController *scroll_controller;
    GAsyncQueue *event_queue;
    gboolean destroyed;
};

static GList *all_input_queues = NULL;

// Create a new input event (caller must free with ainput_event_free)
static struct AInputEvent *ainput_event_new(int32_t type, int32_t source)
{
    struct AInputEvent *event = calloc(1, sizeof(struct AInputEvent));
    if (!event) return NULL;
    event->type = type;
    event->source = source;
    event->event_time = g_get_monotonic_time() / 1000; // milliseconds
    return event;
}

void ainput_event_free(struct AInputEvent *event)
{
    if (!event) return;
    free(event->pointer_ids);
    free(event->pointer_x);
    free(event->pointer_y);
    free(event->pointer_pressure);
    free(event->pointer_size);
    free(event);
}

// Allocate pointer arrays for multitouch
static void ainput_event_alloc_pointers(struct AInputEvent *event, int32_t count)
{
    if (event->pointer_count >= count) return;
    
    event->pointer_ids = realloc(event->pointer_ids, count * sizeof(int32_t));
    event->pointer_x = realloc(event->pointer_x, count * sizeof(float));
    event->pointer_y = realloc(event->pointer_y, count * sizeof(float));
    event->pointer_pressure = realloc(event->pointer_pressure, count * sizeof(float));
    event->pointer_size = realloc(event->pointer_size, count * sizeof(float));
    
    for (int i = event->pointer_count; i < count; i++) {
        event->pointer_ids[i] = i;
        event->pointer_x[i] = 0;
        event->pointer_y[i] = 0;
        event->pointer_pressure[i] = 1.0f;
        event->pointer_size[i] = 1.0f;
    }
    event->pointer_count = count;
}

// Coordinate conversion helper
static void convert_event_coords(GtkWidget *window, GtkWidget *controller_widget, double *x, double *y)
{
    GtkWidget *child = gtk_window_get_child(GTK_WINDOW(window));
    if (child) {
        graphene_point_t p;
        int ret = gtk_widget_compute_point(window, child, &GRAPHENE_POINT_INIT(*x, *y), &p);
        if (ret) {
            *x = p.x;
            *y = p.y;
        }
    }
}

// Touch event handlers
static void on_touch_down(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    struct input_queue *queue = user_data;
    if (queue->destroyed) return;
    
    GtkWidget *window = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    convert_event_coords(window, GTK_WIDGET(gesture), &x, &y);
    
    // For now, handle single touch; multitouch would need tracking sequence state
    struct AInputEvent *event = ainput_event_new(AINPUT_EVENT_TYPE_MOTION, AINPUT_SOURCE_TOUCHSCREEN);
    event->action = AMOTION_EVENT_ACTION_DOWN;
    event->pointer_count = 1;
    ainput_event_alloc_pointers(event, 1);
    event->pointer_ids[0] = 0;
    event->pointer_x[0] = (float)x;
    event->pointer_y[0] = (float)y;
    event->pointer_pressure[0] = 1.0f;
    
    g_async_queue_push(queue->event_queue, event);
}

static void on_touch_up(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    struct input_queue *queue = user_data;
    if (queue->destroyed) return;
    
    GtkWidget *window = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    convert_event_coords(window, GTK_WIDGET(gesture), &x, &y);
    
    struct AInputEvent *event = ainput_event_new(AINPUT_EVENT_TYPE_MOTION, AINPUT_SOURCE_TOUCHSCREEN);
    event->action = AMOTION_EVENT_ACTION_UP;
    event->pointer_count = 1;
    ainput_event_alloc_pointers(event, 1);
    event->pointer_ids[0] = 0;
    event->pointer_x[0] = (float)x;
    event->pointer_y[0] = (float)y;
    event->pointer_pressure[0] = 0.0f;
    
    g_async_queue_push(queue->event_queue, event);
}

static void on_touch_motion(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data)
{
    struct input_queue *queue = user_data;
    if (queue->destroyed) return;
    
    GtkWidget *window = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    convert_event_coords(window, GTK_WIDGET(gesture), &x, &y);
    
    struct AInputEvent *event = ainput_event_new(AINPUT_EVENT_TYPE_MOTION, AINPUT_SOURCE_TOUCHSCREEN);
    event->action = AMOTION_EVENT_ACTION_MOVE;
    event->pointer_count = 1;
    ainput_event_alloc_pointers(event, 1);
    event->pointer_ids[0] = 0;
    event->pointer_x[0] = (float)x;
    event->pointer_y[0] = (float)y;
    event->pointer_pressure[0] = 1.0f;
    
    g_async_queue_push(queue->event_queue, event);
}

// Mouse motion handler
static void on_mouse_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data)
{
    struct input_queue *queue = user_data;
    if (queue->destroyed) return;
    
    GtkWidget *window = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
    convert_event_coords(window, GTK_WIDGET(controller), &x, &y);
    
    struct AInputEvent *event = ainput_event_new(AINPUT_EVENT_TYPE_MOTION, AINPUT_SOURCE_MOUSE);
    event->action = AMOTION_EVENT_ACTION_MOVE;
    event->pointer_count = 1;
    ainput_event_alloc_pointers(event, 1);
    event->pointer_ids[0] = 0;
    event->pointer_x[0] = (float)x;
    event->pointer_y[0] = (float)y;
    event->pointer_pressure[0] = 1.0f;
    
    g_async_queue_push(queue->event_queue, event);
}

// Mouse button handler (using legacy for buttons)
static gboolean on_button_event(GtkEventControllerLegacy *controller, GdkEvent *event, gpointer user_data)
{
    struct input_queue *queue = user_data;
    if (queue->destroyed) return FALSE;
    
    double x, y;
    gdk_event_get_position(event, &x, &y);
    GtkWidget *window = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
    convert_event_coords(window, GTK_WIDGET(controller), &x, &y);
    
    struct AInputEvent *ev = ainput_event_new(AINPUT_EVENT_TYPE_MOTION, AINPUT_SOURCE_MOUSE);
    ev->pointer_count = 1;
    ainput_event_alloc_pointers(ev, 1);
    ev->pointer_ids[0] = 0;
    ev->pointer_x[0] = (float)x;
    ev->pointer_y[0] = (float)y;
    ev->pointer_pressure[0] = 1.0f;
    
    switch (gdk_event_get_event_type(event)) {
        case GDK_BUTTON_PRESS:
            ev->action = AMOTION_EVENT_ACTION_DOWN;
            ev->action_button = gdk_button_event_get_button((GdkEvent *)event);
            break;
        case GDK_BUTTON_RELEASE:
            ev->action = AMOTION_EVENT_ACTION_UP;
            ev->action_button = gdk_button_event_get_button((GdkEvent *)event);
            break;
        default:
            ainput_event_free(ev);
            return FALSE;
    }
    
    g_async_queue_push(queue->event_queue, ev);
    return TRUE;
}

// Key event handler
static gboolean on_key_event(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    struct input_queue *queue = user_data;
    if (queue->destroyed) return FALSE;
    
    struct AInputEvent *event = ainput_event_new(AINPUT_EVENT_TYPE_KEY, AINPUT_SOURCE_KEYBOARD);
    event->action = AMOTION_EVENT_ACTION_DOWN; // Reuse for key down
    event->key_code = (int32_t)keyval; // Map GDK keyval to Android keycode
    event->meta_state = (int32_t)state;
    event->repeat_count = 0;
    
    g_async_queue_push(queue->event_queue, event);
    return TRUE;
}

static gboolean on_key_released(GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    struct input_queue *queue = user_data;
    if (queue->destroyed) return FALSE;
    
    struct AInputEvent *event = ainput_event_new(AINPUT_EVENT_TYPE_KEY, AINPUT_SOURCE_KEYBOARD);
    event->action = AMOTION_EVENT_ACTION_UP; // Reuse for key up
    event->key_code = (int32_t)keyval;
    event->meta_state = (int32_t)state;
    event->repeat_count = 0;
    
    g_async_queue_push(queue->event_queue, event);
    return TRUE;
}

// Scroll event handler
static gboolean on_scroll_event(GtkEventControllerScroll *controller, double dx, double dy, gpointer user_data)
{
    struct input_queue *queue = user_data;
    if (queue->destroyed) return FALSE;
    
    struct AInputEvent *event = ainput_event_new(AINPUT_EVENT_TYPE_MOTION, AINPUT_SOURCE_MOUSE);
    event->action = AMOTION_EVENT_ACTION_SCROLL;
    event->pointer_count = 1;
    ainput_event_alloc_pointers(event, 1);
    event->pointer_ids[0] = 0;
    event->pointer_x[0] = (float)dx * 10.0f; // Scroll distance
    event->pointer_y[0] = (float)dy * 10.0f;
    
    g_async_queue_push(queue->event_queue, event);
    return TRUE;
}

float AMotionEvent_getAxisValue(const struct AInputEvent *motion_event, int32_t axis, size_t pointer_index)
{
    if (!motion_event || pointer_index >= motion_event->pointer_count) return -1;
    switch (axis) {
        case 0: return motion_event->pointer_x[pointer_index]; // AXIS_X
        case 1: return motion_event->pointer_y[pointer_index]; // AXIS_Y
        case 2: return motion_event->pointer_pressure[pointer_index]; // AXIS_PRESSURE
        case 3: return motion_event->pointer_size[pointer_index]; // AXIS_SIZE
        default: return -1;
    }
}

size_t AMotionEvent_getPointerCount(const struct AInputEvent *motion_event)
{
    if (motion_event) return motion_event->pointer_count;
    return 0;
}

int32_t AInputEvent_getType(const struct AInputEvent *event)
{
    if (event) return event->type;
    return -1;
}

int32_t AInputEvent_getSource(const struct AInputEvent *event)
{
    if (event) return event->source;
    return -1;
}

int32_t AMotionEvent_getAction(const struct AInputEvent *motion_event)
{
    if (motion_event) return motion_event->action;
    return -1;
}

int32_t AMotionEvent_getPointerId(const struct AInputEvent *motion_event, size_t pointer_index)
{
    if (motion_event && pointer_index < motion_event->pointer_count) {
        return motion_event->pointer_ids[pointer_index];
    }
    return -1;
}

float AMotionEvent_getX(const struct AInputEvent *motion_event, size_t pointer_index)
{
    if (motion_event && pointer_index < motion_event->pointer_count) {
        return motion_event->pointer_x[pointer_index];
    }
    return -1;
}

float AMotionEvent_getY(const struct AInputEvent *motion_event, size_t pointer_index)
{
    if (motion_event && pointer_index < motion_event->pointer_count) {
        return motion_event->pointer_y[pointer_index];
    }
    return -1;
}

void AInputQueue_detachLooper(AInputQueue *queue)
{
    // The queue will be cleaned up by input_queue_cleanup_all
}

struct input_queue *input_queue_new(void)
{
    struct input_queue *queue = calloc(1, sizeof(struct input_queue));
    if (!queue) return NULL;
    queue->event_queue = g_async_queue_new();
    all_input_queues = g_list_prepend(all_input_queues, queue);
    return queue;
}

void input_queue_free(struct input_queue *queue)
{
    if (!queue) return;
    queue->destroyed = TRUE;
    
    // Disconnect controllers
    if (queue->touch_controller) {
        g_object_unref(queue->touch_controller);
        queue->touch_controller = NULL;
    }
    if (queue->motion_controller) {
        g_object_unref(queue->motion_controller);
        queue->motion_controller = NULL;
    }
    if (queue->key_controller) {
        g_object_unref(queue->key_controller);
        queue->key_controller = NULL;
    }
    if (queue->scroll_controller) {
        g_object_unref(queue->scroll_controller);
        queue->scroll_controller = NULL;
    }
    
    // Clear event queue
    if (queue->event_queue) {
        while (TRUE) {
            struct AInputEvent *event = g_async_queue_try_pop(queue->event_queue);
            if (!event) break;
            ainput_event_free(event);
        }
        g_async_queue_unref(queue->event_queue);
        queue->event_queue = NULL;
    }
    
    all_input_queues = g_list_remove(all_input_queues, queue);
    free(queue);
}

void input_queue_cleanup_all(void)
{
    for (GList *l = all_input_queues; l; l = l->next) {
        struct input_queue *queue = l->data;
        if (queue) {
            queue->destroyed = TRUE;
            // Push a dummy event to wake up any waiting threads
            if (queue->event_queue) {
                g_async_queue_push(queue->event_queue, NULL);
            }
        }
    }
    // Wait a bit for threads to wake up
    g_usleep(10000);
    
    // Now free all queues
    for (GList *l = all_input_queues; l; l = l->next) {
        input_queue_free(l->data);
    }
    g_list_free(all_input_queues);
    all_input_queues = NULL;
}

void AInputQueue_attachLooper(struct input_queue *queue, ALooper *looper, int ident, Looper_callbackFunc callback, void *data)
{
    struct android_poll_source *poll_source = (struct android_poll_source *)data;
    if (!poll_source || !queue) return;
    
    int input_queue_pipe[2];
    if (pipe(input_queue_pipe)) {
        fprintf(stderr, "could not create pipe: %s\n", strerror(errno));
        return;
    }
    fcntl(input_queue_pipe[0], F_SETFL, O_NONBLOCK);
    ALooper_addFd(looper, input_queue_pipe[0], ident, (1 << 0), callback, data);
    queue->fd = input_queue_pipe[0];
    
    // We also need to push events from async queue to pipe
    // This is handled by the looper callback reading from the async queue
    // For simplicity, we'll use the pipe as the primary mechanism
    // and have a separate thread or idle callback push events to the pipe
}

int32_t AInputQueue_getEvent(struct input_queue *queue, struct AInputEvent **outEvent)
{
    if (!queue || !queue->event_queue) return -1;
    
    // Wait for event with timeout
    struct AInputEvent *event = g_async_queue_timeout_pop(queue->event_queue, 100); // 100ms timeout
    if (event) {
        *outEvent = event;
        return 0;
    }
    return -1; // timeout or no events
}

int32_t AInputQueue_preDispatchEvent(AInputQueue *queue, struct AInputEvent *event)
{
    return 0; // we don't want to claim the event for ourselves
}

void AInputQueue_finishEvent(AInputQueue *queue, struct AInputEvent *event, int handled)
{
    // Free the event after processing
    ainput_event_free(event);
}

int32_t AKeyEvent_getKeyCode(struct AInputEvent *event)
{
    if (!event || event->type != AINPUT_EVENT_TYPE_KEY) {
        fprintf(stderr, "AKeyEvent_getKeyCode: not a key event\n");
        return 0;
    }
    return event->key_code;
}

int32_t AKeyEvent_getMetaState(struct AInputEvent *event)
{
    if (!event || event->type != AINPUT_EVENT_TYPE_KEY) return 0;
    return event->meta_state;
}

int32_t AKeyEvent_getRepeatCount(struct AInputEvent *event)
{
    if (!event || event->type != AINPUT_EVENT_TYPE_KEY) return 0;
    return event->repeat_count;
}

// Attach input queue to a GTK window
void input_queue_attach_widget(struct input_queue *queue, GtkWidget *window)
{
    if (!queue || !window || queue->destroyed) return;
    
    // Touch controller for multitouch
    queue->touch_controller = GTK_EVENT_CONTROLLER(gtk_gesture_click_new());
    g_signal_connect(queue->touch_controller, "pressed", G_CALLBACK(on_touch_down), queue);
    g_signal_connect(queue->touch_controller, "released", G_CALLBACK(on_touch_up), queue);
    gtk_widget_add_controller(window, queue->touch_controller);
    
    // Motion controller for mouse hover/motion
    queue->motion_controller = GTK_EVENT_CONTROLLER(gtk_event_controller_motion_new());
    g_signal_connect(queue->motion_controller, "motion", G_CALLBACK(on_mouse_motion), queue);
    gtk_widget_add_controller(window, queue->motion_controller);
    
    // Key controller
    queue->key_controller = GTK_EVENT_CONTROLLER(gtk_event_controller_key_new());
    g_signal_connect(queue->key_controller, "key-pressed", G_CALLBACK(on_key_event), queue);
    g_signal_connect(queue->key_controller, "key-released", G_CALLBACK(on_key_released), queue);
    gtk_widget_add_controller(window, queue->key_controller);
    
    // Scroll controller
    queue->scroll_controller = GTK_EVENT_CONTROLLER(gtk_event_controller_scroll_new(GTK_EVENT_CONTROLLER_SCROLL_VERTICAL | GTK_EVENT_CONTROLLER_SCROLL_HORIZONTAL));
    g_signal_connect(queue->scroll_controller, "scroll", G_CALLBACK(on_scroll_event), queue);
    gtk_widget_add_controller(window, queue->scroll_controller);
    
    // Legacy button events (for mouse buttons)
    GtkEventControllerLegacy *legacy = GTK_EVENT_CONTROLLER_LEGACY(gtk_event_controller_legacy_new());
    g_signal_connect(legacy, "event", G_CALLBACK(on_button_event), queue);
    gtk_widget_add_controller(window, GTK_EVENT_CONTROLLER(legacy));
}