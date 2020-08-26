//
// Created by SwiftGan on 2020/8/23.
//

#include "svm_thread.h"
#include "decode/decode_vixl.h"
#include "svm_jit_manager.h"

using namespace Jit::A64;
using namespace Decode::A64;
using namespace SVM::A64;

static thread_local SharedPtr<ThreadContext> current_context_{};

ThreadContext::ThreadContext(const SharedPtr<Instance> &instance) : instance_(instance) {
    jit_visitor_ = std::make_shared<VixlJitDecodeVisitor>();
    jit_decode_ = std::make_shared<vixl::aarch64::Decoder>();
    jit_decode_->AppendVisitor(jit_visitor_.get());
}

const SharedPtr<ThreadContext> &ThreadContext::Current() {
    return current_context_;
}

void ThreadContext::RegisterCurrent() {
    assert(!current_context_);
    current_context_ = SharedFrom(this);
}

const ContextA64 ThreadContext::GetJitContext() const {
    return jit_contexts_.top();
}

bool ThreadContext::JitInstr(VAddr addr) {
    jit_contexts_.top()->SetPC(addr);
    jit_decode_->Decode(reinterpret_cast<Instruction*>(addr));
    return !jit_contexts_.top()->Termed();
}


void ThreadContext::PushJitContext(ContextA64 context) {
    jit_contexts_.push(context);
    jit_visitor_->PushContext(context);
}

void ThreadContext::PopJitContext() {
    jit_contexts_.pop();
    jit_visitor_->PopContext();
}

const SharedPtr<Instance> &ThreadContext::GetInstance() const {
    return instance_;
}

EmuThreadContext::EmuThreadContext(const SharedPtr<Instance> &instance) : ThreadContext(instance) {
    // 512KB
    interrupt_stack_.resize(512 * 1024);
    cpu_context_.context_ptr = reinterpret_cast<VAddr>(this);
    cpu_context_.dispatcher_table = instance->GetCodeFindTable()->TableEntryPtr();
    auto mmu_ = instance->GetMmu();
    if (mmu_) {
        cpu_context_.page_table = mmu_->TopPageTable();
        cpu_context_.tlb = mmu_->Tbl()->TLBTablePtr();
    }
    cpu_context_.dispatcher_table = instance->GetCodeFindTable()->TableEntryPtr();
    cpu_context_.interrupt_sp = reinterpret_cast<VAddr>(interrupt_stack_.data());
}

void EmuThreadContext::Run(size_t ticks) {
    cpu_context_.ticks_max += ticks;
    LookupJitCache();
    instance_->GetGlobalStubs()->RunCode(&cpu_context_);
}

ThreadType EmuThreadContext::Type() {
    return EnumThreadType;
}

CPUContext *EmuThreadContext::GetCpuContext() {
    return &cpu_context_;
}

void EmuThreadContext::LookupJitCache() {
    auto jit_cache = instance_->FindAndJit(cpu_context_.pc);
    if (jit_cache && jit_cache->Data().GetStub()) {
        cpu_context_.code_cache = jit_cache->Data().GetStub();
    }
}

JitThreadContext::JitThreadContext(const SharedPtr<Instance> &instance) : ThreadContext(instance) {}

ThreadType JitThreadContext::Type() {
    return JitThreadType;
}

JitThread::JitThread(const SharedPtr<JitManager> &manager) : jit_manager_(manager) {
    context_ = SharedPtr<JitThreadContext>(new JitThreadContext(manager->GetInstance()));
    thread_.reset(new std::thread([this]() -> void {
        context_->RegisterCurrent();
        while (!destroyed) {
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
