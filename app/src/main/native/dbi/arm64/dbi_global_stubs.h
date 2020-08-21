//
// Created by SwiftGan on 2020/8/19.
//

#pragma once

#include <base/marcos.h>
#include <aarch64/macro-assembler-aarch64.h>

using namespace vixl::aarch64;

namespace CPU::A64 {
    struct CPUContext;
}

namespace DBI::A64 {

    class Instance;

    class GlobalStubs : public BaseObject {
    public:
        GlobalStubs(SharedPtr<Instance> instance);

        virtual ~GlobalStubs();

        VAddr GetFullSwitchHostToGuest() const;

        VAddr GetFullSwitchGuestToHost() const;

        VAddr GetAbiHostToGuest() const;

        VAddr GetAbiSwitchGuestToHost() const;

        VAddr GetForwardCodeCacheLookup() const;

    private:

        void FullSaveContext(MacroAssembler &masm_, Register& tmp);
        void FullRestoreContext(MacroAssembler &masm_, Register& tmp);

        void BuildFullSwitchGuestToHost();
        void BuildFullSwitchHostToGuest();
        void BuildForwardCodeCacheLookup();

        static CPU::A64::CPUContext *FullSwitchStub(CPU::A64::CPUContext *context);
        static CPU::A64::CPUContext *CodeCacheMissStub(CPU::A64::CPUContext *context);

        VAddr full_switch_host_to_guest_;
        VAddr full_switch_guest_to_host_;
        VAddr abi_host_to_guest_;
        VAddr abi_switch_guest_to_host_;
        VAddr forward_code_cache_lookup_;
        SharedPtr<Instance> instance_;
        const Register &context_reg_;
        const Register &forward_reg_;
        VAddr code_memory_;
        bool load_context_;
    };

}
