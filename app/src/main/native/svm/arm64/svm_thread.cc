//
// Created by SwiftGan on 2020/8/23.
//

#include "svm_thread.h"
#include "decode/decode_vixl.h"

using namespace Jit::A64;
using namespace Decode::A64;

ThreadContext::ThreadContext(const SharedPtr<Instance> &instance) : instance_(instance) {
    if (instance->GetMmuConfig().enable) {
        jit_context_ = SharedPtr<JitContext>(new ContextWithMmu(instance));
    } else {
        jit_context_ = SharedPtr<JitContext>(new QuickContext(instance));
    }
    jit_visitor_ = std::make_unique<VixlJitDecodeVisitor>(jit_context_);
}

EmuThreadContext::EmuThreadContext(const SharedPtr<Instance> &instance) : ThreadContext(instance) {
    // 512KB
    interrupt_stack_.resize(512 * 1024);
    cpu_context.interrupt_sp = reinterpret_cast<VAddr>(interrupt_stack_.data());
}

void EmuThreadContext::Run() {
    instance_->GetGlobalStubs()->RunCode(&cpu_context);
}

JitThreadContext::JitThreadContext(const SharedPtr<Instance> &instance) : ThreadContext(instance) {

}
