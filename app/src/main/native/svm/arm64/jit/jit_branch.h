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
    void BranchImm(ContextA64 context, u64 target, u8 rt = 0, int condition = -1) {
        auto &masm_ = context->Assembler();

        context->Terminal();

        if (flags & Link) {
            context->MarkReturn();
        }

        Label *branch = context->GetLabelAlloc().AllocLabel();

        if constexpr (flags & Comp) {
            {
                RegisterGuard guard(context, context->GetXRegister(rt));
                auto reg_rt = (flags & CompW) ? guard.Target().W() : guard.Target();
                if (flags & Negate) {
                    __ Cbnz(reg_rt, branch);
                } else {
                    __ Cbz(reg_rt, branch);
                }
            }
            context->Forward(context->PC() + 4);
        } else if constexpr (flags & BrunchFlags::TestBit) {
            {
                RegisterGuard guard(context, context->GetXRegister(rt));
                auto bit = context->Instr().b40 | (context->Instr().b5 << 5);
                if (flags & Negate) {
                    __ Tbnz(guard.Target(), bit, branch);
                } else {
                    __ Tbz(guard.Target(), bit, branch);
                }
            }
            context->Forward(context->PC() + 4);
        } else if (condition >= 0) {
            __ B(branch, Condition(condition));
            context->Forward(context->PC() + 4);
        }

        __ Bind(branch);
        context->Forward(target);
    }

    template <unsigned flags = 0>
    void BranchReg(ContextA64 context, u8 reg_target) {
        auto &masm_ = context->Assembler();

        context->Terminal();

        if constexpr (flags & Link) {
            context->MarkReturn();
        }

        auto target = context->GetXRegister(reg_target);

        context->Forward(target);
    }
#undef __
}

