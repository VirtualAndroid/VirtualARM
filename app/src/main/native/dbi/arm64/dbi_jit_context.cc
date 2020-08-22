//
// Created by SwiftGan on 2020/6/17.
//

#include "dbi_jit_context.h"
#include "dbi_arm64.h"

using namespace DBI::A64;
using namespace Jit;

#define __ masm_.

const Register &RegisterAllocator::ContextPtr() {
    if (!context_ptr_) {
        context_ptr_ = const_cast<Register *>(&context_->LoadContextPtr());
        MarkInUsed(*context_ptr_);
    }
    return *context_ptr_;
}

void RegisterAllocator::ClearContext() {
    if (context_ptr_) {
        MarkInUsed(*context_ptr_, false);
        context_->ClearContextPtr(*context_ptr_);
    }
}

const Register &RegisterAllocator::AcquireTempX() {
    for (int i = 0; i < 31; ++i) {
        if (!in_used_[i]) {
            auto &res = context_->GetXRegister(i);
            context_->Push(res);
            MarkInUsed(res);
            return res;
        }
    }
}

void RegisterAllocator::ReleaseTempX(const Register &x) {
    MarkInUsed(x, false);
    context_->Pop(x);
}

void RegisterAllocator::MarkInUsed(const Register &x, bool in_used) {
    in_used_[x.RealCode()] = in_used;
}

bool RegisterAllocator::InUsed(const Register &x) {
    return in_used_[x.RealCode()];
}

void RegisterAllocator::Reset() {
    std::memset(reinterpret_cast<void *>(in_used_[0]), 0, sizeof(in_used_));
}

LabelAllocator::LabelAllocator(MacroAssembler &masm) : masm_(masm) {
    Reset();
}

LabelAllocator::~LabelAllocator() {
    for (auto label : labels_) {
        delete label;
    }
}

void LabelAllocator::SetDestBuffer(VAddr addr) {
    dest_buffer_start_ = addr;
}

void LabelAllocator::Reset() {
    for (auto label : labels_) {
        delete label;
    }
    labels_.clear();
    dest_buffer_start_ = 0;
    dispatcher_label_ = AllocLabel();
    page_lookup_label_ = AllocLabel();
    map_address_label_ = AllocLabel();
}

Label *LabelAllocator::AllocLabel() {
    auto label = new Label();
    labels_.push_back(label);
    return label;
}

Label *LabelAllocator::GetDispatcherLabel() {
    return dispatcher_label_;
}

Label *LabelAllocator::GetPageLookupLabel() {
    return page_lookup_label_;
}

Label *LabelAllocator::GetMapAddressLabel() {
    return map_address_label_;
}

void LabelAllocator::BindDispatcherTrampoline(VAddr addr) {
    assert(dest_buffer_start_);
    ptrdiff_t offset = addr - dest_buffer_start_;
    __ BindToOffset(dispatcher_label_, offset);
}

void LabelAllocator::BindPageLookupTrampoline(VAddr addr) {
    assert(dest_buffer_start_);
    ptrdiff_t offset = addr - dest_buffer_start_;
    __ BindToOffset(page_lookup_label_, offset);
}

void LabelAllocator::BindMapAddress(VAddr addr) {
    assert(dest_buffer_start_);
    ptrdiff_t offset = addr - dest_buffer_start_;
    __ BindToOffset(map_address_label_, offset);
}

const VRegister &JitContext::GetVRegister(u8 code) {
    return VRegister::GetVRegFromCode(code);
}

const Register &JitContext::GetXRegister(u8 code, bool may_sp) {
    if (may_sp && code == 31) {
        code = kSPRegInternalCode;
    }
    return XRegister::GetXRegFromCode(code);
}


void JitContext::Set(const Register &x, u64 value) {
    if (!register_alloc_.InUsed(x)) {
        __ Mov(x, value);
    } else {
        register_alloc_.MarkInUsed(x);
        auto tmp = register_alloc_.AcquireTempX();
        __ Mov(tmp, value);
        __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), 8 * x.RealCode()));
        register_alloc_.ReleaseTempX(tmp);
        register_alloc_.MarkInUsed(x, false);
    }
}

