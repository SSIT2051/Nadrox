#ifndef ATL_JNI_MIR_H
#define ATL_JNI_MIR_H

#include "../mir/mir.h"
#include "../mir/lowering/lowering.h"

#ifdef __cplusplus
extern "C" {
#endif

/* JNI function table indices */
#define JNI_GetVersion 4
#define JNI_DefineClass 5
#define JNI_FindClass 6
#define JNI_FromReflectedMethod 7
#define JNI_FromReflectedField 8
#define JNI_ToReflectedMethod 9
#define JNI_GetSuperclass 10
#define JNI_IsAssignableFrom 11
#define JNI_ToReflectedField 12
#define JNI_Throw 13
#define JNI_ThrowNew 14
#define JNI_ExceptionOccurred 15
#define JNI_ExceptionDescribe 16
#define JNI_ExceptionClear 17
#define JNI_FatalError 18
#define JNI_PushLocalFrame 19
#define JNI_PopLocalFrame 20
#define JNI_NewGlobalRef 21
#define JNI_DeleteGlobalRef 22
#define JNI_DeleteLocalRef 23
#define JNI_IsSameObject 24
#define JNI_NewLocalRef 25
#define JNI_EnsureLocalCapacity 26
#define JNI_AllocObject 27
#define JNI_NewObject 28
#define JNI_NewObjectV 29
#define JNI_NewObjectA 30
#define JNI_GetObjectClass 31
#define JNI_IsInstanceOf 32
#define JNI_GetMethodID 33
#define JNI_CallObjectMethod 34
#define JNI_CallObjectMethodV 35
#define JNI_CallObjectMethodA 36
#define JNI_CallBooleanMethod 37
#define JNI_CallBooleanMethodV 38
#define JNI_CallBooleanMethodA 39
#define JNI_CallByteMethod 40
#define JNI_CallByteMethodV 41
#define JNI_CallByteMethodA 42
#define JNI_CallCharMethod 43
#define JNI_CallCharMethodV 44
#define JNI_CallCharMethodA 45
#define JNI_CallShortMethod 46
#define JNI_CallShortMethodV 47
#define JNI_CallShortMethodA 48
#define JNI_CallIntMethod 49
#define JNI_CallIntMethodV 50
#define JNI_CallIntMethodA 51
#define JNI_CallLongMethod 52
#define JNI_CallLongMethodV 53
#define JNI_CallLongMethodA 54
#define JNI_CallFloatMethod 55
#define JNI_CallFloatMethodV 56
#define JNI_CallFloatMethodA 57
#define JNI_CallDoubleMethod 58
#define JNI_CallDoubleMethodV 59
#define JNI_CallDoubleMethodA 60
#define JNI_CallVoidMethod 61
#define JNI_CallVoidMethodV 62
#define JNI_CallVoidMethodA 63
#define JNI_GetFieldID 94
#define JNI_GetObjectField 95
#define JNI_GetBooleanField 96
#define JNI_GetByteField 97
#define JNI_GetCharField 98
#define JNI_GetShortField 99
#define JNI_GetIntField 100
#define JNI_GetLongField 101
#define JNI_GetFloatField 102
#define JNI_GetDoubleField 103
#define JNI_SetObjectField 104
#define JNI_SetBooleanField 105
#define JNI_SetByteField 106
#define JNI_SetCharField 107
#define JNI_SetShortField 108
#define JNI_SetIntField 109
#define JNI_SetLongField 110
#define JNI_SetFloatField 111
#define JNI_SetDoubleField 112
#define JNI_GetStaticMethodID 113
#define JNI_CallStaticObjectMethod 114
#define JNI_CallStaticObjectMethodV 115
#define JNI_CallStaticObjectMethodA 116
#define JNI_CallStaticBooleanMethod 117
#define JNI_CallStaticBooleanMethodV 118
#define JNI_CallStaticBooleanMethodA 119
#define JNI_CallStaticByteMethod 120
#define JNI_CallStaticByteMethodV 121
#define JNI_CallStaticByteMethodA 122
#define JNI_CallStaticCharMethod 123
#define JNI_CallStaticCharMethodV 124
#define JNI_CallStaticCharMethodA 125
#define JNI_CallStaticShortMethod 126
#define JNI_CallStaticShortMethodV 127
#define JNI_CallStaticShortMethodA 128
#define JNI_CallStaticIntMethod 129
#define JNI_CallStaticIntMethodV 130
#define JNI_CallStaticIntMethodA 131
#define JNI_CallStaticLongMethod 132
#define JNI_CallStaticLongMethodV 133
#define JNI_CallStaticLongMethodA 134
#define JNI_CallStaticFloatMethod 135
#define JNI_CallStaticFloatMethodV 136
#define JNI_CallStaticFloatMethodA 137
#define JNI_CallStaticDoubleMethod 138
#define JNI_CallStaticDoubleMethodV 139
#define JNI_CallStaticDoubleMethodA 140
#define JNI_CallStaticVoidMethod 141
#define JNI_CallStaticVoidMethodV 142
#define JNI_CallStaticVoidMethodA 143

/* Initialize JNI MIR lowering */
bool atl_jni_mir_init(void);

/* Lower JNI_CALL MIR instruction to appropriate code */
bool atl_jni_mir_lower_call(atl_lower_context_t *ctx, atl_mir_inst_t *inst);

/* Access JNIEnv function table */
atl_mir_value_t *atl_jni_mir_get_env_func(atl_mir_builder_t *b, 
                                           atl_mir_value_t *env, 
                                           uint32_t func_index);

/* Java object handle management */
atl_mir_value_t *atl_jni_mir_get_object_handle(atl_mir_builder_t *b, 
                                                atl_mir_value_t *jobject);
void atl_jni_mir_release_object_handle(atl_mir_builder_t *b, 
                                       atl_mir_value_t *jobject);

#ifdef __cplusplus
}
#endif

#endif /* ATL_JNI_MIR_H */
