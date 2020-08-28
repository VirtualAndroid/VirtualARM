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

void VixlJitDecodeVisitor::PushContext(Jit::A64::ContextA64 context) {
    jit_contexts_.push(context);
}

void VixlJitDecodeVisitor::PopContext() {
    jit_contexts_.pop();
}

void VixlJitDecodeVisitor::VisitCompareBranch(const Instruction *instr) {
    VAddr target = (VAddr) instr->GetImmPCOffsetTarget();
    switch (instr->Mask(CompareBranchMask)) {
        case CBZ_w:
            BranchImm<Comp | CompW>(Context(), target, static_cast<u8>(instr->GetRt()));
            break;
        case CBZ_x:
            BranchImm<Comp>(Context(), target, static_cast<u8>(instr->GetRt()));
            break;
        case CBNZ_w:
            BranchImm<Comp | CompW | Negate>(Context(), target, static_cast<u8>(instr->GetRt()));
            break;
        case CBNZ_x:
            BranchImm<Comp | Negate>(Context(), target, static_cast<u8>(instr->GetRt()));
            break;
        default:
            break;
    }
}

void VixlJitDecodeVisitor::VisitConditionalBranch(const Instruction *instr) {
    BranchImm(Context(), (u64) instr->GetImmPCOffsetTarget(), 0, instr->GetCondition());
}

void VixlJitDecodeVisitor::VisitTestBranch(const Instruction *instr) {
    switch (instr->Mask(TestBranchMask)) {
        case TBZ:
            BranchImm<TestBit>(Context(), reinterpret_cast<VAddr>(instr->GetImmPCOffsetTarget()),
                               instr->Rt());
            break;
        case TBNZ:
            BranchImm<TestBit | Negate>(Context(),
                                        reinterpret_cast<VAddr>(instr->GetImmPCOffsetTarget()),
                                        instr->Rt());
            break;
    }
}

void VixlJitDecodeVisitor::VisitUnconditionalBranch(const Instruction *instr) {
    switch (instr->Mask(UnconditionalBranchMask)) {
        case BL:
            BranchImm<Link>(Context(), reinterpret_cast<VAddr>(instr->GetImmPCOffsetTarget()));
        case B:
            BranchImm(Context(), reinterpret_cast<VAddr>(instr->GetImmPCOffsetTarget()));
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
        Context()->Interrupt({InterruptHelp::ErrorInstr, reinterpret_cast<VAddr>(instr)});
        return;
    }

    if (link) {
        BranchReg<Link>(Context(), static_cast<u8>(instr->GetRn()));
    } else {
        BranchReg(Context(), static_cast<u8>(instr->GetRn()));
    }

}

void VixlJitDecodeVisitor::VisitPCRelAddressing(const Instruction *instr) {
    if (instr->Mask(PCRelAddressingMask) == ADRP) {
        Addressing<true>(Context(), static_cast<u8>(instr->GetRd()), instr->GetImmPCRel());
    } else {
        assert(instr->Mask(PCRelAddressingMask) == ADR);
        Addressing(Context(), static_cast<u8>(instr->GetRd()), instr->GetImmPCRel());
    }
}

void VixlJitDecodeVisitor::VisitLoadLiteral(const Instruction *instr) {
    auto offset = instr->GetImmLLiteral() * kLiteralEntrySize;
    u8 rt = static_cast<u8>(instr->GetRt());
    switch (instr->Mask(LoadLiteralMask)) {
        // Use NoRegLog to suppress the register trace (LOG_REGS, LOG_VREGS), then
        // print a more detailed log.
        case LDR_w_lit:
            LoadLiteral<u32>(Context(), rt, offset);
            break;
        case LDR_x_lit:
            LoadLiteral<u64>(Context(), rt, offset);
            break;
        case LDR_s_lit:
            LoadLiteral<f32, Float>(Context(), rt, offset);
            break;
        case LDR_d_lit:
            LoadLiteral<f64, Float>(Context(), rt, offset);
            break;
        case LDR_q_lit:
            LoadLiteral<f128, Float>(Context(), rt, offset);
            break;
        case LDRSW_x_lit:
            LoadLiteral<u32, ImmSigned>(Context(), rt, offset);
            break;
        case PRFM_lit:
            LoadLiteral<u64, Prfm>(Context(), rt, offset);
            break;
        default:
            break;
    }
}