void JitContext::Set(const Register &w, u32 value) {
    if (!register_alloc_.InUsed(w)) {
        __ Mov(w, value);
    } else {
        register_alloc_.MarkInUsed(w);
        auto tmp = register_alloc_.AcquireTempX();
        __ Mov(tmp.W(), value);
        __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), 8 * w.RealCode()));
        register_alloc_.ReleaseTempX(tmp);
        register_alloc_.MarkInUsed(w, false);
    }
}

void JitContext::Push(const Register &reg) {
    if (reg.IsFPRegister()) {
        __ Str(reg, MemOperand(register_alloc_.ContextPtr(), 8 * reg.RealCode()));
    } else if (register_alloc_.InUsed(reg)) {
        return;
    } else if (reg.IsSP()) {
        auto &tmp = register_alloc_.AcquireTempX();
        __ Mov(tmp, reg);
        __ Str(reg, MemOperand(register_alloc_.ContextPtr(), 8 * reg.RealCode()));
        register_alloc_.ReleaseTempX(tmp);
    } else {
        register_alloc_.MarkInUsed(reg);
        __ Str(reg, MemOperand(register_alloc_.ContextPtr(), 16 * reg.RealCode()));
        register_alloc_.MarkInUsed(reg, false);
    }
}

void JitContext::Pop(const Register &reg) {
    if (reg.IsFPRegister()) {
        __ Ldr(reg, MemOperand(register_alloc_.ContextPtr(), 8 * reg.RealCode()));
    } else if (register_alloc_.InUsed(reg)) {
        return;
    } else if (reg.IsSP()) {
        auto &tmp = register_alloc_.AcquireTempX();
        __ Ldr(tmp, MemOperand(register_alloc_.ContextPtr(), 8 * reg.RealCode()));
        __ Mov(reg, tmp);
        register_alloc_.ReleaseTempX(tmp);
    } else {
        __ Ldr(reg, MemOperand(register_alloc_.ContextPtr(),
                               OFFSET_CTX_A64_VEC_REG + 16 * reg.RealCode()));
    }
}

void JitContext::MarkPC() {
    auto tmp = register_alloc_.AcquireTempX();
    __ Mov(tmp, PC());
    __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_PC));
    register_alloc_.ReleaseTempX(tmp);
}

void JitContext::MarkReturn() {
    auto ret_addr = PC() + 4;
    Set(lr, ret_addr);
    auto jit_cache = instance_->FindAndJit(ret_addr);
    if (jit_cache && !jit_cache->disabled) {
        code_find_table_->FillCodeAddress(ret_addr, jit_cache->Data().stub_addr);
    }
}

void JitContext::Push(const Register &reg1, const Register &reg2) {
    if ((reg2.RealCode() - reg1.RealCode() == 1)
        && (!reg1.IsSP() && !reg2.IsSP())
        && (!register_alloc_.InUsed(reg1) && !register_alloc_.InUsed(reg2))) {
        __ Stp(reg1, reg2, MemOperand(register_alloc_.ContextPtr(), 8 * reg1.RealCode()));
    } else {
        Push(reg1);
        Push(reg2);
    }
}

void JitContext::Pop(const Register &reg1, const Register &reg2) {
    if ((reg2.RealCode() - reg1.RealCode() == 1)
        && (!reg1.IsSP() && !reg2.IsSP())
        && (!register_alloc_.InUsed(reg1) && !register_alloc_.InUsed(reg2))) {
        __ Ldp(reg1, reg2, MemOperand(register_alloc_.ContextPtr(), 8 * reg1.RealCode()));
    } else {
        Pop(reg1);
        Pop(reg2);
    }
}

