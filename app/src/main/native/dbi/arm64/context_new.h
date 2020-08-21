//
// Created by SwiftGan on 2020/6/17.
//

#pragma once

#include <aarch64/macro-assembler-aarch64.h>
#include "asm/arm64/cpu_arm64.h"
#include <base/marcos.h>
#include <asm/arm64/instruction_fields.h>
#include "block/code_find_table.h"
#include "dbi_mmu_arm64.h"
#include "block/host_code_block.h"

using namespace vixl::aarch64;
using namespace CodeCache;
using namespace CodeCache::A64;
using namespace CPU::A64;

#define CTX_TLS_SLOT 7

namespace DBI::A64 {
    class BaseContext;

    class RegisterAllocator;

    class Instance;

    class RegisterAllocator {
    public:
        const Register &ContextPtr();

        void ClearContext();

        const Register &AcquireTempX();

        void ReleaseTempX(const Register &x);

        void MarkInUsed(const Register &x, bool in_used = true);

        bool InUsed(const Register &x);

        void Reset();

    private:
        bool in_used_[32]{false};
        BaseContext *context_;
        Register *context_ptr_;
    };

    class LabelAllocator {
    public:

        LabelAllocator(MacroAssembler &masm);

        ~LabelAllocator();

        void SetDestBuffer(VAddr addr);

        void Reset();

        Label *AllocLabel();

        Label *GetDispatcherLabel();

        Label *GetMapAddressLabel();

        Label *GetPageLookupLabel();

        void BindDispatcherTrampoline(VAddr addr);

        void BindPageLookupTrampoline(VAddr addr);

        void BindMapAddress(VAddr addr);

    private:
        VAddr dest_buffer_start_;
        MacroAssembler &masm_;
        std::list<Label *> labels_;
        Label *dispatcher_label_;
        Label *page_lookup_label_;
        Label *map_address_label_;
    };

    class BaseContext : public BaseObject {
    public:
        BaseContext(const SharedPtr<DBI::A64::Instance> &instance);

        virtual const Register &LoadContextPtr() = 0;

        virtual void ClearContextPtr(const Register &context) = 0;

        void Set(const Register &x, u64 value);

        void Set(const Register &x, u32 value);

        virtual void Push(const Register &reg);

        virtual void Pop(const Register &reg);

        virtual void Push(const Register &reg1, const Register &reg2);

        virtual void Pop(const Register &reg1, const Register &reg2);

        void SaveContext();

        void LoadContext();

        void Terminal(bool check_suspend = false);

        void Forward(VAddr addr);

        void Forward(const Register &target);

        void ToContextSwitch(const ContextSwitcher &switcher);

        virtual void LookupPageTable(const Register &rt) {};

        void EndBlock();

        VAddr PC() const;

        const Instance &GetInstance() const;

        Instructions::A64::AArch64Inst *Instr();

        MacroAssembler &Assembler();

        RegisterAllocator &GetRegisterAlloc();

        LabelAllocator &GetLabelAlloc();

    protected:
        void MarkPC();

        void AddTicks(u64 ticks);

        SharedPtr<Instance> instance_;
        const Register &reg_ctx_;
        const Register &reg_forward_;
        RegisterAllocator register_alloc_;
        // must pic code
        MacroAssembler masm_{PositionIndependentCode};
        LabelAllocator label_allocator_{masm_};
        SharedPtr<FindTable<VAddr>> code_find_table_;
        Instructions::A64::AArch64Inst *pc_;
        size_t current_block_ticks_;
    };

    class QuickContext : public BaseContext {
    public:
        QuickContext(const SharedPtr<DBI::A64::Instance> &instance);

        const Register &LoadContextPtr() override;

        void ClearContextPtr(const Register &context) override;
    };

    using ContextA64 = SharedPtr<BaseContext>;

    class RegisterGuard {
    public:
        RegisterGuard(const ContextA64 &context, const Register &target);

        ~RegisterGuard();

        const Register &Target() const;

        void Dirty();

    private:
        ContextA64 context_;
        const Register &target_;
        Register tmp;
        bool use_tmp{false};
        bool dirty{false};
    };

}
