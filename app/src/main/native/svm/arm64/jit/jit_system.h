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

    template <unsigned flags = 0>
    void Exception(ContextA64 context, u16 imm = 0) {
        if constexpr (flags & Svc) {
            context->Interrupt({InterruptHelp::Svc, imm});
        } else if constexpr (flags & Hvc) {
            context->Interrupt({InterruptHelp::Hvc, imm});
        } else if constexpr (flags & Brk) {
            context->Interrupt({InterruptHelp::Brk, 0});
        } else {
            context->Interrupt({InterruptHelp::ErrorInstr, context->PC()});
        }
    }

    template <bool write = false>
    void ReadWriteSysRegister(ContextA64 context, u8 rt, u16 system_reg) {

        auto &masm_ = context->Assembler();

        RegisterGuard guard(context, context->GetXRegister(rt));
        if constexpr (write) {
            guard.Dirty();
        }
        s64 context_offset;
        switch (system_reg) {
            case TPIDR_EL0:
                context_offset = OFFSET_CTX_A64_TPIDR;
                break;
            case TPIDRRO_EL0:
                context_offset = OFFSET_CTX_A64_TPIDRRO;
                break;
            case CNTFREQ_EL0:
                context_offset = OFFSET_OF(CPUContext, cntfreq);
                break;
            case CNTPCT_EL0:
                context_offset = OFFSET_OF(CPUContext, ticks_now);
                break;
            default:
                context_offset = -1;
                break;
        }

        if (context_offset < 0) {
            auto instr = context->Instr();
            instr.Rt = guard.Target().RealCode();
            __ Emit(instr.raw);
            return;
        }

        auto ctx_reg = context->GetRegisterAlloc().ContextPtr();

        if constexpr (write) {
            __ Str(guard.Target(), MemOperand(ctx_reg, context_offset));
        } else {
            __ Ldr(guard.Target(), MemOperand(ctx_reg, context_offset));
        }
    }

#undef __
}

