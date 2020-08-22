//
// Created by 甘尧 on 2020-02-29.
//
#pragma once

#include <base/marcos.h>
#include "dbi_global_stubs.h"
#include "block/code_cache.h"

namespace Jit {
    template<typename T>
    class FindTable;
}

namespace DBI::A64 {


    struct ContextConfig {
        u8 context_reg;
        u8 forward_reg;
    };

    struct MmuConfig {
        bool enable;
        u8 readable_bit;
        u8 writable_bit;
        u8 executable_bit;
    };

    class JitCacheBlock {
    public:

        JitCacheBlock();

        JitCacheBlock(VAddr stub_addr);
        ~JitCacheBlock();

        VAddr cache_start;
        VAddr cache_len;
        VAddr stub_addr;
        SpinMutex jit_lock;
    };

    class Instance : public BaseObject {
    public:

        constexpr static size_t page_bits = 12;
        using JitCacheA64 = Jit::JitCache<JitCacheBlock, page_bits>;

        explicit Instance();

        const ContextConfig &GetContextConfig() const;

        const MmuConfig &GetMmuConfig() const;

        const SharedPtr<Jit::FindTable<VAddr>> &GetCodeFindTable() const;

        const SharedPtr<GlobalStubs> &GetGlobalStubs() const;

        JitCacheA64::Entry *JitBlock(VAddr vaddr);
        JitCacheA64::Entry *FindJitCache(VAddr addr);
        JitCacheA64::Entry *FindAndJit(VAddr addr);
        JitCacheA64::Entry *CommitJitCache(const JitCacheA64::Entry &entry);

    private:
        ContextConfig context_config_;
        MmuConfig mmu_config_;
        SharedPtr<Jit::FindTable<VAddr>> code_find_table_;
        SharedPtr<GlobalStubs> global_stubs_;
        SharedPtr<JitCacheA64> jit_cache_;
    };

    class Core : public BaseObject {
    public:
    private:

    };

}
