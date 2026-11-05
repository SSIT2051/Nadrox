#ifndef ATL_JNI_TABLE_H
#define ATL_JNI_TABLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* JNIEnv;
typedef void* jobject;
typedef void* jclass;
typedef void* jmethodID;
typedef void* jfieldID;

/* JNI method cache entry */
typedef struct {
    JNIEnv *env;
    jmethodID method_id;
    const char *method_name;
    const char *signature;
    void *thunk;
    void *translated_func;
    bool is_static;
} jni_cache_entry_t;

/* Initialize JNI method cache */
bool atl_jni_table_init(void);

/* Shutdown JNI method cache */
void atl_jni_table_shutdown(void);

/* Register a JNI method mapping */
bool atl_jni_register_method(JNIEnv *env, jmethodID method_id, 
                             const char *method_name, const char *signature,
                             void *translated_func);

/* Get cached thunk for JNI method */
void *atl_jni_get_method_thunk(JNIEnv *env, jmethodID method_id, 
                                const char *method_name, const char *signature);

/* Handle callback from ARM code into JNI */
void *atl_jni_callback_invoke(JNIEnv *env, jmethodID method_id, 
                              void *args, bool is_static, bool is_varargs);

/* Thread-local JNIEnv management */
JNIEnv *atl_jni_get_thread_env(void);
void atl_jni_set_thread_env(JNIEnv *env);

#ifdef __cplusplus
}
#endif

#endif /* ATL_JNI_TABLE_H */
