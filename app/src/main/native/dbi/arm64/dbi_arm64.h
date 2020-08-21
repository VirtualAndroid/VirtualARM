//
// Created by 甘尧 on 2020-02-29.
//
#pragma once

#include <base/marcos.h>
#include "dbi_global_stubs.h"
#include "block/code_cache.h"

namespace CodeCache {
    template<typename T>
    class FindTable;
}

namespace DBI::A64 {

    enum RuntimeMode : u8 {
        QuickMode,
        MemoryTrace,
        Mmu
    };

    struct ContextConfig {
        RuntimeMode mode;
        u8 context_reg;
        u8 forward_reg;
    };

    struct JitCacheBlock {
        VAddr cache_start;
        VAddr cache_len;

        void Destroy() const {

        }
    };

    class Instance : public BaseObject {
    public:
        Instance(RuntimeMode mode);

        const ContextConfig &GetContextConfig() const;

        const SharedPtr<CodeCache::FindTable<VAddr>> &GetCodeFindTable() const;

        const SharedPtr<DBI::A64::GlobalStubs> &GetGlobalStubs() const;

    private:
        ContextConfig context_config_;
        SharedPtr<CodeCache::FindTable<VAddr>> code_find_table_;
        SharedPtr<GlobalStubs> global_stubs_;
        SharedPtr<Jit::JitCache<JitCacheBlock, 12>> jit_cache_;
    };

    class Core : public BaseObject {
    public:
    private:

    };

}
