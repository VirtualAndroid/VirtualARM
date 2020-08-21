//
// Created by SwiftGan on 2020/8/20.
//

#include "decode_vixl.h"
#include "dbi/arm64/jit/jit_branch.h"
#include "dbi/arm64/jit/jit_load_store.h"
#include "dbi/arm64/jit/jit_system.h"

using namespace Decode::A64;
using namespace vixl::aarch64;
using namespace Jit::A64;

void VixlJitDecodeVisitor::VisitCompareBranch(const Instruction *instr) {
}

void VixlJitDecodeVisitor::VisitConditionalBranch(const vixl::aarch64::Instruction *instr) {

}

void VixlJitDecodeVisitor::VisitTestBranch(const vixl::aarch64::Instruction *instr) {

}

void VixlJitDecodeVisitor::VisitUnconditionalBranch(const vixl::aarch64::Instruction *instr) {
//    BranchImm()
}

void VixlJitDecodeVisitor::VisitUnconditionalBranchToRegister(
        const vixl::aarch64::Instruction *instr) {

}