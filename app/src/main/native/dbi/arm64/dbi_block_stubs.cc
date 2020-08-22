//
// Created by SwiftGan on 2020/8/22.
//

#include "dbi_block_stubs.h"
#include "dbi_arm64.h"

#define __ masm_.

using namespace CPU::A64;
using namespace DBI::A64;

BlockStubs::BlockStubs(const SharedPtr<Instance> &instance) : instance_{instance}, context_reg_{
XRegister::GetXRegFromCode(instance->GetContextConfig().context_reg)}, forward_reg_{
XRegister::GetXRegFromCode(instance->GetContextConfig().forward_reg)} {
    BuildPageLookup();

}

void BlockStubs::BuildPageLookup() {
    MacroAssembler masm_;

    if (context_reg_.RealCode() == lr.RealCode()) {
    }

}