void JitContext::SaveContext() {
    auto reg_ctx = register_alloc_.ContextPtr();
    auto tmp = register_alloc_.AcquireTempX();
    // XRegs
    for (int i = 0; i < 30; i += 2) {
        Push(GetXRegister(i), GetXRegister(i + 1));
    }
    // lr
    Push(lr);
    // Sysregs
    __ Mrs(tmp.W(), NZCV);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_CTX_A64_PSTATE));
    __ Mrs(tmp.W(), FPCR);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_CTX_A64_FPCR));
    __ Mrs(tmp.W(), FPSR);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_CTX_A64_FPSR));
    // Save Guest sp
    Push(sp);
    // Prepare host SP
    __ Ldr(tmp, MemOperand(reg_ctx, OFFSET_CTX_A64_HOST_SP));
    __ Mov(sp, tmp);
    // VRegs
    __ Add(tmp, reg_ctx, OFFSET_CTX_A64_VEC_REG);
    for (int i = 0; i < 32; i += 2) {
        __ Stp(GetVRegister(i), GetVRegister(i + 1),
               MemOperand(tmp, 16 * i));
    }
    register_alloc_.ReleaseTempX(tmp);
}

void JitContext::LoadContext() {
    auto reg_ctx = register_alloc_.ContextPtr();
    auto tmp = register_alloc_.AcquireTempX();
    // XRegs
    for (int i = 0; i < 30; i += 2) {
        Push(GetXRegister(i), GetXRegister(i + 1));
    }
    // lr
    Push(lr);
    // Sysregs
    __ Mrs(tmp.W(), NZCV);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_CTX_A64_PSTATE));
    __ Mrs(tmp.W(), FPCR);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_CTX_A64_FPCR));
    __ Mrs(tmp.W(), FPSR);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_CTX_A64_FPSR));
    // Save Guest sp
    Push(sp);
    // Prepare host SP
    __ Ldr(tmp, MemOperand(reg_ctx, OFFSET_CTX_A64_HOST_SP));
    __ Mov(sp, tmp);
    // VRegs
    __ Add(tmp, reg_ctx, OFFSET_CTX_A64_VEC_REG);
    for (int i = 0; i < 32; i += 2) {
        __ Stp(GetVRegister(i), GetVRegister(i + 1),
               MemOperand(tmp, 16 * i));
    }
}

void JitContext::Forward(VAddr addr) {
    // Step 1: search in this module, found direct to stub

    // Step 2: if no stub
    __ Push(reg_forward_);
    __ Mov(reg_forward_, addr);
    __ Str(reg_forward_, MemOperand(MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_FORWARD)));
    __ Mov(reg_forward_, global_stubs_->GetForwardCodeCache());
    __ Br(reg_forward_);
}

void JitContext::Forward(const Register &target) {
    Push(reg_forward_);
    if (register_alloc_.InUsed(target)) {
        __ Ldr(reg_forward_, MemOperand(register_alloc_.ContextPtr(), target.RealCode() * 8));
    }
    __ Str(target, MemOperand(MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_FORWARD)));
    __ Mov(reg_forward_, global_stubs_->GetForwardCodeCache());
    __ Br(reg_forward_);
}

void JitContext::AddTicks(u64 ticks, bool check_remain) {
    constexpr static u64 max_imm_add = (1 << 12) - 1;
    assert(ticks <= max_imm_add);
    auto tmp = register_alloc_.AcquireTempX();
    __ Ldr(tmp, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, ticks_now)));
    __ Add(tmp, tmp, ticks);
    __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, ticks_now)));
    if (check_remain) {
        Label *continue_label = label_allocator_.AllocLabel();
        auto tmp2 = register_alloc_.AcquireTempX();
        auto tmp3 = register_alloc_.AcquireTempX();
        __ Ldr(tmp2, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, ticks_max)));
        __ Mrs(tmp3.W(), NZCV);
        __ Subs(tmp2, tmp2, tmp);
        __ B(continue_label, Condition::gt);
        __ Msr(NZCV, tmp3.W());
        // TODO Return Host

        __ Bind(continue_label);
        __ Msr(NZCV, tmp3.W());
        register_alloc_.ReleaseTempX(tmp2);
        register_alloc_.ReleaseTempX(tmp3);
    }
    register_alloc_.ReleaseTempX(tmp);
}

void JitContext::Terminal(bool check_suspend) {
    MarkPC();
    AddTicks(current_block_ticks_, check_suspend);
}

