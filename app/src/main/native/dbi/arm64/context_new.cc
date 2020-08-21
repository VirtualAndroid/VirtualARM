//
// Created by SwiftGan on 2020/6/17.
//

#include "context_new.h"
#include "dbi_arm64.h"

using namespace DBI::A64;

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
            auto &res = XRegister::GetXRegFromCode(i);
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
    in_used_[x.GetCode()] = in_used;
}

bool RegisterAllocator::InUsed(const Register &x) {
    return in_used_[x.GetCode()];
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

void BaseContext::Set(const Register &x, u64 value) {
    if (!register_alloc_.InUsed(x)) {
        __ Mov(x, value);
    } else {
        register_alloc_.MarkInUsed(x);
        auto tmp = register_alloc_.AcquireTempX();
        __ Mov(tmp, value);
        __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), 8 * x.GetCode()));
        register_alloc_.ReleaseTempX(tmp);
        register_alloc_.MarkInUsed(x, false);
    }
}

void BaseContext::Set(const Register &w, u32 value) {
    if (!register_alloc_.InUsed(w)) {
        __ Mov(w, value);
    } else {
        register_alloc_.MarkInUsed(w);
        auto tmp = register_alloc_.AcquireTempX();
        __ Mov(tmp.W(), value);
        __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), 8 * w.GetCode()));
        register_alloc_.ReleaseTempX(tmp);
        register_alloc_.MarkInUsed(w, false);
    }
}

void BaseContext::Push(const Register &reg) {
    if (reg.IsFPRegister()) {
        __ Str(reg, MemOperand(register_alloc_.ContextPtr(), 8 * reg.GetCode()));
    } else if (register_alloc_.InUsed(reg)) {
        return;
    } else if (reg.IsSP()) {
        auto &tmp = register_alloc_.AcquireTempX();
        __ Mov(tmp, reg);
        __ Str(reg, MemOperand(register_alloc_.ContextPtr(), 8 * reg.GetCode()));
        register_alloc_.ReleaseTempX(tmp);
    } else {
        register_alloc_.MarkInUsed(reg);
        __ Str(reg, MemOperand(register_alloc_.ContextPtr(), 16 * reg.GetCode()));
        register_alloc_.MarkInUsed(reg, false);
    }
}

void BaseContext::Pop(const Register &reg) {
    if (reg.IsFPRegister()) {
        __ Ldr(reg, MemOperand(register_alloc_.ContextPtr(), 8 * reg.GetCode()));
    } else if (register_alloc_.InUsed(reg)) {
        return;
    } else if (reg.IsSP()) {
        auto &tmp = register_alloc_.AcquireTempX();
        __ Ldr(tmp, MemOperand(register_alloc_.ContextPtr(), 8 * reg.GetCode()));
        __ Mov(reg, tmp);
        register_alloc_.ReleaseTempX(tmp);
    } else {
        __ Ldr(reg, MemOperand(register_alloc_.ContextPtr(),
                               OFFSET_CTX_A64_VEC_REG + 16 * reg.GetCode()));
    }
}

void BaseContext::MarkPC() {
    auto tmp = register_alloc_.AcquireTempX();
    __ Mov(tmp, PC());
    __ Str(tmp, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_PC));
    register_alloc_.ReleaseTempX(tmp);
}

void BaseContext::Push(const Register &reg1, const Register &reg2) {
    if ((reg2.GetCode() - reg1.GetCode() == 1)
        && (!reg1.IsSP() && !reg2.IsSP())
        && (!register_alloc_.InUsed(reg1) && !register_alloc_.InUsed(reg2))) {
        __ Stp(reg1, reg2, MemOperand(register_alloc_.ContextPtr(), 8 * reg1.GetCode()));
    } else {
        Push(reg1);
        Push(reg2);
    }
}

void BaseContext::Pop(const Register &reg1, const Register &reg2) {
    if ((reg2.GetCode() - reg1.GetCode() == 1)
        && (!reg1.IsSP() && !reg2.IsSP())
        && (!register_alloc_.InUsed(reg1) && !register_alloc_.InUsed(reg2))) {
        __ Ldp(reg1, reg2, MemOperand(register_alloc_.ContextPtr(), 8 * reg1.GetCode()));
    } else {
        Pop(reg1);
        Pop(reg2);
    }
}

void BaseContext::SaveContext() {
    auto reg_ctx = register_alloc_.ContextPtr();
    auto tmp = register_alloc_.AcquireTempX();
    // XRegs
    for (int i = 0; i < 30; i += 2) {
        Push(XRegister::GetXRegFromCode(i), XRegister::GetXRegFromCode(i + 1));
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
        __ Stp(VRegister::GetVRegFromCode(i), VRegister::GetVRegFromCode(i + 1),
               MemOperand(tmp, 16 * i));
    }
    register_alloc_.ReleaseTempX(tmp);
}

