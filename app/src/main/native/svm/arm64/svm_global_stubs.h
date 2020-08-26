//
// Created by SwiftGan on 2020/8/19.
//

#pragma once

#include <base/marcos.h>
#include <aarch64/macro-assembler-aarch64.h>

namespace CPU::A64 {
    struct CPUContext;
}

namespace SVM::A64 {

    using namespace vixl::aarch64;

    class Instance;

    class GlobalStubs : public BaseObject, NonCopyable {
    public:
        GlobalStubs(SharedPtr<Instance> instance);

        virtual ~GlobalStubs();

        VAddr GetFullInterrupt() const;
        VAddr GetForwardCodeCache() const;
        VAddr GetHostToGuest() const;
        VAddr GetReturnToHost() const;
        VAddr GetAbiInterrupt() const;

        static const u32 FullInterruptOffset();
        static const u32 ForwardCodeCacheOffset();
        static const u32 ReturnToHostOffset();
        static const u32 ABIInterruptOffset();

        void RunCode(CPU::A64::CPUContext *context);

    private:

        void FullSaveGuestContext(MacroAssembler &masm_, Register& tmp);
        void FullRestoreGuestContext(MacroAssembler &masm_, Register& tmp);
        void ABISaveGuestContext(MacroAssembler &masm_, Register& tmp);
        void ABIRestoreGuestContext(MacroAssembler &masm_, Register& tmp);
        void SaveHostContext(MacroAssembler &masm_);
        void RestoreHostContext(MacroAssembler &masm_);

        void BuildFullInterruptStub();
        void BuildABIInterruptStub();
        void BuildHostToGuestStub();
        void BuildReturnToHostStub();
        void BuildForwardCodeCache();

        static CPU::A64::CPUContext *InterruptStub(CPU::A64::CPUContext *context);
        static CPU::A64::CPUContext *ABIStub(CPU::A64::CPUContext *context);
        static CPU::A64::CPUContext *JitCacheMissStub(CPU::A64::CPUContext *context);

        CPU::A64::CPUContext *(*host_to_guest_)(CPU::A64::CPUContext *);
        SharedPtr<Instance> instance_;
        const Register &context_reg_;
        const Register &forward_reg_;
        VAddr code_memory_;
        VAddr return_to_host_;
        VAddr full_interrupt_;
        VAddr abi_interrupt_;
        VAddr forward_code_cache_;
    };

}
