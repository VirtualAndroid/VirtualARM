//
// Created by SwiftGan on 2020/8/23.
//

#include "svm_thread.h"
#include "decode/decode_vixl.h"
#include "svm_jit_manager.h"

using namespace Jit::A64;
using namespace Decode::A64;
using namespace SVM::A64;

static thread_local SharedPtr<ThreadContext> current_context_;

ThreadContext::ThreadContext(const SharedPtr<Instance> &instance) : instance_(instance) {
    if (instance->GetMmuConfig().enable) {
        jit_context_ = SharedPtr<JitContext>(new ContextWithMmu(instance));
    } else {
        jit_context_ = SharedPtr<JitContext>(new QuickContext(instance));
    }
    jit_visitor_ = std::make_unique<VixlJitDecodeVisitor>(jit_context_);
    jit_decode_ = std::make_unique<vixl::aarch64::Decoder>();
    jit_decode_->AppendVisitor(jit_visitor_.get());
}

const SharedPtr<ThreadContext> &ThreadContext::Current() {
    return current_context_;
}

void ThreadContext::RegisterCurrent() {
    assert(!current_context_);
    current_context_ = SharedFrom(this);
}

const ContextA64 &ThreadContext::GetJitContext() const {
    return jit_context_;
}

bool ThreadContext::JitInstr(VAddr addr) {
    jit_context_->SetPC(addr);
    jit_decode_->Decode(reinterpret_cast<Instruction*>(addr));
    return jit_context_->Termed();
}

EmuThreadContext::EmuThreadContext(const SharedPtr<Instance> &instance) : ThreadContext(instance) {
    // 512KB
    interrupt_stack_.resize(512 * 1024);
    cpu_context.interrupt_sp = reinterpret_cast<VAddr>(interrupt_stack_.data());
}

void EmuThreadContext::Run() {
    instance_->GetGlobalStubs()->RunCode(&cpu_context);
}

ThreadType EmuThreadContext::Type() {
    return EnumThreadType;
}

JitThreadContext::JitThreadContext(const SharedPtr<Instance> &instance) : ThreadContext(instance) {}

ThreadType JitThreadContext::Type() {
    return JitThreadType;
}

JitThread::JitThread(const SharedPtr<JitManager> &manager) : jit_manager_(manager) {
    context_ = SharedPtr<JitThreadContext>(new JitThreadContext(manager->GetInstance()));
    thread_.reset(new std::thread([this]() -> void {
        context_->RegisterCurrent();
        while (destroyed) {
            jit_manager_->JitFromQueue();
        }
    }));
}

JitThread::~JitThread() {
    destroyed = true;
    if (thread_->joinable()) {
        thread_->join();
    }
}
