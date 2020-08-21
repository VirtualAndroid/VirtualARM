//
// Created by 甘尧 on 2020-02-29.
//

#include "dbi_arm64.h"
#include "block/code_find_table.h"

using namespace DBI::A64;
using namespace CodeCache;

Instance::Instance(RuntimeMode mode) {
    context_config_ = {
            .mode = mode,
            .context_reg = 30, // lr
            .forward_reg = 16
    };
    code_find_table_ = SharedPtr<FindTable<VAddr>>(new FindTable<VAddr>(48, 2));
    global_stubs_ = SharedPtr<GlobalStubs>(new GlobalStubs(this));
    jit_cache_ = SharedPtr<Jit::JitCache<JitCacheBlock, 12>>(new Jit::JitCache<JitCacheBlock, 12>(0x10000, 0x100));
    jit_cache_->Put({1, 2, {1, 2}});
    jit_cache_->Invalid(1, 2);
}

const ContextConfig &Instance::GetContextConfig() const {
    return context_config_;
}

const SharedPtr<CodeCache::FindTable<VAddr>> &Instance::GetCodeFindTable() const {
    return code_find_table_;
}

const SharedPtr<DBI::A64::GlobalStubs> &Instance::GetGlobalStubs() const {
    return global_stubs_;
}
