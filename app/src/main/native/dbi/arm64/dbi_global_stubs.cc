//
// Created by SwiftGan on 2020/8/19.
//

#include <asm/arm64/cpu_arm64.h>
#include <platform/memory.h>
#include <base/hash_table.h>
#include "dbi_global_stubs.h"
#include "dbi_arm64.h"
#include "block/code_find_table.h"

#define __ masm_.

using namespace DBI::A64;
using namespace CPU::A64;

constexpr static size_t stub_memory_size = PAGE_SIZE;

CPUContext *GlobalStubs::FullSwitchStub(CPUContext *context) {
    return context;
}


CPUContext *GlobalStubs::CodeCacheMissStub(CPUContext *context) {
    return context;
}

GlobalStubs::GlobalStubs(SharedPtr<Instance> instance) : instance_{instance}, context_reg_{
        XRegister::GetXRegFromCode(instance->GetContextConfig().context_reg)}, forward_reg_{
        XRegister::GetXRegFromCode(instance->GetContextConfig().forward_reg)} {
    load_context_ = instance->GetContextConfig().mode == QuickMode;
    // allocate code memory
    code_memory_ = reinterpret_cast<VAddr>(Platform::MapExecutableMemory(stub_memory_size));
    full_switch_host_to_guest_ = code_memory_;
    full_switch_guest_to_host_ = code_memory_ + 512;
    abi_host_to_guest_ = code_memory_ + 512 * 2;
    abi_switch_guest_to_host_ = code_memory_ + 512 * 3;
    forward_code_cache_lookup_ = code_memory_ + 512 * 4;
    // do builds
    BuildFullSwitchGuestToHost();
    BuildFullSwitchHostToGuest();
    BuildForwardCodeCacheLookup();
}

GlobalStubs::~GlobalStubs() {
    Platform::UnMapExecutableMemory(code_memory_, stub_memory_size);
}

VAddr GlobalStubs::GetFullSwitchHostToGuest() const {
    return full_switch_host_to_guest_;
}

VAddr GlobalStubs::GetFullSwitchGuestToHost() const {
    return full_switch_guest_to_host_;
}

VAddr GlobalStubs::GetAbiHostToGuest() const {
    return abi_host_to_guest_;
}

VAddr GlobalStubs::GetAbiSwitchGuestToHost() const {
    return abi_switch_guest_to_host_;
}

VAddr GlobalStubs::GetForwardCodeCacheLookup() const {
    return forward_code_cache_lookup_;
}

// must save lr & forward_reg_ before call
void GlobalStubs::BuildFullSwitchGuestToHost() {
    MacroAssembler masm_;
    auto tmp = forward_reg_;
    // restore forward reg first
    __ Ldr(forward_reg_, MemOperand(context_reg_, forward_reg_.GetCode() * 8));

    FullSaveContext(masm_, tmp);

    // prepare Host Sp
    __ Ldr(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_HOST_SP));
    __ Mov(sp, tmp);

    __ Mov(x0, context_reg_);
    __ Mov(forward_reg_, reinterpret_cast<VAddr>(FullSwitchStub));
    __ Blr(forward_reg_);

    // If returned, direct to kernel_to_guest_trampoline
    __ Mov(context_reg_, x0);
    __ Mov(forward_reg_, GetFullSwitchGuestToHost());
    __ Br(forward_reg_);

    auto stub_size = __ GetBuffer()->GetSizeInBytes();
    VAddr buffer_start = full_switch_guest_to_host_;
    VAddr tmp_code_start = __ GetBuffer()->GetStartAddress<VAddr>();
    __ FinalizeCode();
    std::memcpy(reinterpret_cast<void *>(buffer_start),
                reinterpret_cast<const void *>(tmp_code_start), stub_size);
    __builtin___clear_cache(reinterpret_cast<char *>(buffer_start),
                            reinterpret_cast<char *>(buffer_start + stub_size));
}

