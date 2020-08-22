//
// Created by SwiftGan on 2020/8/22.
//

#pragma once

#pragma once

#include <base/marcos.h>
#include <aarch64/macro-assembler-aarch64.h>

using namespace vixl::aarch64;

namespace CPU::A64 {
    struct CPUContext;
}

namespace DBI::A64 {

    class Instance;

    class BlockStubs : public BaseObject {
    public:
        BlockStubs(const SharedPtr<Instance> &instance);

    private:
        void BuildPageLookup();

        SharedPtr <Instance> instance_;
        const Register &context_reg_;
        const Register &forward_reg_;
    };


}