void BaseContext::LoadContext() {
    auto reg_ctx = register_alloc_.ContextPtr();
    auto tmp = register_alloc_.AcquireTempX();
    // XRegs
    for (int i = 0; i < 30; i += 2) {
        Push(XRegister::GetXRegFromCode(i), XRegister::GetXRegFromCode(i + 1));
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
        __ Stp(VRegister::GetVRegFromCode(i), VRegister::GetVRegFromCode(i + 1),
               MemOperand(tmp, 16 * i));
    }
}

void BaseContext::Forward(VAddr addr) {

}

void BaseContext::Forward(const Register &target) {

}

void BaseContext::AddTicks(u64 ticks) {

}

void BaseContext::Terminal(bool check_suspend) {
    MarkPC();
    AddTicks(current_block_ticks_);
}

VAddr BaseContext::PC() const {
    return reinterpret_cast<VAddr>(pc_);
}

MacroAssembler &BaseContext::Assembler() {
    return masm_;
}

RegisterAllocator &BaseContext::GetRegisterAlloc() {
    return register_alloc_;
}

LabelAllocator &BaseContext::GetLabelAlloc() {
    return label_allocator_;
}

void BaseContext::EndBlock() {
    __ Reset();
    register_alloc_.Reset();
    label_allocator_.Reset();
}

Instructions::A64::AArch64Inst *BaseContext::Instr() {
    return pc_;
}

const Instance &BaseContext::GetInstance() const {
    return *instance_;
}

void BaseContext::ToContextSwitch(const ContextSwitcher &switcher) {
    Terminal();
    auto reg_ctx = register_alloc_.ContextPtr();
    auto tmp = register_alloc_.AcquireTempX();
    __ Mov(tmp, switcher.reason);
    __ Str(tmp.W(), MemOperand(reg_ctx, OFFSET_OF(CPUContext, ctx_switch.reason)));
    __ Mov(tmp, switcher.data);
    __ Str(tmp, MemOperand(reg_ctx, OFFSET_OF(CPUContext, ctx_switch.data)));
    register_alloc_.ReleaseTempX(tmp);
    Push(reg_forward_);
    __ Mov(reg_forward_, instance_->GetGlobalStubs()->GetFullSwitchGuestToHost());
    __ Br(reg_forward_);
    EndBlock();
}

BaseContext::BaseContext(const SharedPtr<Instance> &instance) : instance_{instance}, reg_ctx_{
        XRegister::GetXRegFromCode(instance->GetContextConfig().context_reg)}, reg_forward_{
        XRegister::GetXRegFromCode(instance->GetContextConfig().forward_reg)} {

}

QuickContext::QuickContext(const SharedPtr<Instance> &instance) : BaseContext(instance) {

}

const Register &QuickContext::LoadContextPtr() {
    const auto &context_reg = reg_ctx_;
    __ Push(context_reg);
    __ Mrs(context_reg, TPIDR_EL0);
    __ Ldr(context_reg, MemOperand(context_reg, CTX_TLS_SLOT * 8));
    // save tmp0, tmp1
    __ Str(x16, MemOperand(context_reg, x16.GetCode() * 8));
    __ Pop(x16);
    __ Str(x16, MemOperand(context_reg, context_reg.GetCode() * 8));
    // restore tmp0
    __ Ldr(x16, MemOperand(context_reg, x16.GetCode() * 8));
    register_alloc_.MarkInUsed(context_reg);
    return context_reg;
}

void QuickContext::ClearContextPtr(const Register &context) {
    __ Ldr(context, MemOperand(context, 8 * context.GetCode()));
    register_alloc_.MarkInUsed(context, false);
}

#undef __

RegisterGuard::RegisterGuard(const ContextA64 &context, const Register &target) : context_(context),
                                                                                  target_(target) {
    auto &reg_allocator = context->GetRegisterAlloc();
    use_tmp = reg_allocator.InUsed(target);
    if (use_tmp) {
        tmp = reg_allocator.AcquireTempX();
        context_->Assembler().Ldr(tmp,
                                  MemOperand(reg_allocator.ContextPtr(), target_.GetCode() * 8));
    }
}

RegisterGuard::~RegisterGuard() {
    if (use_tmp) {
        auto &reg_allocator = context_->GetRegisterAlloc();
        // if dirty, need write back
        if (dirty) {
            context_->Assembler().Str(tmp, MemOperand(reg_allocator.ContextPtr(),
                                                      target_.GetCode() * 8));
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
