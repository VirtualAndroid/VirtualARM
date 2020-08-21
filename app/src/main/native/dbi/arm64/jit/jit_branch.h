//
// Created by SwiftGan on 2020/8/20.
//

#pragma once

#include "asm/arm64/instruction_table.h"
#include "asm/arm64/visitors/deocde_a64.h"
#include "dbi/arm64/context_new.h"


using namespace Decode::A64;
using namespace DBI::A64;

namespace Jit::A64 {
#define __ masm_.
    template <unsigned flags = 0>
    void BranchImm(ContextA64 context, s64 offset, u8 rt, int condition = -1) {
        auto &masm_ = context->Assembler();

        VAddr target = context->PC() + offset;

        context->Terminal(true);

        if (flags & Link) {
            context->Set(lr, context->PC() + 4);
        }

        Label *branch = context->GetLabelAlloc().AllocLabel();

        if constexpr (flags & Comp) {
            {
                RegisterGuard guard(context, XRegister::GetXRegFromCode(rt));
                if (flags & Negate) {
                    __ Cbnz(guard.Target(), branch);
                } else {
                    __ Cbz(guard.Target(), branch);
                }
            }
            context->Forward(context->PC() + 4);
        } else if constexpr (flags & BrunchFlags::TestBit) {
            {
                RegisterGuard guard(context, XRegister::GetXRegFromCode(rt));
                auto bit = context->Instr()->b40 | (context->Instr()->b5 << 5);
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
        context->EndBlock();
    }

    template <unsigned flags = 0>
    void BranchReg(ContextA64 context, u8 reg_target) {
        auto &masm_ = context->Assembler();

        context->Terminal(true);

        if constexpr (flags & Link) {
            context->Set(lr, context->PC() + 4);
        }

        auto &reg_allocator = context->GetRegisterAlloc();
        auto target = XRegister::GetXRegFromCode(reg_target);

        if (reg_allocator.InUsed(target)) {
            auto &tmp = reg_allocator.AcquireTempX();
            __ Mov(tmp, target);
            context->Forward(tmp);
        } else {
            context->Push(target);
            context->Forward(target);
        }

        context->EndBlock();
    }
#undef __
}

