//
// Created by SwiftGan on 2020/8/20.
//

#include "decode_vixl.h"
#include "svm/arm64/jit/jit_branch.h"
#include "svm/arm64/jit/jit_load_store.h"
#include "svm/arm64/jit/jit_system.h"

using namespace Decode::A64;
using namespace vixl::aarch64;
using namespace Jit::A64;

VixlJitDecodeVisitor::VixlJitDecodeVisitor(ContextA64 &context) : DecoderVisitor{}, context_(context) {}


void VixlJitDecodeVisitor::VisitCompareBranch(const Instruction *instr) {
    VAddr target = (VAddr) instr->GetImmPCOffsetTarget();
    switch (instr->Mask(CompareBranchMask)) {
        case CBZ_w:
            BranchImm<Comp | CompW>(context_, target, static_cast<u8>(instr->GetRt()));
            break;
        case CBZ_x:
            BranchImm<Comp>(context_, target, static_cast<u8>(instr->GetRt()));
            break;
        case CBNZ_w:
            BranchImm<Comp | CompW | Negate>(context_, target, static_cast<u8>(instr->GetRt()));
            break;
        case CBNZ_x:
            BranchImm<Comp | Negate>(context_, target, static_cast<u8>(instr->GetRt()));
            break;
        default:
            break;
    }
}

void VixlJitDecodeVisitor::VisitConditionalBranch(const Instruction *instr) {
    BranchImm(context_, (u64) instr->GetImmPCOffsetTarget(), 0, instr->GetCondition());
}

void VixlJitDecodeVisitor::VisitTestBranch(const Instruction *instr) {
    switch (instr->Mask(TestBranchMask)) {
        case TBZ:
            BranchImm<TestBit>(context_, reinterpret_cast<VAddr>(instr->GetImmPCOffsetTarget()),
                               instr->Rt());
            break;
        case TBNZ:
            BranchImm<TestBit | Negate>(context_,
                                        reinterpret_cast<VAddr>(instr->GetImmPCOffsetTarget()),
                                        instr->Rt());
            break;
    }
}

void VixlJitDecodeVisitor::VisitUnconditionalBranch(const Instruction *instr) {
    switch (instr->Mask(UnconditionalBranchMask)) {
        case BL:
            BranchImm<Link>(context_, reinterpret_cast<VAddr>(instr->GetImmPCOffsetTarget()));
        case B:
            BranchImm(context_, reinterpret_cast<VAddr>(instr->GetImmPCOffsetTarget()));
            break;
        default:
            abort();
    }
}

void VixlJitDecodeVisitor::VisitUnconditionalBranchToRegister(
        const Instruction *instr) {

    bool authenticate = false;
    bool link = false;

    switch (instr->Mask(UnconditionalBranchToRegisterMask)) {
        case BLR:
            link = true;
            break;
        case BR:
        case RET:
            break;
        default:
            authenticate = true;
            break;
    }

    // Do not support now, go interpreter
    if (authenticate) {
        context_->Interrupt({InterruptHelp::ErrorInstr, reinterpret_cast<VAddr>(instr)});
        return;
    }

    if (link) {
        BranchReg<Link>(context_, static_cast<u8>(instr->GetRn()));
    } else {
        BranchReg(context_, static_cast<u8>(instr->GetRn()));
    }

}

void VixlJitDecodeVisitor::VisitPCRelAddressing(const Instruction *instr) {
    if (instr->Mask(PCRelAddressingMask) == ADRP) {
        Addressing<true>(context_, static_cast<u8>(instr->GetRd()), instr->GetImmPCRel());
    } else {
        assert(instr->Mask(PCRelAddressingMask) == ADR);
        Addressing(context_, static_cast<u8>(instr->GetRd()), instr->GetImmPCRel());
    }
}

void VixlJitDecodeVisitor::VisitLoadLiteral(const Instruction *instr) {
    auto offset = instr->GetImmLLiteral() * kLiteralEntrySize;
    u8 rt = static_cast<u8>(instr->GetRt());
    switch (instr->Mask(LoadLiteralMask)) {
        // Use NoRegLog to suppress the register trace (LOG_REGS, LOG_VREGS), then
        // print a more detailed log.
        case LDR_w_lit:
            LoadLiteral<u32>(context_, rt, offset);
            break;
        case LDR_x_lit:
            LoadLiteral<u64>(context_, rt, offset);
            break;
        case LDR_s_lit:
            LoadLiteral<f32, Float>(context_, rt, offset);
            break;
        case LDR_d_lit:
            LoadLiteral<f64, Float>(context_, rt, offset);
            break;
        case LDR_q_lit:
            LoadLiteral<f128, Float>(context_, rt, offset);
            break;
        case LDRSW_x_lit:
            LoadLiteral<u32, ImmSigned>(context_, rt, offset);
            break;
        case PRFM_lit:
            LoadLiteral<u64, Prfm>(context_, rt, offset);
            break;
        default:
            break;
    }
}

void VixlJitDecodeVisitor::VisitException(const Instruction *instr) {
    switch (instr->Mask(ExceptionMask)) {
        case BRK:
            Exception<Brk>(context_);
            break;
        case SVC:
            Exception<Svc>(context_, static_cast<u16>(instr->GetImmException()));
            break;
        case HVC:
            Exception<Hvc>(context_, static_cast<u16>(instr->GetImmException()));
            break;
        default:
            context_->Interrupt({InterruptHelp::ErrorInstr, (u64) instr});
            break;
    }
}

void VixlJitDecodeVisitor::VisitSystem(const Instruction *instr) {
    if (instr->Mask(SystemSysRegFMask) == SystemSysRegFixed) {
        switch (instr->Mask(SystemSysRegMask)) {
            case MRS:
                ReadWriteSysRegister(context_, static_cast<u8>(instr->GetRt()),
                                     static_cast<u16>(instr->GetImmSystemRegister()));
                return;
            case MSR:
                ReadWriteSysRegister<true>(context_, static_cast<u8>(instr->GetRt()),
                                     static_cast<u16>(instr->GetImmSystemRegister()));
                return;
        }
    }
    context_->Assembler().Emit(instr->GetInstructionBits());
}

void VixlJitDecodeVisitor::VisitLoadStorePairOffset(const Instruction *instr) {
    VisitUnimplemented(instr);
}

void VixlJitDecodeVisitor::VisitLoadStorePairPostIndex(const Instruction *instr) {
    VisitUnimplemented(instr);
}

void VixlJitDecodeVisitor::VisitLoadStorePairPreIndex(const Instruction *instr) {
    VisitUnimplemented(instr);
}

void VixlJitDecodeVisitor::VisitLoadStorePostIndex(const Instruction *instr) {
    VisitUnimplemented(instr);
}

void VixlJitDecodeVisitor::VisitLoadStorePreIndex(const Instruction *instr) {
    VisitUnimplemented(instr);
}

void VixlJitDecodeVisitor::VisitUnallocated(const Instruction *instr) {
    context_->Assembler().Emit(instr->GetInstructionBits());
}

void VixlJitDecodeVisitor::VisitUnimplemented(const Instruction *instr) {
    context_->Assembler().Emit(instr->GetInstructionBits());
}

void VixlJitDecodeVisitor::VisitReserved(const Instruction *instr) {
    context_->Assembler().Emit(instr->GetInstructionBits());
}

VixlJitDecodeVisitor::~VixlJitDecodeVisitor() {

}
