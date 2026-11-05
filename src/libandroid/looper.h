#ifndef LOOPER_H
#define LOOPER_H

#include <gtk/gtk.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void ALooper;
typedef int (*Looper_callbackFunc)(int fd, int events, void *data);

ALooper *ALooper_prepare(int opts);
void ALooper_wake(ALooper *looper);
bool ALooper_isPolling(ALooper *looper);
int ALooper_pollOnce(int timeoutMillis, int *outFd, int *outEvents, void **outData);
int ALooper_addFd(ALooper *looper, int fd, int ident, int events, Looper_callbackFunc callback, void *data);

// Input queue types
typedef void AInputQueue;

struct input_queue;
struct AInputEvent;

struct input_queue *input_queue_new(void);
void input_queue_free(struct input_queue *queue);
void input_queue_cleanup_all(void);
void input_queue_attach_widget(struct input_queue *queue, GtkWidget *window);

void AInputQueue_attachLooper(struct input_queue *queue, ALooper *looper, int ident, Looper_callbackFunc callback, void *data);
int32_t AInputQueue_getEvent(struct input_queue *queue, struct AInputEvent **outEvent);
int32_t AInputQueue_preDispatchEvent(AInputQueue *queue, struct AInputEvent *event);
void AInputQueue_finishEvent(AInputQueue *queue, struct AInputEvent *event, int handled);
void AInputQueue_detachLooper(AInputQueue *queue);

// Motion event functions
float AMotionEvent_getAxisValue(const struct AInputEvent *motion_event, int32_t axis, size_t pointer_index);
size_t AMotionEvent_getPointerCount(const struct AInputEvent *motion_event);
int32_t AMotionEvent_getAction(const struct AInputEvent *motion_event);
int32_t AMotionEvent_getPointerId(const struct AInputEvent *motion_event, size_t pointer_index);
float AMotionEvent_getX(const struct AInputEvent *motion_event, size_t pointer_index);
float AMotionEvent_getY(const struct AInputEvent *motion_event, size_t pointer_index);

// General input event functions
int32_t AInputEvent_getType(const struct AInputEvent *event);
int32_t AInputEvent_getSource(const struct AInputEvent *event);

// Key event functions
int32_t AKeyEvent_getKeyCode(struct AInputEvent *event);
int32_t AKeyEvent_getMetaState(struct AInputEvent *event);
int32_t AKeyEvent_getRepeatCount(struct AInputEvent *event);

// Free event
void ainput_event_free(struct AInputEvent *event);

#endif
