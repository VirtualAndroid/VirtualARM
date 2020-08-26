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
    __ SetStackPointer(sp);
    __ Bind(&true_label);
    __ Stp(x29, x30, MemOperand(sp, -16, PreIndex));
    __ Svc(0);
    __ Ldp(x29, x30, MemOperand(sp, 16, PostIndex));
    __ Sub(x0, x0, 1);
    __ Cbnz(x0, &true_label);
    __ Bind(&loop);
    __ Nop();
    __ Nop();
    __ Nop();
    __ Bl(&loop);
    __ Ret();

    __ FinalizeCode();
    return __ GetBuffer()->GetStartAddress<void*>();
}

bool debug = true;

class MyEmuThread : public EmuThreadContext {
public:
    MyEmuThread(const SharedPtr<Instance> &instance) : EmuThreadContext(instance) {

    }

    std::atomic_int i = 0;

    void Interrupt(InterruptHelp &interrupt) override {
        LOGE("Interrupt: %d", i++);
        if (cpu_context_.cpu_registers[0].X == 1) {
            LOGE("Interrupt");
        }
        cpu_context_.pc += 4;
    }
};

extern "C"
JNIEXPORT void JNICALL
load_test(JNIEnv *env, jobject instance) {
    auto svm = SharedPtr<Instance>(new Instance());
    svm->Initialize();
    auto context = SharedPtr<MyEmuThread>(new MyEmuThread(svm));
    context->RegisterCurrent();
    context->GetCpuContext()->cpu_registers[0].X = 50;
    context->GetCpuContext()->pc = reinterpret_cast<u64>(TestCase1());
    context->GetCpuContext()->sp = reinterpret_cast<u64>(malloc(256 * 1024)) + 256 * 1024;
    assert(context->GetCpuContext()->ticks_now == 0);
    assert(context->GetCpuContext()->ticks_max == 0);
//    while (debug) {
//        sleep(1);
//    }
    context->Run(500);
    LOGE("RunTicks %llu", context->GetCpuContext()->ticks_now);
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

