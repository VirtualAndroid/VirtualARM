//
// Created by SwiftGan on 2020/8/23.
//

#pragma once

#include <base/marcos.h>
#include "svm_jit_context.h"
#include "svm_arm64.h"

namespace Decode::A64 {
    class VixlJitDecodeVisitor;
}

namespace SVM::A64 {

    enum ThreadType {
        JitThreadType,
        EnumThreadType
    };

    class ThreadContext : public BaseObject, NonCopyable {
    public:
        ThreadContext(const SharedPtr<Instance> &instance);
    protected:
        SharedPtr<Instance> instance_;
        Jit::A64::ContextA64 jit_context_;
        std::unique_ptr<Decode::A64::VixlJitDecodeVisitor> jit_visitor_;
    };

    class EmuThreadContext : public ThreadContext {
    public:
        EmuThreadContext(const SharedPtr<Instance> &instance);
        void Run();
    private:
        CPUContext cpu_context;
        std::vector<u8> interrupt_stack_;
    };

    class JitThreadContext : public ThreadContext {
    public:
        JitThreadContext(const SharedPtr<Instance> &instance);

    private:

    };

    class JitThread : public BaseObject, NonCopyable {
    public:

    private:
        ThreadType type_;
    };

}