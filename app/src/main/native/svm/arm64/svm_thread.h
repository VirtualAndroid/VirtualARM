//
// Created by SwiftGan on 2020/8/23.
//

#pragma once

#include <base/marcos.h>
#include "svm_jit_context.h"
#include "svm_arm64.h"
#include <thread>
#include <stack>

namespace Decode::A64 {
    class VixlJitDecodeVisitor;
}

namespace Jit::A64 {
    class JitManager;
}

namespace SVM::A64 {

    constexpr size_t default_jit_run_ticks = 0x1000;

    enum ThreadType {
        JitThreadType,
        EmuThreadType
    };

    class ThreadContext : public BaseObject, NonCopyable {
    public:
        ThreadContext(const SharedPtr<Instance> &instance);

        const static SharedPtr<ThreadContext> &Current();

        const SharedPtr<Instance> &GetInstance() const;

        void RegisterCurrent();

        virtual ThreadType Type() {
            abort();
        };

        const ContextA64 GetJitContext() const;

        void PushJitContext(ContextA64 context);

        void PopJitContext();

        // if false : end of block
        bool JitInstr(VAddr addr);

    protected:
        SharedPtr<Instance> instance_;
        std::shared_ptr<Decode::A64::VixlJitDecodeVisitor> jit_visitor_;
        std::shared_ptr<vixl::aarch64::Decoder> jit_decode_;
        std::stack<Jit::A64::ContextA64> jit_contexts_;
    };

    class EmuThreadContext : public ThreadContext {
    public:
        EmuThreadContext(const SharedPtr<Instance> &instance);

        void Run(size_t ticks = default_jit_run_ticks);

        void LookupJitCache();

        CPUContext *GetCpuContext();

        ThreadType Type() override;

        // User Callbacks
        virtual void Interrupt(InterruptHelp &interrupt) {
            cpu_context_.pc += 4;
        };

        virtual void Fallback() {};

        virtual u64 GetClockTicks() { return 0; };

    protected:
        CPUContext cpu_context_;
        std::vector<u8> interrupt_stack_;
    };

    class JitThreadContext : public ThreadContext {
    public:
        JitThreadContext(const SharedPtr<Instance> &instance);

        ThreadType Type() override;
    };

    class JitThread : public BaseObject {
    public:
        explicit JitThread(const SharedPtr<JitManager> &manager);

        virtual ~JitThread();

    private:
        ThreadType type_;
        std::unique_ptr<std::thread> thread_;
        SharedPtr<JitManager> jit_manager_;
        SharedPtr<JitThreadContext> context_;
        bool destroyed{false};
    };

}