//
// Created by SwiftGan on 2020/8/23.
//

#include "svm_jit_manager.h"
#include "svm_thread.h"

using namespace Jit::A64;

JitManager::JitManager(const SharedPtr<Instance> &instance) : instance_(instance), queue_{0x1000} {
    jit_cache_ = SharedPtr<JitCacheA64>(new JitCacheA64(0x10000, 0x1000));
}

void JitManager::Initialize() {
    for (int i = 0; i < instance_->GetJitConfig().jit_thread_count; ++i) {
        jit_threads_.emplace_back(std::make_shared<JitThread>(SharedFrom(this)));
    }
}

const SharedPtr<Instance> &JitManager::GetInstance() const {
    return instance_;
}


void JitManager::CommitJit(JitCacheEntry *entry) {
    // alloc stub
    auto &code_block = entry->Data().code_block;
    if (!code_block) {
        code_block = instance_->PeekCacheBlock(entry->addr_start);
    }
    if (!entry->Data().id_in_block) {
        auto buffer = code_block->AllocCodeBuffer(entry->addr_start);
        entry->Data().id_in_block = buffer->id_;
    }
    queue_.push(entry);
}

void JitManager::JitNow(JitCacheEntry *entry) {
    if (entry && !entry->Data().ready) {
        SpinLockGuard guard(entry->Data().jit_lock);
        if (entry->Data().ready) {
            return;
        }
        JitUnsafe(entry);
    }
}

void JitManager::JitFromQueue() {
    JitCacheEntry *entry{};
    queue_.pop(entry);
    if (entry && !entry->Data().ready) {
        SpinLockGuard guard(entry->Data().jit_lock);
        if (entry->Data().ready) {
            return;
        }
        // do jit
        JitUnsafe(entry);
    }
}

void JitManager::JitUnsafe(JitCacheEntry *entry) {
    const auto &thread_context = ThreadContext::Current();
    const auto &jit_context = thread_context->GetJitContext();
    auto pc = entry->addr_start;
    // peek code block
    auto &code_block = entry->Data().code_block;
    if (!code_block) {
        code_block = instance_->PeekCacheBlock(entry->addr_start);
    }
    Buffer *buffer = nullptr;
    if (!entry->Data().id_in_block) {
        buffer = code_block->AllocCodeBuffer(entry->addr_start);
        entry->Data().id_in_block = buffer->id_;
    } else {
        buffer = code_block->GetBuffer(entry->Data().id_in_block);
    }
    jit_context->SetCacheEntry(entry);
    jit_context->BeginBlock(entry->addr_start);
    while (thread_context->JitInstr(pc)) {
        pc += 4;
        jit_context->Tick();
    }
    entry->addr_end = pc;
    // alloc
    auto cache_size = jit_context->BlockCacheSize();
    code_block->FlushCodeBuffer(buffer, cache_size);
    jit_context->EndBlock();
}

JitCacheEntry *JitManager::EmplaceJit(VAddr addr) {
    auto entry = jit_cache_->Emplace(addr);
    if (entry && !entry->Data().ready) {
        auto &jit_lock = entry->Data().jit_lock;
        if (jit_lock.LockedBySelf())
            return entry;
        SpinLockGuard guard(jit_lock);
        if (entry->Data().ready) {
            return entry;
        }
        if (ThreadContext::Current()->Type() == EnumThreadType &&
            JitNestGuard::CurrentNest() < 4) {
            // do jit now
            JitNestGuard nest_guard;
            JitUnsafe(entry);
        } else {
            CommitJit(entry);
        }
    }
    assert(entry);
    return entry;
}


static thread_local int jit_nested_{0};

JitNestGuard::JitNestGuard() {
    jit_nested_++;
}

JitNestGuard::~JitNestGuard() {
    jit_nested_--;
}

int JitNestGuard::CurrentNest() {
    return jit_nested_;
}

JitCacheBlock::JitCacheBlock(CodeBlock *codeBlock) : code_block(codeBlock) {}
