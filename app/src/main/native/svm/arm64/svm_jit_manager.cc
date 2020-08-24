//
// Created by SwiftGan on 2020/8/23.
//

#include "svm_jit_manager.h"
#include "svm_thread.h"

using namespace Jit::A64;

JitManager::JitManager(const SharedPtr<Instance> &instance) : instance_(instance), queue_{0x1000} {
    jit_cache_ = SharedPtr<JitCacheA64>(new JitCacheA64(0x10000, 0x1000));
    for (int i = 0; i < instance->GetJitConfig().jit_thread_count; ++i) {
        jit_threads_.emplace_back(std::make_shared<JitThread>(SharedFrom(this)));
    }
}

const SharedPtr<Instance> &JitManager::GetInstance() const {
    return instance_;
}


void JitManager::CommitJit(JitCacheEntry *entry) {
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
        JitNow(entry);
    }
}

void JitManager::JitUnsafe(JitCacheEntry *entry) {
    const auto &thread_context = ThreadContext::Current();
    const auto &jit_context = thread_context->GetJitContext();
    auto pc = entry->addr_start;
    jit_context->BeginBlock(entry->addr_start);
    jit_context->SetCacheEntry(entry);
    while (thread_context->JitInstr(pc)) {
        pc += 4;
        jit_context->Tick();
    }
    entry->addr_end = pc;
    // alloc
    auto cache_size = jit_context->BlockCacheSize();
    // peek code block
    if (!entry->Data().code_block) {
        //TODO
    }
    Buffer *buffer = nullptr;
    if (entry->Data().code_block) {
        buffer = entry->Data().code_block->AllocCodeBuffer(entry->addr_start);
    }
    entry->Data().code_block->FlushCodeBuffer(buffer, cache_size);
    jit_context->EndBlock();
}

JitCacheEntry *JitManager::EmplaceJit(VAddr addr) {
    auto entry = jit_cache_->Emplace(addr);
    if (entry && !entry->Data().ready) {
        SpinLockGuard guard(entry->Data().jit_lock);
        if (entry->Data().ready) {
            return entry;
        }
        if (JitNestGuard::CurrentNest() < 32) {
            // do jit now
            JitNestGuard nest_guard;
            JitNow(entry);
        } else {
            // alloc stub and return

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
