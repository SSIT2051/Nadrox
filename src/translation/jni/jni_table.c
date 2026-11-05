/*
 * JNI Method Cache
 * Maps JNIEnv* method IDs to generated thunks, handles callbacks from ARM to JNI
 */

#define _GNU_SOURCE
#include "jni_table.h"
#include "thunk_gen.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define JNI_CACHE_SIZE 4096

static struct {
    jni_cache_entry_t entries[JNI_CACHE_SIZE];
    size_t count;
    pthread_mutex_t mutex;
    bool initialized;
} g_jni_cache;

static pthread_key_t g_jni_env_key;
static bool g_jni_env_key_created = false;

bool atl_jni_table_init(void)
{
    if (g_jni_cache.initialized) return true;
    memset(&g_jni_cache, 0, sizeof(g_jni_cache));
    pthread_mutex_init(&g_jni_cache.mutex, NULL);
    
    if (!g_jni_env_key_created) {
        pthread_key_create(&g_jni_env_key, NULL);
        g_jni_env_key_created = true;
    }
    
    atl_thunk_gen_init();
    
    g_jni_cache.initialized = true;
    return true;
}

void atl_jni_table_shutdown(void)
{
    if (!g_jni_cache.initialized) return;
    pthread_mutex_lock(&g_jni_cache.mutex);
    for (size_t i = 0; i < g_jni_cache.count; i++) {
        if (g_jni_cache.entries[i].method_name) {
            free((void *)g_jni_cache.entries[i].method_name);
        }
        if (g_jni_cache.entries[i].signature) {
            free((void *)g_jni_cache.entries[i].signature);
        }
    }
    g_jni_cache.count = 0;
    pthread_mutex_unlock(&g_jni_cache.mutex);
    pthread_mutex_destroy(&g_jni_cache.mutex);
    
    if (g_jni_env_key_created) {
        pthread_key_delete(g_jni_env_key);
        g_jni_env_key_created = false;
    }
    
    g_jni_cache.initialized = false;
}

bool atl_jni_register_method(JNIEnv *env, jmethodID method_id,
                             const char *method_name, const char *signature,
                             void *translated_func)
{
    if (!g_jni_cache.initialized) atl_jni_table_init();
    if (!env || !method_id || g_jni_cache.count >= JNI_CACHE_SIZE) return false;
    
    pthread_mutex_lock(&g_jni_cache.mutex);
    
    /* Check if already registered */
    for (size_t i = 0; i < g_jni_cache.count; i++) {
        if (g_jni_cache.entries[i].env == env && 
            g_jni_cache.entries[i].method_id == method_id) {
            pthread_mutex_unlock(&g_jni_cache.mutex);
            return true;
        }
    }
    
    size_t idx = g_jni_cache.count++;
    g_jni_cache.entries[idx].env = env;
    g_jni_cache.entries[idx].method_id = method_id;
    g_jni_cache.entries[idx].method_name = method_name ? strdup(method_name) : NULL;
    g_jni_cache.entries[idx].signature = signature ? strdup(signature) : NULL;
    g_jni_cache.entries[idx].translated_func = translated_func;
    g_jni_cache.entries[idx].is_static = false;
    
    /* Generate trampoline */
    void *thunk = NULL;
    if (translated_func) {
        thunk = atl_jni_create_trampoline(translated_func, signature);
    } else if (signature) {
        thunk = atl_jni_get_trampoline(signature);
    }
    g_jni_cache.entries[idx].thunk = thunk;
    
    pthread_mutex_unlock(&g_jni_cache.mutex);
    return true;
}

void *atl_jni_get_method_thunk(JNIEnv *env, jmethodID method_id,
                                const char *method_name, const char *signature)
{
    if (!g_jni_cache.initialized) atl_jni_table_init();
    if (!env) return NULL;
    
    pthread_mutex_lock(&g_jni_cache.mutex);
    for (size_t i = 0; i < g_jni_cache.count; i++) {
        if (g_jni_cache.entries[i].env == env && 
            g_jni_cache.entries[i].method_id == method_id) {
            void *thunk = g_jni_cache.entries[i].thunk;
            pthread_mutex_unlock(&g_jni_cache.mutex);
            return thunk;
        }
    }
    pthread_mutex_unlock(&g_jni_cache.mutex);
    
    /* If not cached, try to generate on-demand */
    if (signature) {
        return atl_jni_get_trampoline(signature);
    }
    
    return NULL;
}

/*
 * Handle callbacks from ARM code into JNI
 * CallStaticObjectMethod, CallObjectMethod, etc.
 */
void *atl_jni_callback_invoke(JNIEnv *env, jmethodID method_id,
                              void *args, bool is_static, bool is_varargs)
{
    if (!env || !method_id) return NULL;
    
    /* Get thread-local env */
    JNIEnv *thread_env = atl_jni_get_thread_env();
    if (!thread_env) {
        atl_jni_set_thread_env(env);
    }
    
    /* Find cached thunk */
    void *thunk = atl_jni_get_method_thunk(env, method_id, NULL, NULL);
    if (thunk) {
        /* Call through trampoline */
        typedef void* (*tramp_t)(void *, void *);
        tramp_t tramp = (tramp_t)thunk;
        return tramp(args, method_id);
    }
    
    /* Fallback - direct call handling would go here */
    (void)args;
    (void)is_static;
    (void)is_varargs;
    
    return NULL;
}

JNIEnv *atl_jni_get_thread_env(void)
{
    if (!g_jni_env_key_created) {
        pthread_key_create(&g_jni_env_key, NULL);
        g_jni_env_key_created = true;
    }
    return (JNIEnv *)pthread_getspecific(g_jni_env_key);
}

void atl_jni_set_thread_env(JNIEnv *env)
{
    if (!g_jni_env_key_created) {
        pthread_key_create(&g_jni_env_key, NULL);
        g_jni_env_key_created = true;
    }
    pthread_setspecific(g_jni_env_key, env);
}
