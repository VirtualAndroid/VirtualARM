//
// Created by SwiftGan on 2020/6/17.
//

#include <base/log.h>
#include "svm_jit_context.h"
#include "svm_arm64.h"

using namespace SVM::A64;
using namespace Jit;
using namespace Jit::A64;

#define __ masm_.

const Register &RegisterAllocator::ContextPtr() {
    if (!context_ptr_.IsValid()) {
        context_ptr_ = context_->LoadContextPtr();
        MarkInUsed(context_ptr_);
    }
    return context_ptr_;
}

void RegisterAllocator::ClearContext() {
    if (context_ptr_.IsValid()) {
        MarkInUsed(context_ptr_, false);
        context_->ClearContextPtr(context_ptr_);
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

void RegisterAllocator::Initialize(JitContext *context) {
    context_ = context;
}

LabelAllocator::LabelAllocator(MacroAssembler &masm) : masm_(masm) {}

LabelAllocator::~LabelAllocator() {
    for (auto label : labels_) {
        delete label;
    }
}

void LabelAllocator::SetDestBuffer(VAddr addr) {
    dest_buffer_start_ = addr;
    assert(dest_buffer_start_);
    for (auto label : labels_outstanding_) {
        ptrdiff_t offset = label.target - dest_buffer_start_;
        __ BindToOffset(label.label, offset);
    }
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

Label *LabelAllocator::AllocOutstanding(VAddr target) {
    auto label = AllocLabel();
    labels_outstanding_.push_back({target, label});
    return label;
}


JitContext::JitContext(Instance &instance) : instance_{instance}, reg_ctx_{
        XRegister::GetXRegFromCode(instance.GetJitConfig().context_reg)}, reg_forward_{
        XRegister::GetXRegFromCode(instance.GetJitConfig().forward_reg)} {
    mmu_ = instance.GetMmu().get();
    if (mmu_) {
        page_bits_ = mmu_->GetPageBits();
        address_bits_unused_ = mmu_->GetUnusedBits();
        tlb_bits_ = mmu_->Tbl()->TLBBits();
    }
}

void JitContext::SetPC(VAddr pc) {
    pc_ = pc;
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
        __ Str(reg, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_VEC_REG + 16 * reg.RealCode()));
    } else if (register_alloc_.InUsed(reg)) {
        return;
    } else if (reg.IsSP()) {
        auto &tmp = register_alloc_.AcquireTempX();
        __ Mov(tmp, reg);
        __ Str(reg, MemOperand(register_alloc_.ContextPtr(), 8 * reg.RealCode()));
        register_alloc_.ReleaseTempX(tmp);
    } else {
        __ Str(reg, MemOperand(register_alloc_.ContextPtr(), 8 * reg.RealCode()));
    }
}

void JitContext::Pop(const Register &reg) {
    if (reg.IsFPRegister()) {
        __ Ldr(reg, MemOperand(register_alloc_.ContextPtr(),
                               OFFSET_CTX_A64_VEC_REG + 16 * reg.RealCode()));
    } else if (register_alloc_.InUsed(reg)) {
        return;
    } else if (reg.IsSP()) {
        auto &tmp = register_alloc_.AcquireTempX();
        __ Ldr(tmp, MemOperand(register_alloc_.ContextPtr(), 8 * reg.RealCode()));
        __ Mov(reg, tmp);
        register_alloc_.ReleaseTempX(tmp);
    } else {
        __ Ldr(reg, MemOperand(register_alloc_.ContextPtr(), 8 * reg.RealCode()));
    }
}

void JitContext::MarkBlockEnd(Register tmp) {
    bool need_restore{false};
    if (!tmp.IsValid()) {
        tmp = register_alloc_.AcquireTempX();
        need_restore = true;
    }
    __ Mov(tmp, PC());
    __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, pc)));
    __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, pre_pc)));
    if (need_restore) {
        register_alloc_.ReleaseTempX(tmp);
    }
}

void JitContext::MarkReturn() {
    auto ret_addr = PC() + 4;
    Set(lr, ret_addr);
    instance_.FindAndJit(ret_addr);
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
    __ Ldr(tmp, MemOperand(reg_ctx, OFFSET_CTX_A64_INTERRUPT_SP));
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
    __ Ldr(tmp, MemOperand(reg_ctx, OFFSET_CTX_A64_INTERRUPT_SP));
    __ Mov(sp, tmp);
    // VRegs
    __ Add(tmp, reg_ctx, OFFSET_CTX_A64_VEC_REG);
    for (int i = 0; i < 32; i += 2) {
        __ Stp(GetVRegister(i), GetVRegister(i + 1),
               MemOperand(tmp, 16 * i));
    }
}

