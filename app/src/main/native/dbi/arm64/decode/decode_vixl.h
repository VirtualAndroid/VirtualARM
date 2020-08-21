//
// Created by SwiftGan on 2020/8/20.
//

#pragma once

#include <aarch64/macro-assembler-aarch64.h>
#include <dbi/arm64/context_new.h>

namespace Decode::A64 {

    using namespace vixl::aarch64;

    class VixlJitDecodeVisitor : public vixl::aarch64::DecoderVisitor {

        // Declare all Visitor functions.
#define DECLARE(A) \
  virtual void Visit##A(const Instruction* instr) VIXL_OVERRIDE;
        VISITOR_LIST_THAT_RETURN(DECLARE)
#undef DECLARE


#define DECLARE(A) \
  VIXL_NO_RETURN virtual void Visit##A(const Instruction* instr) VIXL_OVERRIDE;
        VISITOR_LIST_THAT_DONT_RETURN(DECLARE)
#undef DECLARE


#define DECLARE(A)                                                             \
  VIXL_NO_RETURN_IN_DEBUG_MODE virtual void Visit##A(const Instruction* instr) \
      VIXL_OVERRIDE;
        VISITOR_LIST_THAT_DONT_RETURN_IN_DEBUG_MODE(DECLARE)
#undef DECLARE

    private:
        DBI::A64::ContextA64 context_;
    };

}