VAddr JitContext::PC() const {
    return reinterpret_cast<VAddr>(pc_);
}

MacroAssembler &JitContext::Assembler() {
    return masm_;
}

RegisterAllocator &JitContext::GetRegisterAlloc() {
    return register_alloc_;
}

LabelAllocator &JitContext::GetLabelAlloc() {
    return label_allocator_;
}

void JitContext::EndBlock() {
    __ Reset();
    register_alloc_.Reset();
    label_allocator_.Reset();
}

Instructions::A64::AArch64Inst *JitContext::Instr() {
    return pc_;
}

const Instance &JitContext::GetInstance() const {
    return *instance_;
}

void JitContext::Interrupt(const InterruptHelp &interrupt) {
    Terminal();
    auto reg_ctx = register_alloc_.ContextPtr();
    auto tmp = register_alloc_.AcquireTempX();
    __ Mov(tmp, interrupt.reason);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_OF(CPUContext, interrupt.reason)));
    __ Mov(tmp, interrupt.data);
    __ Str(tmp, MemOperand(reg_ctx, OFFSET_OF(CPUContext, interrupt.data)));
    register_alloc_.ReleaseTempX(tmp);
    Push(reg_forward_);
    __ Mov(reg_forward_, instance_->GetGlobalStubs()->GetFullSwitchGuestToHost());
    __ Br(reg_forward_);
    EndBlock();
}

JitContext::JitContext(const SharedPtr<Instance> &instance) : instance_{instance}, reg_ctx_{
        XRegister::GetXRegFromCode(instance->GetContextConfig().context_reg)}, reg_forward_{
        XRegister::GetXRegFromCode(instance->GetContextConfig().forward_reg)} {
    global_stubs_ = instance->GetGlobalStubs();
}

QuickContext::QuickContext(const SharedPtr<Instance> &instance) : JitContext(instance) {

}

const Register &QuickContext::LoadContextPtr() {
    const auto &context_reg = reg_ctx_;
    __ Push(context_reg);
    __ Mrs(context_reg, TPIDR_EL0);
    __ Ldr(context_reg, MemOperand(context_reg, CTX_TLS_SLOT * 8));
    // save tmp0, tmp1
    __ Str(x16, MemOperand(context_reg, x16.RealCode() * 8));
    __ Pop(x16);
    __ Str(x16, MemOperand(context_reg, context_reg.RealCode() * 8));
    // restore tmp0
    __ Ldr(x16, MemOperand(context_reg, x16.RealCode() * 8));
    register_alloc_.MarkInUsed(context_reg);
    return context_reg;
}

void QuickContext::ClearContextPtr(const Register &context) {
    __ Ldr(context, MemOperand(context, 8 * context.RealCode()));
    register_alloc_.MarkInUsed(context, false);
}

ContextWithMmu::ContextWithMmu(const SharedPtr<Instance> &instance, SharedPtr<A64MMU> mmu)
        : JitContext(instance), mmu_{mmu} {
    page_bits_ = mmu->GetPageBits();
    address_bits_unused_ = mmu->GetUnusedBits();
    tlb_bits_ = mmu->Tbl()->TLBBits();
}

