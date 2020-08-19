//
// Created by SwiftGan on 2020/6/17.
//

#pragma once

#include <aarch64/macro-assembler-aarch64.h>
#include "asm/arm64/cpu_arm64.h"
#include <base/marcos.h>
#include "block/code_find_table.h"
#include "dbi_mmu_arm64.h"
#include "block/host_code_block.h"

using namespace vixl::aarch64;
using namespace CodeCache;
using namespace CodeCache::A64;

#define CTX_TLS_SLOT 7

namespace DBI::A64 {

    class BaseContext;
    class RegisterAllocator;

    class RegisterAllocator : public BaseObject {
    public:
        const Register &ContextPtr();
        void ClearContext();
        const Register &AcquireTempX();
        const Register &ReleaseTempX(const Register &x);
        void MarkInUsed(const Register &x, bool in_used = true);
        bool InUsed(const Register &x);
        void Reset();
    private:
        bool in_used_[32]{false};
        BaseContext* context_;
        Register *context_ptr_;
    };

    class BaseContext : public BaseObject {
    public:
        virtual const Register &LoadContextPtr() = 0;
        virtual void ClearContextPtr(const Register &context) = 0;
        void Set(const Register &x, u64 value);
        void Set(const Register &x, u32 value);
        virtual void Push(const Register &reg);
        virtual void Pop(const Register &reg);
        virtual void Push(const Register &reg1, const Register &reg2);
        virtual void Pop(const Register &reg1, const Register &reg2);
        void MarkPC(u64 value);
        void SaveContext();
        void LoadContext();
    protected:
        RegisterAllocator register_alloc_;
        // must pic code
        MacroAssembler masm_{PositionIndependentCode};
        SharedPtr<FindTable<VAddr>> code_find_table_;
    };

    class QuickContext : public BaseContext {
    public:
        const Register &LoadContextPtr() override;
        void ClearContextPtr(const Register &context) override;
    };

}
