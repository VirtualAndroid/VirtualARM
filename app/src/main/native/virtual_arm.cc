//
// Created by 甘尧 on 2019-09-20.
//

#include <jni.h>
#include <dlfcn.h>
#include <base/log.h>
#include "virtual_arm.h"
#include "asm/arm64/instruction_decode.h"
#include "asm/arm64/cpu_arm64.h"
#include "frontend/ir/instruction_ir.h"
#include "svm/arm64/svm_jit_arm64.h"
#include "block/code_cache.h"


using namespace Instructions::A64;
using namespace CPU::A64;
using namespace Instructions::IR;
using namespace Jit;

struct Dt {
    int a;
};

class Test : NonCopyable{
public:

    Test(Dt d, u64 a, int i) : i(i) {
        LOGE("Test(dai)");
    }

    Test(int i) : i(i) {
        LOGE("Test(i)");
    }

    Test() {
        LOGE("Test()");
    }

    ~Test() {
        LOGE("~Test()");
    }

public:
    int i;
};


SharedPtr<JitCache<Test, 12>> cache{new JitCache<Test, 12>(0x100, 0x10)};

extern "C"
JNIEXPORT void JNICALL
load_test(JNIEnv *env, jobject instance) {
//    FastBranchDecoder decoder;
////    InstrA64Ref instr = decoder.Decode(reinterpret_cast<InstrA64 *>((VAddr)rename + 24));
//    assert(sizeof(Argument) == sizeof(Imm128));
//    CPUContext context;
//    context.cpu_registers[30].X = 0x111;
//    assert(context.lr.X == 0x111);
    auto res = cache->Emplace(0x10000, Dt(), u64(1), (int)123);
    LOGE("Test: %d" ,res->Data().i);
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

