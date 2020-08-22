//
// Created by 甘尧 on 2020-02-29.
//

#include "dbi_arm64.h"
#include "block/code_find_table.h"

using namespace DBI::A64;
using namespace Jit;

Instance::Instance() {
    context_config_ = {
            .context_reg = 30, // lr
            .forward_reg = 16
    };
    mmu_config_ = {
            .enable = false,
            .readable_bit = 0,
            .writable_bit = 1,
            .executable_bit = 2
    };
    code_find_table_ = SharedPtr<FindTable<VAddr>>(new FindTable<VAddr>(48, 2));
    global_stubs_ = SharedPtr<GlobalStubs>(new GlobalStubs(this));
    jit_cache_ = SharedPtr<JitCacheA64>(new JitCacheA64(0x10000, 0x1000));
}

const ContextConfig &Instance::GetContextConfig() const {
    return context_config_;
}

const SharedPtr<Jit::FindTable<VAddr>> &Instance::GetCodeFindTable() const {
    return code_find_table_;
}

const SharedPtr<GlobalStubs> &Instance::GetGlobalStubs() const {
    return global_stubs_;
}

Instance::JitCacheA64::Entry *Instance::FindJitCache(VAddr addr) {
    return nullptr;
}

Instance::JitCacheA64::Entry *Instance::FindAndJit(VAddr addr) {
    auto cache = jit_cache_->Emplace(addr);
    SpinLockGuard guard(cache->Data().jit_lock);
    if (!cache->Data().cache_start) {
        // do jit
    }
    return cache;
}

Instance::JitCacheA64::Entry *Instance::CommitJitCache(const JitCacheA64::Entry &entry) {
    return nullptr;
}

Instance::JitCacheA64::Entry *Instance::JitBlock(VAddr addr) {
    return nullptr;
}

const MmuConfig &Instance::GetMmuConfig() const {
    return mmu_config_;
}


JitCacheBlock::JitCacheBlock() {}

JitCacheBlock::JitCacheBlock(VAddr stub_addr) : stub_addr(stub_addr) {

}

JitCacheBlock::~JitCacheBlock() {
    // Destroy
    SpinLockGuard guard(jit_lock);

}
