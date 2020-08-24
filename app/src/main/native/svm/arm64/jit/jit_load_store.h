//
// Created by SwiftGan on 2020/8/20.
//

#pragma once

#include "asm/arm64/instruction_table.h"
#include "asm/arm64/visitors/deocde_a64.h"
#include "svm/arm64/svm_jit_context.h"


using namespace Decode::A64;
using namespace SVM::A64;

namespace Jit::A64 {
#define __ masm_.

    template <bool align_page = false>
    void Addressing(ContextA64 context, u8 rd, s64 offset) {

        auto base = context->PC();

        if constexpr (align_page) {
            offset <<= PAGE_BITS;
            base = RoundDown(base, PAGE_SIZE);
        }

        auto target =  base + offset;

        RegisterGuard guard(context, context->GetXRegister(rd));
        context->Set(guard.Target(), target);
        guard.Dirty();
    }

    template<typename T, unsigned flags = 0>
    void LoadLiteral(ContextA64 context, u8 rt, u32 offset) {
        auto &masm_ = context->Assembler();
        auto &reg_alloc = context->GetRegisterAlloc();
        VAddr addr = context->PC() + offset;

        auto tmp = reg_alloc.AcquireTempX();

        __ Mov(tmp, addr);
        context->LookupPageTable(tmp, addr);

        if constexpr (flags & Float) {
            auto rt_v = context->GetVRegister(rt);
            if constexpr (sizeof(T) == 16) {
                __ Ldr(rt_v.Q(), MemOperand(tmp));
            } else if constexpr (sizeof(T) == 8) {
                __ Ldr(rt_v.D(), MemOperand(tmp));
            } else if constexpr (sizeof(T) == 4) {
                __ Ldr(rt_v.S(), MemOperand(tmp));
            } else {
                __ Emit(context->Instr().raw);
            }
        } else if constexpr (flags & Prfm) {
            __ Prfm(PrefetchOperation(context->Instr().Rt), MemOperand(tmp));
        } else if constexpr (flags & LoadSigned) {
            RegisterGuard guard(context, context->GetXRegister(rt));
            guard.Dirty();
            __ Ldrsw(guard.Target(), MemOperand(tmp));
        } else {
            RegisterGuard guard(context, context->GetXRegister(rt));
            guard.Dirty();
            if constexpr (sizeof(T) == 8) {
                __ Ldr(guard.Target(), MemOperand(tmp));
            } else if constexpr (sizeof(T) == 4) {
                __ Ldr(guard.Target().W(), MemOperand(tmp));
            } else {
                __ Emit(context->Instr().raw);
            }
        }

        reg_alloc.ReleaseTempX(tmp);
    }

    template<typename T, unsigned flags = 0>
    void LoadStoreReg(ContextA64 context, u8 rt, u8 rn) {

    }

    template<typename T, unsigned flags = 0>
    void LoadStorePair(ContextA64 context, u8 rt, u8 rt2, u8 rn) {

    }

#undef __
}