void GlobalStubs::BuildFullSwitchHostToGuest() {
    MacroAssembler masm_;
    auto tmp = forward_reg_;
    Label code_lookup_label;

    // load context reg
    __ Mov(context_reg_, x0);

    FullRestoreContext(masm_, tmp);

    // try load code cache first
    __ Ldr(forward_reg_, MemOperand(context_reg_, OFFSET_CTX_A64_FORWARD));
    __ Cbnz(forward_reg_, &code_lookup_label);
    // clear
    __ Str(xzr, MemOperand(context_reg_, OFFSET_CTX_A64_FORWARD));
    __ Br(forward_reg_);
    __ Bind(&code_lookup_label);
    // to code cache lookup stub
    __ Mov(forward_reg_, forward_code_cache_lookup_);
    __ Br(forward_reg_);

    auto stub_size = __ GetBuffer()->GetSizeInBytes();
    VAddr buffer_start = full_switch_host_to_guest_;
    VAddr tmp_code_start = __ GetBuffer()->GetStartAddress<VAddr>();
    __ FinalizeCode();
    std::memcpy(reinterpret_cast<void *>(buffer_start),
                reinterpret_cast<const void *>(tmp_code_start), stub_size);
    __builtin___clear_cache(reinterpret_cast<char *>(buffer_start),
                            reinterpret_cast<char *>(buffer_start + stub_size));
}

void GlobalStubs::BuildForwardCodeCacheLookup() {
    MacroAssembler masm_;
    auto tmp1 = x0;
    auto tmp2 = x1;
    auto rt = forward_reg_;
    Label miss_target;
    Label label_loop;
    Label label_end;
    // save tmp1, tmp2;
    __ Stp(tmp1, tmp2, MemOperand(context_reg_, tmp1.GetCode() * 8));
    // Find hash table
    __ Ldr(tmp1, MemOperand(context_reg_, OFFSET_CTX_A64_DISPATCHER_TABLE));
    __ Lsr(tmp2, rt, CODE_CACHE_HASH_BITS);
    __ Bfc(tmp2, instance_->GetCodeFindTable()->TableBits(),
           sizeof(VAddr) * 8 - instance_->GetCodeFindTable()->TableBits());
    __ Ldr(tmp1, MemOperand(tmp1, tmp2, LSL, 3));
    __ Cbz(tmp1, &miss_target);
    __ And(tmp2, rt, (CODE_CACHE_HASH_SIZE - CODE_CACHE_HASH_OVERP) << 2);
    // 2 + 2 = 4 = 16字节 = Entry 大小
    __ Add(tmp1, tmp1, Operand(tmp2, LSL, 2));
    __ Bind(&label_loop);
    __ Ldr(tmp2, MemOperand(tmp1, 16, PostIndex));
    __ Cbz(tmp2, &miss_target);
    __ Sub(tmp2, tmp2, rt);
    __ Cbnz(tmp2, &label_loop);
    // find target
    __ Ldr(forward_reg_, MemOperand(tmp1, -8, PreIndex));
    // restore tmp1, tmp2;
    __ Ldp(tmp1, tmp2, MemOperand(context_reg_, tmp1.GetCode() * 8));
    __ Br(forward_reg_);
    // can not find from table
    __ B(&miss_target);
    __ Ldp(tmp1, tmp2, MemOperand(context_reg_, tmp1.GetCode() * 8));
    __ Ldr(forward_reg_, MemOperand(context_reg_, forward_reg_.GetCode() * 8));
    FullSaveContext(masm_, tmp1);
    __ Mov(forward_reg_, (VAddr)CodeCacheMissStub);
    __ Mov(x0, context_reg_);
    __ Blr(forward_reg_);
    __ Mov(context_reg_, x0);
    FullRestoreContext(masm_, const_cast<Register &>(forward_reg_));
    __ Ldr(forward_reg_, MemOperand(context_reg_, OFFSET_CTX_A64_FORWARD));
    __ Br(forward_reg_);

    auto stub_size = __ GetBuffer()->GetSizeInBytes();
    VAddr buffer_start = forward_code_cache_lookup_;
    VAddr tmp_code_start = __ GetBuffer()->GetStartAddress<VAddr>();
    __ FinalizeCode();
    std::memcpy(reinterpret_cast<void *>(buffer_start),
                reinterpret_cast<const void *>(tmp_code_start), stub_size);
    __builtin___clear_cache(reinterpret_cast<char *>(buffer_start),
                            reinterpret_cast<char *>(buffer_start + stub_size));
}

