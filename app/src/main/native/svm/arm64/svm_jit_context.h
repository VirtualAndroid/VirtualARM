//
// Created by SwiftGan on 2020/6/17.
//

#pragma once

#include <aarch64/macro-assembler-aarch64.h>
#include "asm/arm64/cpu_arm64.h"
#include <base/marcos.h>
#include <asm/arm64/instruction_fields.h>
#include "block/code_find_table.h"
#include "svm_mmu.h"
#include "block/host_code_block.h"
#include "svm_jit_manager.h"

using namespace Jit::A64;
using namespace CPU::A64;
using namespace SVM::A64;

#define CTX_TLS_SLOT 7

namespace SVM::A64 {
    class Instance;
    class GlobalStubs;
}

namespace Jit::A64 {

    using namespace vixl::aarch64;

    class JitContext;
    class RegisterAllocator;

    class RegisterAllocator {
    public:
        const Register &ContextPtr();

        void ClearContext();

        const Register &AcquireTempX();

        void ReleaseTempX(const Register &x);

        void MarkInUsed(const Register &x, bool in_used = true);

        bool InUsed(const Register &x);

        void Reset(JitContext *context);

    private:
        bool in_used_[32]{false};
        JitContext *context_;
        Register context_ptr_ = NoReg;
    };

    class LabelAllocator {
    public:

        LabelAllocator(MacroAssembler &masm);

        ~LabelAllocator();

        void SetDestBuffer(VAddr addr);

        void Reset();

        Label *AllocLabel();

        Label *AllocOutstanding(VAddr target);

        Label *GetDispatcherLabel();

        Label *GetMapAddressLabel();

        Label *GetPageLookupLabel();

        void BindDispatcherTrampoline(VAddr addr);

        void BindPageLookupTrampoline(VAddr addr);

        void BindMapAddress(VAddr addr);

    private:

        struct Outstanding {
            VAddr target;
            Label *label;
        };

        VAddr dest_buffer_start_;
        MacroAssembler &masm_;
        std::list<Label *> labels_;
        std::list<Outstanding> labels_outstanding_;
        Label *dispatcher_label_;
        Label *page_lookup_label_;
        Label *map_address_label_;
    };

    class VirtualAddress final {
    public:
        constexpr VirtualAddress(const Register &rt) : rt_{rt}, vaddr_(0), const_addr_{false} {};
        constexpr VirtualAddress(const VAddr &vaddr) : rt_{x0}, vaddr_(vaddr), const_addr_{true} {};

        const bool ConstAddress() const {
            return const_addr_;
        }

        const VAddr Address() const {
            return vaddr_;
        }

        const Register &VARegister() const {
            return rt_;
        }
    private:
        const Register &rt_;
        const VAddr vaddr_;
        const bool const_addr_;
    };

    class JitContext : public BaseObject, NonCopyable {
    public:
        JitContext(const SharedPtr<SVM::A64::Instance> &instance);

        virtual const Register &LoadContextPtr() {};

        virtual void ClearContextPtr(const Register &context) {};

        const Register &GetXRegister(u8 code, bool may_sp = false);

        const VRegister &GetVRegister(u8 code);

        void SetPC(VAddr pc);

        void Tick();

        void Set(const Register &x, u64 value);

        void Set(const Register &x, u32 value);

        virtual void Push(const Register &reg);

        virtual void Pop(const Register &reg);

        virtual void Push(const Register &reg1, const Register &reg2);

        virtual void Pop(const Register &reg1, const Register &reg2);

        void SaveContext();

        void LoadContext();

        void MarkReturn();

        void Terminal(const Register& tmp = NoReg);

        void CheckTicks();

        void Forward(VAddr addr);

        void Forward(const Register &target);

        void Interrupt(const InterruptHelp &interrupt);

        virtual void LookupPageTable(const Register &rt, const VirtualAddress &va, bool write = false) {};

        void BeginBlock(VAddr start);

        void SetCacheEntry(JitCacheEntry *entry);

        void EndBlock();

        VAddr PC() const;

        bool Termed() const;

        size_t BlockCacheSize();

        virtual Instructions::A64::AArch64Inst Instr();

        MacroAssembler &Assembler();

        RegisterAllocator &GetRegisterAlloc();

        LabelAllocator &GetLabelAlloc();

    protected:
        void MarkBlockEnd(Register tmp = NoReg);
        void AddTicks(u64 ticks, Register tmp = NoReg);

        SharedPtr<Instance> instance_;
        const Register &reg_ctx_;
        const Register &reg_forward_;
        RegisterAllocator register_alloc_;
        // must pic code
        MacroAssembler masm_{PositionIndependentCode};
        LabelAllocator label_allocator_{masm_};
        SharedPtr<FindTable<VAddr>> code_find_table_;
        SharedPtr<GlobalStubs> global_stubs_;
        VAddr pc_;
        u32 current_block_ticks_;
        bool terminal{false};
        JitCacheEntry *current_cache_entry_;
    };

    class QuickContext : public JitContext {
    public:
        QuickContext(const SharedPtr<Instance> &instance);

        const Register &LoadContextPtr() override;

        void ClearContextPtr(const Register &context) override;
    };

    class ContextWithMmu : public JitContext {
    public:

        ContextWithMmu(const SharedPtr<Instance> &instance);

        const Register &LoadContextPtr() override;

        void LookupPageTable(const Register &rt, const VirtualAddress &va, bool write) override;

        Instructions::A64::AArch64Inst Instr() override;

    private:

        void LookupTLB(const Register &rt, const VirtualAddress &va, Label *miss_cache);

        u8 address_bits_unused_;
        u8 page_bits_;
        u8 tlb_bits_;
        SharedPtr<A64MMU> mmu_;
    };

    using ContextA64 = SharedPtr<JitContext>;

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
