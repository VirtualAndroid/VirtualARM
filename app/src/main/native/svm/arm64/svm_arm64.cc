//
// Created by 甘尧 on 2020-02-29.
//

#include "svm_arm64.h"
#include "block/code_find_table.h"

using namespace SVM::A64;
using namespace Jit;

Instance::Instance() {
    context_config_ = {
            .jit_thread_count = 2,
            .context_reg = 30, // lr
            .forward_reg = 16,
            .protect_code = true
    };
    mmu_config_ = {
            .enable = false,
            .addr_width = 48,
            .page_bits = 12,
            .readable_bit = 0,
            .writable_bit = 1,
            .executable_bit = 2
    };
    code_find_table_ = SharedPtr<FindTable<VAddr>>(new FindTable<VAddr>(48, 2));
    global_stubs_ = SharedPtr<GlobalStubs>(new GlobalStubs(SharedFrom(this)));
    jit_manager_ = SharedPtr<JitManager>(new JitManager(SharedFrom(this)));
    mmu_ = SharedPtr<A64MMU>(new A64MMU(mmu_config_.addr_width, mmu_config_.page_bits));
}

const JitConfig &Instance::GetJitConfig() const {
    return context_config_;
}

const SharedPtr<Jit::FindTable<VAddr>> &Instance::GetCodeFindTable() const {
    return code_find_table_;
}

const SharedPtr<GlobalStubs> &Instance::GetGlobalStubs() const {
    return global_stubs_;
}

JitCacheEntry *Instance::FindAndJit(VAddr addr) {
    return jit_manager_->EmplaceJit(addr);
}

const MmuConfig &Instance::GetMmuConfig() const {
    return mmu_config_;
}

const SharedPtr<SVM::A64::A64MMU> &Instance::GetMmu() const {
    return mmu_;
}

const SharedPtr<JitManager> &Instance::GetJitManager() const {
    return jit_manager_;
}
