//
// Created by 甘尧 on 2020-02-29.
//
#pragma once

#include <base/marcos.h>
#include "svm_global_stubs.h"
#include "svm_mmu.h"
#include "svm_jit_manager.h"

namespace Jit {
    template<typename T>
    class FindTable;
}

using namespace Jit::A64;

namespace SVM::A64 {

    struct JitConfig {
        u8 context_reg;
        u8 forward_reg;
        u8 jit_thread_count;
        bool protect_code;
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

        const JitConfig &GetJitConfig() const;

        const MmuConfig &GetMmuConfig() const;

        const SharedPtr<Jit::FindTable<VAddr>> &GetCodeFindTable() const;

        const SharedPtr<GlobalStubs> &GetGlobalStubs() const;

        const SharedPtr<SVM::A64::A64MMU> &GetMmu() const;

        const SharedPtr<JitManager> &GetJitManager() const;

        JitCacheEntry *FindAndJit(VAddr addr);

    private:
        JitConfig context_config_;
        MmuConfig mmu_config_;
        SharedPtr<Jit::FindTable<VAddr>> code_find_table_;
        SharedPtr<GlobalStubs> global_stubs_;
        SharedPtr<JitManager> jit_manager_;
        SharedPtr<A64MMU> mmu_;
    };

    class Core : public BaseObject {
    public:
    private:

    };

}