void VixlJitDecodeVisitor::VisitException(const Instruction *instr) {
    switch (instr->Mask(ExceptionMask)) {
        case BRK:
            Exception<Brk>(Context());
            break;
        case SVC:
            Exception<Svc>(Context(), static_cast<u16>(instr->GetImmException()));
            break;
        case HVC:
            Exception<Hvc>(Context(), static_cast<u16>(instr->GetImmException()));
            break;
        default:
            Context()->Interrupt({InterruptHelp::ErrorInstr, (u64) instr});
            break;
    }
}

void VixlJitDecodeVisitor::VisitSystem(const Instruction *instr) {
    if (instr->Mask(SystemSysRegFMask) == SystemSysRegFixed) {
        switch (instr->Mask(SystemSysRegMask)) {
            case MRS:
                ReadWriteSysRegister(Context(), static_cast<u8>(instr->GetRt()),
                                     static_cast<u16>(instr->GetImmSystemRegister()));
                return;
            case MSR:
                ReadWriteSysRegister<true>(Context(), static_cast<u8>(instr->GetRt()),
                                     static_cast<u16>(instr->GetImmSystemRegister()));
                return;
        }
    }
    Context()->Assembler().Emit(instr->GetInstructionBits());
}

bool LoadStorePairFloat(const Instruction *instr) {
    switch (instr->Mask(LoadStorePairMask)) {
        case LDP_x:
        case LDP_w:
        case STP_x:
        case STP_w:
            return false;
        default:
            return true;
    }
}

void VixlJitDecodeVisitor::VisitLoadStorePairOffset(const Instruction *instr) {
    if (LoadStorePairFloat(instr)) {
        LoadStorePair<Float>(Context(), instr->GetRt(), instr->GetRt2(), instr->GetRn());
    } else {
        LoadStorePair(Context(), instr->GetRt(), instr->GetRt2(), instr->GetRn());
    }
}

void VixlJitDecodeVisitor::VisitLoadStorePairPostIndex(const Instruction *instr) {
    if (LoadStorePairFloat(instr)) {
        LoadStorePair<Float | WriteBack | PostIndex>(Context(), instr->GetRt(), instr->GetRt2(), instr->GetRn());
    } else {
        LoadStorePair<WriteBack | PostIndex>(Context(), instr->GetRt(), instr->GetRt2(), instr->GetRn());
    }
}

void VixlJitDecodeVisitor::VisitLoadStorePairPreIndex(const Instruction *instr) {
    if (LoadStorePairFloat(instr)) {
        LoadStorePair<Float | WriteBack>(Context(), instr->GetRt(), instr->GetRt2(), instr->GetRn());
    } else {
        LoadStorePair<WriteBack>(Context(), instr->GetRt(), instr->GetRt2(), instr->GetRn());
    }
}

void VixlJitDecodeVisitor::VisitLoadStorePostIndex(const Instruction *instr) {
    LoadStoreReg<WriteBack | PostIndex>(Context(), instr->GetRt(), instr->GetRn());
}

void VixlJitDecodeVisitor::VisitLoadStorePreIndex(const Instruction *instr) {
    LoadStoreReg<WriteBack>(Context(), instr->GetRt(), instr->GetRn());
}

void VixlJitDecodeVisitor::VisitLoadStoreUnsignedOffset(const Instruction *instr) {
    LoadStoreReg(Context(), instr->GetRt(), instr->GetRn());
}

void VixlJitDecodeVisitor::VisitUnallocated(const Instruction *instr) {
    Context()->Assembler().Emit(instr->GetInstructionBits());
}

void VixlJitDecodeVisitor::VisitUnimplemented(const Instruction *instr) {
    Context()->Assembler().Emit(instr->GetInstructionBits());
}

void VixlJitDecodeVisitor::VisitReserved(const Instruction *instr) {
    Context()->Assembler().Emit(instr->GetInstructionBits());
}
