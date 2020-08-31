//
// Created by 甘尧 on 2019-09-20.
//

#include <jni.h>
#include <dlfcn.h>
#include <base/log.h>
#include <platform/memory.h>
#include "virtual_arm.h"

#include "svm/arm64/svm_arm64.h"
#include "svm/arm64/svm_thread.h"
#include "loader/nro.h"

#define __ masm_.
static MacroAssembler masm_;

void PlatformSignalHandler(int signum, siginfo_t *siginfo, ucontext_t *uc) {
    if (signum != SIGILL) {
        abort();
    }
    sigcontext *context = &uc->uc_mcontext;
    u32 code = *reinterpret_cast<u32 *>(context->pc);
    for (int i = -8; i < 9; ++i) {
        std::ostringstream string;
        PrintDisassembler disassembler(string);
        disassembler.Disassemble(reinterpret_cast<const Instruction *>(context->pc + (i * 4)));
        LOGE((i == 0) ?  "Error Instr: %s" : "Instr: %s" , disassembler.GetOutput());
    }
    abort();
}

void *TestCase1() {
    Label true_label;
    Label loop;
    __ Reset();
    __ SetStackPointer(sp);
    __ Bind(&true_label);
    __ Str(x30, MemOperand(sp, -16, PostIndex));
    __ Stp(x30, x30, MemOperand(sp, -16, PreIndex));
    __ Svc(0);
    __ Brk(0);
    __ Svc(0);
    __ Svc(0);
    __ Ldr(x30, MemOperand(sp, 16, PreIndex));
    __ Ldp(x30, x2, MemOperand(sp, 16, PostIndex));
    __ Sub(x0, x0, 1);
    __ Cbnz(x0, &true_label);
    __ Bind(&loop);
    __ Nop();
    __ Nop();
    __ Nop();
    __ Blr(x8);
    __ Ret();

    __ FinalizeCode();
    return __ GetBuffer()->GetStartAddress<void*>();
}

void Test1() {
    LOGE("Caonima");
}

bool debug = true;

class MyEmuThread : public EmuThreadContext {
public:
    MyEmuThread(const SharedPtr<Instance> &instance) : EmuThreadContext(instance) {}

    void Interrupt(InterruptHelp &interrupt) override {
        if (interrupt.reason == CPU::A64::InterruptHelp::Svc) {
            LOGE("Svc: %d", interrupt.exception);
        }
        cpu_context_.pc += 4;
    }

    void Fallback() override {
        LOGE("Fallback");
        abort();
    }
};

void RunDemo() {
    auto svm = SharedPtr<Instance>(new Instance());
    svm->Initialize();
    auto context = SharedPtr<MyEmuThread>(new MyEmuThread(svm));
    context->RegisterCurrent();
    context->GetCpuContext()->cpu_registers[0].X = 50;
    u64 target = 1573291731371;
    context->GetCpuContext()->cpu_registers[8].X = target;
    context->GetCpuContext()->pc = reinterpret_cast<u64>(TestCase1());
    context->GetCpuContext()->sp = reinterpret_cast<u64>(malloc(256 * 1024)) + 256 * 1024;
    assert(context->GetCpuContext()->ticks_now == 0);
    assert(context->GetCpuContext()->ticks_max == 0);
    svm->GetCodeFindTable()->FillCodeAddress(target, 0x100);
    context->Run(500);
    LOGE("RunTicks %llu", context->GetCpuContext()->ticks_now);
}

void RunTestNro() {
    struct sigaction sig{};
    sigemptyset(&sig.sa_mask);
    sig.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sig.sa_sigaction = reinterpret_cast<void (*)(int, siginfo_t *, void *)>(PlatformSignalHandler);
    if (sigaction(SIGILL, &sig, nullptr) == -1) {
    }
    Loader::Nro nro("/sdcard/barrier.nro");
    auto code_set = std::make_shared<Jit::CodeSet>();
    code_set->base_addr = 0x120000000;
    Platform::MapExecutableMemory(nro.GetLoadSegmentsSize(), code_set->base_addr);
    nro.Load(code_set.get());
    auto svm = SharedPtr<Instance>(new Instance());
    svm->Initialize();
    svm->RegisterCodeSet(code_set);
    auto context = SharedPtr<MyEmuThread>(new MyEmuThread(svm));
    context->RegisterCurrent();
    context->GetCpuContext()->pc = reinterpret_cast<u64>(code_set->entrypoint);
    context->GetCpuContext()->tpidrro = reinterpret_cast<u64>(malloc(512));
    context->GetCpuContext()->sp = reinterpret_cast<u64>(malloc(256 * 1024)) + 256 * 1024;
    context->GetCpuContext()->cpu_registers[0].X = 0;
    context->GetCpuContext()->cpu_registers[1].X = 1;
    context->Run(0x4000000);
}

extern "C"
JNIEXPORT void JNICALL
load_test(JNIEnv *env, jobject instance) {
    RunTestNro();
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