void JitContext::Forward(VAddr addr) {
    Push(reg_forward_);
    __ Mov(reg_forward_, addr);
    __ Str(reg_forward_, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_PC));
    CheckTicks();

    // Step 1: search in this module, found direct to stub

    auto jit_cache = instance_.FindAndJit(addr);
    if (jit_cache && jit_cache->Data().GetStub() &&
        current_cache_entry_->Data().code_block == jit_cache->Data().code_block) {
        Label *next_block_stub = label_allocator_.AllocOutstanding(jit_cache->Data().GetStub());
        __ B(next_block_stub);
        return;
    }

    // Step 2: if no stub, search dispatch table
    __ Mov(reg_forward_, addr);
    __ Str(reg_forward_,
           MemOperand(MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_PC)));
    LoadGlobalStub(reg_forward_, GlobalStubs::ForwardCodeCacheOffset());
    __ Br(reg_forward_);
}

void JitContext::Forward(const Register &target) {
    Push(reg_forward_);
    if (register_alloc_.InUsed(target)) {
        __ Ldr(reg_forward_, MemOperand(register_alloc_.ContextPtr(), target.RealCode() * 8));
    }
    __ Str(target, MemOperand(MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_PC)));
    CheckTicks();
    LoadGlobalStub(reg_forward_, GlobalStubs::ForwardCodeCacheOffset());
    __ Br(reg_forward_);
}

void JitContext::AddTicks(u64 ticks, Register tmp) {
    constexpr static u64 max_imm_add = (u64(1) << 12) - 1;
    assert(ticks <= max_imm_add);
    bool need_restore{false};
    if (!tmp.IsValid()) {
        tmp = register_alloc_.AcquireTempX();
        need_restore = true;
    }
    __ Ldr(tmp, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, ticks_now)));
    __ Add(tmp, tmp, ticks);
    __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, ticks_now)));
    if (need_restore) {
        register_alloc_.ReleaseTempX(tmp);
    }
}

void JitContext::CheckTicks() {
    Label *continue_label = label_allocator_.AllocLabel();
    auto tmp1 = reg_forward_;
    auto tmp2 = register_alloc_.AcquireTempX();
    auto tmp3 = register_alloc_.AcquireTempX();
    __ Ldr(tmp1, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, ticks_now)));
    __ Ldr(tmp2, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, ticks_max)));
    __ Mrs(tmp3.W(), NZCV);
    __ Subs(tmp2, tmp2, tmp1);
    register_alloc_.ReleaseTempX(tmp2);
    __ B(continue_label, Condition::gt);
    __ Msr(NZCV, tmp3.W());
    // Return Host
    register_alloc_.ReleaseTempX(tmp3);
    LoadGlobalStub(reg_forward_, GlobalStubs::ReturnToHostOffset());
    __ Br(reg_forward_);
    __ Bind(continue_label);
    __ Msr(NZCV, tmp3.W());
    register_alloc_.ReleaseTempX(tmp3);
}

void JitContext::Terminal(const Register &tmp) {
    const auto &tmp_reg = tmp.IsValid() ? tmp : register_alloc_.AcquireTempX();
    MarkBlockEnd(tmp_reg);
    AddTicks(current_block_ticks_, tmp_reg);
    if (!tmp.IsValid()) {
        register_alloc_.ReleaseTempX(tmp_reg);
    }
    terminal = true;
}

VAddr JitContext::PC() const {
    return pc_;
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

void JitContext::BeginBlock(VAddr pc) {
    terminal = false;
    current_block_ticks_ = 0;
    register_alloc_.Initialize(this);
    SetPC(pc);
    Pop(reg_forward_);
}

void JitContext::EndBlock() {
    assert(current_cache_entry_);
    auto jit_block_size = BlockCacheSize();
    auto &entry_data = current_cache_entry_->Data();
    auto buffer = entry_data.code_block->GetBuffer(entry_data.id_in_block);
    assert(buffer);
    auto buffer_start = entry_data.code_block->GetBufferStart(buffer);

    label_allocator_.SetDestBuffer(buffer_start);
    __ FinalizeCode();

    std::memcpy(reinterpret_cast<void *>(buffer_start), __ GetBuffer()->GetStartAddress<void *>(),
                jit_block_size);
    ClearCachePlatform(buffer_start, jit_block_size);

    entry_data.ready = true;
    current_cache_entry_ = nullptr;
}

Instructions::A64::AArch64Inst JitContext::Instr() {
    if (mmu_) {
        mmu_->Read<Instructions::A64::AArch64Inst>(pc_);
    } else {
        return *reinterpret_cast<Instructions::A64::AArch64Inst *>(pc_);
    }
}

void JitContext::Interrupt(const InterruptHelp &interrupt) {
    auto reg_ctx = register_alloc_.ContextPtr();
    auto tmp = reg_forward_;
    Push(reg_forward_);
    Terminal(reg_forward_);
    __ Mov(tmp, interrupt.reason);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_OF(CPUContext, interrupt.reason)));
    __ Mov(tmp, interrupt.data);
    __ Str(tmp, MemOperand(reg_ctx, OFFSET_OF(CPUContext, interrupt.data)));
    LoadGlobalStub(reg_forward_, GlobalStubs::FullInterruptOffset());
    __ Br(reg_forward_);
}