void GlobalStubs::FullSaveContext(MacroAssembler &masm_, Register &tmp) {
    // x regs
    for (int i = 0; i < 30; i += 2) {
        if (i == context_reg_.GetCode()) {
            __ Str(XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * (i + 1)));
        } else if (i + 1 == context_reg_.GetCode()) {
            __ Str(XRegister::GetXRegFromCode(i),
                   MemOperand(context_reg_, 8 * i));
        } else {
            __ Stp(XRegister::GetXRegFromCode(i), XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * i));
        }
    }
    // save lr
    if (context_reg_.GetCode() != lr.GetCode()) {
        __ Str(x30, MemOperand(context_reg_, OFFSET_CTX_A64_LR));
    }
    // sysregs
    __ Mrs(tmp.W(), NZCV);
    __ Str(tmp.W(), MemOperand(context_reg_, OFFSET_CTX_A64_PSTATE));
    __ Mrs(tmp.W(), FPCR);
    __ Str(tmp.W(), MemOperand(context_reg_, OFFSET_CTX_A64_FPCR));
    __ Mrs(tmp.W(), FPSR);
    __ Str(tmp.W(), MemOperand(context_reg_, OFFSET_CTX_A64_FPSR));
    // sp
    __ Mov(tmp, sp);
    __ Str(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_SP));
    // v regs
    __ Add(tmp, context_reg_, OFFSET_CTX_A64_VEC_REG);
    for (int i = 0; i < 32; i += 2) {
        __ Stp(VRegister::GetVRegFromCode(i), VRegister::GetVRegFromCode(i + 1),
               MemOperand(tmp, 16 * i));
    }
}

void GlobalStubs::FullRestoreContext(MacroAssembler &masm_, Register &tmp) {
    for (int i = 0; i < 30; i += 2) {
        if (i == context_reg_.GetCode()) {
            __ Ldr(XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * (i + 1)));
        } else if (i + 1 == context_reg_.GetCode()) {
            __ Ldr(XRegister::GetXRegFromCode(i),
                   MemOperand(context_reg_, 8 * i));
        } else {
            __ Ldp(XRegister::GetXRegFromCode(i), XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * i));
        }
    }
    // Sysregs
    __ Ldr(tmp.W(), MemOperand(context_reg_, OFFSET_CTX_A64_PSTATE));
    __ Msr(NZCV, tmp.W());
    __ Ldr(tmp.W(), MemOperand(context_reg_, OFFSET_CTX_A64_FPCR));
    __ Msr(FPCR, tmp.W());
    __ Ldr(tmp.W(), MemOperand(context_reg_, OFFSET_CTX_A64_FPSR));
    __ Msr(FPSR, tmp.W());
    // Save Host SP
    __ Mov(tmp, sp);
    __ Str(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_HOST_SP));
    // Sp
    __ Ldr(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_SP));
    __ Mov(sp, tmp);
    if (context_reg_.GetCode() != x30.GetCode()) {
        // origin lr
        __ Ldr(x30, MemOperand(context_reg_, OFFSET_CTX_A64_LR));
    }
    // VRegs
    __ Add(tmp, context_reg_, OFFSET_CTX_A64_VEC_REG);
    for (int i = 0; i < 32; i += 2) {
        __ Ldp(VRegister::GetVRegFromCode(i), VRegister::GetVRegFromCode(i + 1),
               MemOperand(tmp, 16 * i));
    }
}
