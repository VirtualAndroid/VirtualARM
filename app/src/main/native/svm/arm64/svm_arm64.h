//
// Created by 甘尧 on 2020-02-29.
//
#pragma once

#include <base/marcos.h>
#include "svm_global_stubs.h"
#include "block/code_cache.h"
#include "svm_mmu.h"

namespace Jit {
    template<typename T>
    class FindTable;
}

namespace SVM::A64 {


    struct JitConfig {
        u8 context_reg;
        u8 forward_reg;
        u8 jit_thread_count;
    };

    struct MmuConfig {
        bool enable;
        u8 addr_width;
        u8 page_bits;
        u8 readable_bit;
        u8 writable_bit;
        u8 executable_bit;
    };

    class JitCacheBlock : NonCopyable {
    public:

        JitCacheBlock();

        JitCacheBlock(VAddr stub_addr);
        ~JitCacheBlock();

        VAddr cache_start;
        VAddr cache_len;
        VAddr stub_addr;
        SpinMutex jit_lock;
    };

    class Instance : public BaseObject, NonCopyable {
    public:

        constexpr static size_t page_bits = 12;
        using JitCacheA64 = Jit::JitCache<JitCacheBlock, page_bits>;

        explicit Instance();

        const JitConfig &GetJitConfig() const;

        const MmuConfig &GetMmuConfig() const;

        const SharedPtr<Jit::FindTable<VAddr>> &GetCodeFindTable() const;

        const SharedPtr<GlobalStubs> &GetGlobalStubs() const;

        const SharedPtr<SVM::A64::A64MMU> &GetMmu() const;

        JitCacheA64::Entry *JitBlock(VAddr vaddr);
        JitCacheA64::Entry *FindJitCache(VAddr addr);
        JitCacheA64::Entry *FindAndJit(VAddr addr);
        JitCacheA64::Entry *CommitJitCache(const JitCacheA64::Entry &entry);

    private:
        JitConfig context_config_;
        MmuConfig mmu_config_;
        SharedPtr<Jit::FindTable<VAddr>> code_find_table_;
        SharedPtr<GlobalStubs> global_stubs_;
        SharedPtr<JitCacheA64> jit_cache_;
        SharedPtr<A64MMU> mmu_;
    };

    class Core : public BaseObject {
    public:
    private:

    };

}
