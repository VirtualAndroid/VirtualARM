//
// Created by SwiftGan on 2020/8/23.
//

#include <base/log.h>
#include "svm_jit_manager.h"
#include "svm_thread.h"

using namespace Jit::A64;

JitManager::JitManager(const SharedPtr<Instance> &instance) : instance_(instance), queue_{0x1000} {
    jit_cache_ = SharedPtr<JitCacheA64>(new JitCacheA64(0x10000, 0x1000));
}

void JitManager::Initialize() {
    cache_find_table_ = instance_->GetCodeFindTable();
    for (int i = 0; i < instance_->GetJitConfig().jit_thread_count; ++i) {
        jit_threads_.emplace_back(std::make_shared<JitThread>(SharedFrom(this)));
    }
}

const SharedPtr<Instance> &JitManager::GetInstance() const {
    return instance_;
}


void JitManager::CommitJit(JitCacheEntry *entry) {
    EmplaceCacheAllocation(entry);
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
    LOGE("JitUnsafe: %llu", entry->addr_start);
    const auto &thread_context = ThreadContext::Current();
    auto pc = entry->addr_start;
    // peek code block
    auto &code_block = entry->Data().code_block;

    EmplaceCacheAllocation(entry);
    auto buffer = code_block->GetBuffer(entry->Data().id_in_block);
    JitContext jit_context(*instance_);
    jit_context.SetCacheEntry(entry);
    jit_context.BeginBlock(entry->addr_start);
    thread_context->PushJitContext(&jit_context);

    while (thread_context->JitInstr(pc)) {
        pc += 4;
        jit_context.Tick();
    }

    thread_context->PopJitContext();
    entry->addr_end = pc + 4;
    auto cache_size = jit_context.BlockCacheSize();
    code_block->FlushCodeBuffer(buffer, cache_size);
    jit_context.EndBlock();
    entry->Data().ready = true;
    jit_cache_->Flush(entry);
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
        // do jit now
        JitNestGuard nest_guard;
        bool is_emu_thread = ThreadContext::Current()->Type() == EmuThreadType;
        if (!is_emu_thread || JitNestGuard::CurrentNest() > 8) {
            CommitJit(entry);
        } else {
            JitUnsafe(entry);
        }
    }
    return entry;
}

void JitManager::EmplaceCacheAllocation(JitCacheEntry *entry) {
    auto &code_block = entry->Data().code_block;
    if (!code_block) {
        code_block = instance_->PeekCacheBlock(entry->addr_start);
    }
    if (!entry->Data().id_in_block) {
        auto buffer = code_block->AllocCodeBuffer(entry->addr_start);
        entry->Data().id_in_block = buffer->id_;
        cache_find_table_->FillCodeAddress(entry->addr_start, code_block->GetDispatcherAddr(buffer));
    }
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
