//
// Created by SwiftGan on 2020/8/20.
//

#pragma once

#include <aarch64/macro-assembler-aarch64.h>
#include <svm/arm64/svm_jit_context.h>
#include <stack>

#define VISITOR_LIST_THAT_INTEREST(V)     \
  V(CompareBranch)                      \
  V(ConditionalBranch)                  \
  V(Exception)                          \
  V(LoadLiteral)                        \
  V(LoadStorePairOffset)                \
  V(LoadStorePairPostIndex)             \
  V(LoadStorePairPreIndex)              \
  V(LoadStorePostIndex)                 \
  V(LoadStorePreIndex)                  \
  V(LoadStoreUnsignedOffset)            \
  V(PCRelAddressing)                    \
  V(System)                             \
  V(TestBranch)                         \
  V(UnconditionalBranch)                \
  V(UnconditionalBranchToRegister)

namespace Decode::A64 {

    using namespace vixl::aarch64;

    class VixlJitDecodeVisitor : public DecoderVisitor {
    public:
        VixlJitDecodeVisitor() = default;

        ~VixlJitDecodeVisitor() = default;

        // Declare all Visitor functions.
#define DECLARE(A) \
  virtual void Visit##A(const Instruction* instr) VIXL_OVERRIDE;
        VISITOR_LIST_THAT_INTEREST(DECLARE)
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

        void PushContext(Jit::A64::ContextA64 context);

        void PopContext();

        constexpr Jit::A64::ContextA64 Context() {
          return jit_contexts_.top();
        }

    private:

        std::stack<Jit::A64::ContextA64> jit_contexts_{};
    };

}
