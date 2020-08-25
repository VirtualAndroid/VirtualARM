//
// Created by 甘尧 on 2019-09-20.
//

#include <jni.h>
#include <dlfcn.h>
#include <base/log.h>
#include "virtual_arm.h"

#include "svm/arm64/svm_arm64.h"
#include "svm/arm64/svm_thread.h"

#define __ masm_.
static MacroAssembler masm_;

void *TestCase1() {
    Label true_label;
    Label loop;
    __ Reset();
    __ Bind(&loop);
    __ Push(x0, x1, x2, x3);
    __ Sub(x1, x2, x3);
    __ Cbz(x1, &true_label);
    // do some
    __ Add(x1, x1, 8);
    __ Add(x1, x1, 8);
    __ Add(x1, x1, 8);
    __ Add(x1, x1, 8);
    __ Bind(&true_label);
    __ Pop(x3, x2, x1, x0);
    __ Bl(&loop);

    __ FinalizeCode();
    return __ GetBuffer()->GetStartAddress<void*>();
}


extern "C"
JNIEXPORT void JNICALL
load_test(JNIEnv *env, jobject instance) {
    auto svm = SharedPtr<Instance>(new Instance());
    svm->Initialize();
    auto context = SharedPtr<EmuThreadContext>(new EmuThreadContext(svm));
    context->RegisterCurrent();
    context->GetCpuContext()->pc = reinterpret_cast<u64>(TestCase1());
    context->GetCpuContext()->sp = reinterpret_cast<u64>(malloc(256 * 1024));
    context->Run(100);
}

static bool registerNativeMethods(JNIEnv *env, const char *className, JNINativeMethod *jniMethods, int methods) {
    jclass clazz = env->FindClass(className);
    if (clazz == nullptr) {
        return false;
    }
    return env->RegisterNatives(clazz, jniMethods, methods) >= 0;
}

static JNINativeMethod jniMethods[] = {
        {
                "launch",
                "()V",
                (void *) load_test
        }
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {

    const char* CLASS_VIRTUAL_ARM = "com/swift/virtualarm/VirtualARM";

    int jniMethodSize = sizeof(JNINativeMethod);

    JNIEnv *env = nullptr;

    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return -1;
    }

    if (!registerNativeMethods(env, CLASS_VIRTUAL_ARM, jniMethods, sizeof(jniMethods) / jniMethodSize)) {
        return -1;
    }

    return JNI_VERSION_1_6;
}