void ContextWithMmu::LookupPageTable(const Register &rt, const VirtualAddress &va, bool write) {
    Label *label_end = label_allocator_.AllocLabel();
    const auto &mmu_config = instance_->GetMmuConfig();
    if (mmu_->GetLevel() == 1) {
        if (va.ConstAddress()) {
            auto tmp1 = register_alloc_.AcquireTempX();
            __ Ldr(rt, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_PAGE_TABLE));
            __ Mov(tmp1, BitRange<VAddr>(va.Address(), page_bits_,
                                         sizeof(VAddr) * 8 - address_bits_unused_ - 1) * 8);
            __ Add(rt, rt, tmp1);
            __ Ldr(rt, MemOperand(rt));
            register_alloc_.ReleaseTempX(tmp1);
        } else {
            auto tmp1 = register_alloc_.AcquireTempX();
            auto tmp2 = register_alloc_.AcquireTempX();
            __ Lsr(tmp1, va.VARegister(), page_bits_);
            __ Bfc(tmp1, sizeof(VAddr) * 8 - address_bits_unused_ - page_bits_,
                   address_bits_unused_ + page_bits_);
            __ Ldr(tmp2, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_PAGE_TABLE));
            __ Add(tmp1, tmp2, Operand(tmp1, LSL, 3));
            __ Ldr(rt, MemOperand(tmp1));
            register_alloc_.ReleaseTempX(tmp1);
            register_alloc_.ReleaseTempX(tmp2);
        }
    } else {
        // multi level
        Label *label_miss = label_allocator_.AllocLabel();
        LookupTLB(rt, va, label_miss);
        __ B(label_end);
        // look up page table
        if (va.ConstAddress()) {

        }
    }
    __ Tbnz(rt, write ? mmu_config.writable_bit : mmu_config.readable_bit, label_end);
    Push(reg_forward_);
    __ Mov(reg_forward_, PC() + 4);
    __ Str(reg_forward_, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_FORWARD));
    __ Mov(reg_forward_, global_stubs_->GetFullSwitchGuestToHost());
    __ Br(reg_forward_);
    __ Pop(reg_forward_);
    __ Bind(label_end);
}

void ContextWithMmu::LookupTLB(const Register &rt, const VirtualAddress &va, Label *miss_cache) {
    auto tmp1 = register_alloc_.AcquireTempX();
    auto tmp2 = register_alloc_.AcquireTempX();
    if (!va.ConstAddress()) {
        __ Mov(tmp1, Operand(va.VARegister(), LSR, page_bits_));
        __ Bfc(tmp1, tlb_bits_, sizeof(VAddr) * 8 - tlb_bits_);
        __ Ldr(tmp2, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_TLB));
        // PTE size of a64 = 8, key = 8,so size of tlb entry = 16
        __ Add(tmp2, tmp2, Operand(tmp1, LSL, 4));
        __ Ldr(tmp1, MemOperand(tmp2));
        __ Sub(tmp1, tmp1, Operand(va.VARegister(), LSR, page_bits_));
        __ Cbnz(tmp1, miss_cache);
        __ Ldr(rt, MemOperand(tmp2, 8));
        // miss cache
    } else {
        __ Ldr(tmp1, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_TLB));
        __ Mov(tmp2, BitRange<VAddr>(va.Address(), page_bits_, tlb_bits_) << 4);
        __ Add(tmp2, tmp1, tmp2);
        __ Ldr(tmp1, MemOperand(tmp2));
        __ Mov(rt, va.Address() >> page_bits_);
        __ Sub(tmp1, rt, tmp1);
        __ Cbnz(tmp1, miss_cache);
        __ Ldr(rt, MemOperand(tmp2, 8));
        // miss cache
    }
    register_alloc_.ReleaseTempX(tmp1);
    register_alloc_.ReleaseTempX(tmp2);
}

RegisterGuard::RegisterGuard(const ContextA64 &context, const Register &target) : context_(context),
                                                                                  target_(target) {
    auto &reg_allocator = context->GetRegisterAlloc();
    use_tmp = reg_allocator.InUsed(target);
    if (use_tmp) {
        tmp = reg_allocator.AcquireTempX();
        context_->Assembler().Ldr(tmp,
                                  MemOperand(reg_allocator.ContextPtr(), target_.RealCode() * 8));
    }
}

RegisterGuard::~RegisterGuard() {
    if (use_tmp) {
        auto &reg_allocator = context_->GetRegisterAlloc();
        // if dirty, need write back
        if (dirty) {
            context_->Assembler().Str(tmp, MemOperand(reg_allocator.ContextPtr(),
                                                      target_.RealCode() * 8));
        }
        reg_allocator.ReleaseTempX(tmp);
    }
}

void RegisterGuard::Dirty() {
    dirty = true;
}

const Register &RegisterGuard::Target() const {
    if (use_tmp) {
        return tmp;
    } else {
        return target_;
    }
}

#undef __

