//
// Created by 甘尧 on 2020-02-29.
//
#pragma once

#include <base/marcos.h>
#include "svm_global_stubs.h"
#include "svm_mmu.h"
#include "svm_jit_manager.h"
#include "block/host_code_block.h"
#include "block/code_set.h"
#include <boost/icl/interval_map.hpp>
#include <list>

namespace Jit {
    template<typename T>
    class FindTable;
}

namespace SVM::A64 {

    using namespace Jit::A64;

    struct JitConfig {
        u8 context_reg;
        u8 forward_reg;
        u8 jit_thread_count;
        bool protect_code;
        bool use_host_clock;
    };

    struct MmuConfig {
        bool enable;
        u8 addr_width;
        u8 page_bits;
        u8 readable_bit;
        u8 writable_bit;
        u8 executable_bit;
    };

    class Instance : public BaseObject {
    public:

        explicit Instance();

        Instance(const JitConfig &jit, const MmuConfig &mmu);

        virtual ~Instance();

        void Initialize();

        const JitConfig &GetJitConfig() const;

        const MmuConfig &GetMmuConfig() const;

        const SharedPtr<Jit::FindTable<VAddr>> &GetCodeFindTable() const;

        const SharedPtr<GlobalStubs> &GetGlobalStubs() const;

        const SharedPtr<SVM::A64::A64MMU> &GetMmu() const;

        const SharedPtr<JitManager> &GetJitManager() const;

        CodeBlock *AllocCacheBlock(u32 size);

        void RegisterCodeSet(const std::shared_ptr<Jit::CodeSet> &code_set);

        JitCacheEntry *FindAndJit(VAddr addr);

        CodeBlock *PeekCacheBlock(VAddr pc);

    private:

        void ProtectCodeSegment(VAddr start, VAddr end);

        JitConfig jit_config_;
        MmuConfig mmu_config_;
        SharedPtr<Jit::FindTable<VAddr>> code_find_table_;
        SharedPtr<GlobalStubs> global_stubs_;
        SharedPtr<JitManager> jit_manager_;
        SharedPtr<A64MMU> mmu_;

        std::shared_mutex code_set_lock_;
        std::list<std::shared_ptr<Jit::CodeSet>> code_sets_;
        std::list<std::unique_ptr<CodeBlock>> cache_blocks_;
        std::list<CodeBlock*> isolate_cache_blocks_;
        std::unordered_map<Jit::CodeSet*, CodeBlock*> cache_blocks_set_;
        using IntervalCache = boost::icl::interval_map<VAddr, CodeBlock*>;
        using IntervalType = typename IntervalCache::interval_type;
        IntervalCache cache_blocks_addresses_;
    };

    class Core : public BaseObject {
    public:
    private:

    };

}