bool JitContext::Termed() const {
    return terminal;
}

void JitContext::SetCacheEntry(JitCacheEntry *entry) {
    current_cache_entry_ = entry;
}

void JitContext::Tick() {
    current_block_ticks_++;
}

size_t JitContext::BlockCacheSize() {
    return __ GetBuffer()->GetSizeInBytes();
}

void JitContext::LookupPageTable(const Register &rt, const VirtualAddress &va, bool write) {
    if (!mmu_) {
        return;
    }
    Label *label_end = label_allocator_.AllocLabel();
    const auto &mmu_config = instance_.GetMmuConfig();
    if (!va.ConstAddress()) {
        __ Str(va.VARegister(),
               MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_QUERY_PAGE));
    }
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
    Push(rt);
    Push(reg_forward_);
    __ Mov(reg_forward_, InterruptHelp::PageFatal);
    __ Str(reg_forward_.W(),
           MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, interrupt.reason)));
    if (va.ConstAddress()) {
        __ Mov(reg_forward_, va.Address());
        __ Str(reg_forward_, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_QUERY_PAGE));
    }
    LoadGlobalStub(reg_forward_, GlobalStubs::FullInterruptOffset());
    __ Br(reg_forward_);
    Pop(reg_forward_);
    __ Bind(label_end);
}

void JitContext::LookupTLB(const Register &rt, const VirtualAddress &va, Label *miss_cache) {
    if (!mmu_) {
        return;
    }
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

const Register &JitContext::LoadContextPtr() {
    const auto &context_reg = reg_ctx_;
//    __ Push(context_reg);
//    __ Mrs(context_reg, TPIDR_EL0);
//    __ Ldr(context_reg, MemOperand(context_reg, CTX_TLS_SLOT * 8));
//    // save tmp0, tmp1
//    __ Str(x16, MemOperand(context_reg, x16.RealCode() * 8));
//    __ Pop(x16);
//    __ Str(x16, MemOperand(context_reg, context_reg.RealCode() * 8));
//    // restore tmp0
//    __ Ldr(x16, MemOperand(context_reg, x16.RealCode() * 8));
    register_alloc_.MarkInUsed(context_reg);
    return context_reg;
}

void JitContext::ClearContextPtr(const Register &context) {
//    __ Ldr(context, MemOperand(context, 8 * context.RealCode()));
//    register_alloc_.MarkInUsed(context, false);
}

void JitContext::LoadGlobalStub(const Register &target, u32 stub_offset) {
    __ Ldr(target, MemOperand(register_alloc_.ContextPtr(), OFFSET_OF(CPUContext, host_stubs)));
    __ Ldr(target, MemOperand(target, stub_offset));
}

void JitContext::ABICall(const ABICallHelp &call_help) {
    auto reg_ctx = register_alloc_.ContextPtr();
    auto tmp = reg_forward_;
    Push(reg_forward_);
    Terminal(reg_forward_);
    __ Mov(tmp, call_help.reason);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_OF(CPUContext, abi_call.reason)));
    __ Mov(tmp, call_help.data);
    __ Str(tmp, MemOperand(reg_ctx, OFFSET_OF(CPUContext, abi_call.data)));
    LoadGlobalStub(reg_forward_, GlobalStubs::FullInterruptOffset());
    __ Br(reg_forward_);
}

void JitContext::ABICall(const ABICallHelp::Reason call, const Register &xt) {
    auto reg_ctx = register_alloc_.ContextPtr();
    auto tmp = reg_forward_;
    if (xt != tmp) {
        Push(tmp);
        __ Mov(tmp, xt);
    }
    __ Str(tmp, MemOperand(reg_ctx, OFFSET_OF(CPUContext, abi_call.data)));
    __ Mov(tmp, call);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_OF(CPUContext, abi_call.reason)));
    Terminal(tmp);
    LoadGlobalStub(tmp, GlobalStubs::FullInterruptOffset());
    __ Br(tmp);
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

