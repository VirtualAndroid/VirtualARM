//
// Created by SwiftGan on 2020/6/17.
//

#include "context_new.h"

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
            return XRegister::GetXRegFromCode(i);
        }
    }
}

const Register &RegisterAllocator::ReleaseTempX(const Register &x) {
    return x0;
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
        __ Ldr(reg, MemOperand(register_alloc_.ContextPtr(), OFFSET_CTX_A64_VEC_REG + 16 * reg.GetCode()));
    }
}

void BaseContext::MarkPC(u64 value) {
    auto tmp = register_alloc_.AcquireTempX();
    __ Mov(tmp, value);
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
    // Protect Pc
    // Pc could be changed by host
    // dispatch if changed
    __ Ldr(tmp, MemOperand(reg_ctx, OFFSET_CTX_A64_PC));
    __ Str(tmp, MemOperand(reg_ctx, OFFSET_CTX_A64_TMP_PC));
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
    // Protect Pc
    // Pc could be changed by host
    // dispatch if changed
    __ Ldr(tmp, MemOperand(reg_ctx, OFFSET_CTX_A64_PC));
    __ Str(tmp, MemOperand(reg_ctx, OFFSET_CTX_A64_TMP_PC));
    // VRegs
    __ Add(tmp, reg_ctx, OFFSET_CTX_A64_VEC_REG);
    for (int i = 0; i < 32; i += 2) {
        __ Stp(VRegister::GetVRegFromCode(i), VRegister::GetVRegFromCode(i + 1),
               MemOperand(tmp, 16 * i));
    }
}

const Register &QuickContext::LoadContextPtr() {
    const auto &context_reg = x17;
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