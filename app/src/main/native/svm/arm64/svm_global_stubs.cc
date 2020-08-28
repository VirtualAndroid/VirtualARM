//
// Created by SwiftGan on 2020/8/19.
//

#include <asm/arm64/cpu_arm64.h>
#include <platform/memory.h>
#include <base/hash_table.h>
#include <base/log.h>
#include "svm_global_stubs.h"
#include "svm_arm64.h"
#include "block/code_find_table.h"
#include "svm_thread.h"

#define __ masm_.

using namespace SVM::A64;
using namespace CPU::A64;

constexpr static size_t stub_memory_size = PAGE_SIZE;

CPUContext *GlobalStubs::InterruptStub(CPUContext *context) {
    auto thread_ctx = reinterpret_cast<EmuThreadContext *>(context->context_ptr);
    thread_ctx->Interrupt(context->interrupt);
    thread_ctx->LookupJitCache();
    if (!context->code_cache) {
        thread_ctx->Fallback();
    }
    return context;
}

CPUContext *GlobalStubs::JitCacheMissStub(CPUContext *context) {
    auto thread_ctx = reinterpret_cast<EmuThreadContext *>(context->context_ptr);
    thread_ctx->LookupJitCache();
    if (!context->code_cache) {
        thread_ctx->Fallback();
    }
    return context;
}

CPU::A64::CPUContext *GlobalStubs::ABIStub(CPU::A64::CPUContext *context) {

    return context;
}

GlobalStubs::GlobalStubs(SharedPtr<Instance> instance) : instance_{instance}, context_reg_{
        XRegister::GetXRegFromCode(instance->GetJitConfig().context_reg)}, forward_reg_{
        XRegister::GetXRegFromCode(instance->GetJitConfig().forward_reg)} {
    // allocate code memory
    code_memory_ = reinterpret_cast<VAddr>(Platform::MapExecutableMemory(stub_memory_size));
    host_to_guest_ = reinterpret_cast<CPUContext *(*)(CPUContext *)>(code_memory_);
    return_to_host_ = code_memory_ + 512;
    full_interrupt_ = code_memory_ + 512 * 2;
    abi_interrupt_ = code_memory_ + 512 * 3;
    forward_code_cache_ = code_memory_ + 512 * 4;
    // do builds
    BuildABIInterruptStub();
    BuildFullInterruptStub();
    BuildReturnToHostStub();
    BuildHostToGuestStub();
    BuildForwardCodeCache();
}

GlobalStubs::~GlobalStubs() {
    Platform::UnMapExecutableMemory(code_memory_, stub_memory_size);
}

void GlobalStubs::RunCode(CPUContext *context) {
    // check may context error
    assert(host_to_guest_(context) == context);
}

VAddr GlobalStubs::GetFullInterrupt() const {
    return full_interrupt_;
}

VAddr GlobalStubs::GetForwardCodeCache() const {
    return forward_code_cache_;
}

VAddr GlobalStubs::GetHostToGuest() const {
    return reinterpret_cast<VAddr>(host_to_guest_);
}

VAddr GlobalStubs::GetReturnToHost() const {
    return return_to_host_;
}

VAddr GlobalStubs::GetAbiInterrupt() const {
    return abi_interrupt_;
}

void GlobalStubs::BuildForwardCodeCache() {
    MacroAssembler masm_;
    auto dispatcher = instance_->GetCodeFindTable();
    auto tmp1 = x0;
    auto tmp2 = x1;
    auto rt = forward_reg_;
    Label miss_target;
    Label label_loop;
    Label label_end;
    // save tmp1, tmp2;
    __ Stp(tmp1, tmp2, MemOperand(context_reg_, tmp1.RealCode() * 8));
    // load rt
    __ Ldr(rt, MemOperand(context_reg_, OFFSET_CTX_A64_PC));
    // load hash table
    __ Ldr(tmp1, MemOperand(context_reg_, OFFSET_CTX_A64_DISPATCHER_TABLE));
    __ Lsr(tmp2, rt, dispatcher->align_bits_ + CODE_CACHE_HASH_BITS + dispatcher->redun_bits);
    __ Bfc(tmp2, dispatcher->TableBits(),
           sizeof(VAddr) * 8 -  dispatcher->TableBits());

    __ Ldr(tmp1, MemOperand(tmp1, tmp2, LSL, 3));
    __ Cbz(tmp1, &miss_target);
    __ Mov(tmp2, CODE_CACHE_HASH_SIZE << 2);
    __ And(tmp2, rt, tmp2);
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
    __ Ldp(tmp1, tmp2, MemOperand(context_reg_, tmp1.RealCode() * 8));
    __ Br(forward_reg_);
    // can not find from table
    __ Bind(&miss_target);
    __ Ldp(tmp1, tmp2, MemOperand(context_reg_, tmp1.RealCode() * 8));
    __ Ldr(forward_reg_, MemOperand(context_reg_, forward_reg_.RealCode() * 8));
    ABISaveGuestContext(masm_, tmp1);
    __ Mov(forward_reg_, (VAddr) JitCacheMissStub);
    __ Mov(x0, context_reg_);
    __ Blr(forward_reg_);
    __ Mov(context_reg_, x0);
    ABIRestoreGuestContext(masm_, const_cast<Register &>(forward_reg_));
    __ Ldr(forward_reg_, MemOperand(context_reg_, OFFSET_CTX_A64_CODE_CACHE));
    __ Br(forward_reg_);

    __ FinalizeCode();

    auto stub_size = __ GetBuffer()->GetSizeInBytes();
    VAddr buffer_start = forward_code_cache_;
    VAddr tmp_code_start = __ GetBuffer()->GetStartAddress<VAddr>();
    std::memcpy(reinterpret_cast<void *>(buffer_start),
                reinterpret_cast<const void *>(tmp_code_start), stub_size);
    __builtin___clear_cache(reinterpret_cast<char *>(buffer_start),
                            reinterpret_cast<char *>(buffer_start + stub_size));
}

void GlobalStubs::FullSaveGuestContext(MacroAssembler &masm_, Register &tmp) {
    //restore tmp
    __ Ldr(tmp, MemOperand(context_reg_, tmp.RealCode() * 8));
    // x regs
    for (int i = 0; i < 30; i += 2) {
        if (i == context_reg_.RealCode()) {
            __ Str(XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * (i + 1)));
        } else if (i + 1 == context_reg_.RealCode()) {
            __ Str(XRegister::GetXRegFromCode(i),
                   MemOperand(context_reg_, 8 * i));
        } else {
            __ Stp(XRegister::GetXRegFromCode(i), XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * i));
        }
    }
    // save lr
    if (context_reg_ == lr) {
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

void GlobalStubs::FullRestoreGuestContext(MacroAssembler &masm_, Register &tmp) {
    for (int i = 0; i < 30; i += 2) {
        if (i == context_reg_.RealCode()) {
            __ Ldr(XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * (i + 1)));
        } else if (i + 1 == context_reg_.RealCode()) {
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
    // Sp
    __ Ldr(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_SP));
    __ Mov(sp, tmp);
    if (context_reg_.RealCode() != x30.RealCode()) {
        // origin lr
        __ Ldr(x30, MemOperand(context_reg_, OFFSET_CTX_A64_LR));
    }
    // VRegs
    __ Add(tmp, context_reg_, OFFSET_CTX_A64_VEC_REG);
    for (int i = 0; i < 32; i += 2) {
        __ Ldp(VRegister::GetVRegFromCode(i), VRegister::GetVRegFromCode(i + 1),
               MemOperand(tmp, 16 * i));
    }
    //restore tmp
    __ Ldr(tmp, MemOperand(context_reg_, tmp.RealCode() * 8));
}

void GlobalStubs::ABISaveGuestContext(MacroAssembler &masm_, Register &tmp) {
    //restore tmp
    __ Ldr(tmp, MemOperand(context_reg_, tmp.RealCode() * 8));
    // x regs
    for (int i = 0; i < 19; i += 2) {
        if (i == context_reg_.RealCode()) {
            __ Str(XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * (i + 1)));
        } else if (i + 1 == context_reg_.RealCode()) {
            __ Str(XRegister::GetXRegFromCode(i),
                   MemOperand(context_reg_, 8 * i));
        } else {
            __ Stp(XRegister::GetXRegFromCode(i), XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * i));
        }
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
    for (int i = 0; i < 7; i += 2) {
        __ Stp(VRegister::GetVRegFromCode(i), VRegister::GetVRegFromCode(i + 1),
               MemOperand(tmp, 16 * i));
    }
}

void GlobalStubs::ABIRestoreGuestContext(MacroAssembler &masm_, Register &tmp) {
    for (int i = 0; i < 19; i += 2) {
        if (i == context_reg_.RealCode()) {
            __ Ldr(XRegister::GetXRegFromCode(i + 1),
                   MemOperand(context_reg_, 8 * (i + 1)));
        } else if (i + 1 == context_reg_.RealCode()) {
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
    // Sp
    __ Ldr(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_SP));
    __ Mov(sp, tmp);
    // VRegs
    __ Add(tmp, context_reg_, OFFSET_CTX_A64_VEC_REG);
    for (int i = 0; i < 7; i += 2) {
        __ Ldp(VRegister::GetVRegFromCode(i), VRegister::GetVRegFromCode(i + 1),
               MemOperand(tmp, 16 * i));
    }
    //restore tmp
    __ Ldr(tmp, MemOperand(context_reg_, tmp.RealCode() * 8));
}

void GlobalStubs::SaveHostContext(MacroAssembler &masm_) {
    __ Push(q7, q6, q5, q4);
    __ Push(q3, q2, q1, q0);

    __ Push(x30, x29, x28, x27);
    __ Push(x26, x25, x24, x23);
    __ Push(x22, x21, x20, x19);
    __ Push(x18, x17, x16, x15);
    __ Push(x14, x13, x12, x11);
    __ Push(x10, x9, x8, x7);
    __ Push(x6, x5, x4, x3);
    __ Push(x2, x1);
    __ Push(x0);

    //status regs
    __ Mrs(x1, NZCV);
    __ Mrs(x2, FPSR);
    __ Mrs(x3, FPCR);
    __ Push(x4, x3, x2, x1);
}

void GlobalStubs::RestoreHostContext(MacroAssembler &masm_) {
    //status regs
    __ Pop(x1, x2, x3, x4);
    __ Msr(NZCV, x1);
    __ Msr(FPSR, x2);
    __ Msr(FPCR, x3);

    __ Pop(x0);
    __ Pop(x1, x2);
    __ Pop(x3, x4, x5, x6);
    __ Pop(x7, x8, x9, x10);
    __ Pop(x11, x12, x13, x14);
    __ Pop(x15, x16, x17, x18);
    __ Pop(x19, x20, x21, x22);
    __ Pop(x23, x24, x25, x26);
    __ Pop(x27, x28, x29, x30);

    __ Pop(q0, q1, q2, q3);
    __ Pop(q4, q5, q6, q7);
}

void GlobalStubs::BuildFullInterruptStub() {
    MacroAssembler masm_;
    Label code_lookup_label;
    auto tmp = forward_reg_;
    // restore forward reg first
    __ Ldr(forward_reg_, MemOperand(context_reg_, forward_reg_.RealCode() * 8));

    FullSaveGuestContext(masm_, tmp);

    // prepare interrupt sp
    __ Ldr(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_INTERRUPT_SP));
    __ Mov(sp, tmp);

    __ Mov(x0, context_reg_);
    __ Mov(forward_reg_, reinterpret_cast<VAddr>(InterruptStub));
    __ Blr(forward_reg_);

    // If returned, direct to kernel_to_guest_trampoline
    __ Mov(context_reg_, x0);

    FullRestoreGuestContext(masm_, tmp);

    // try load code cache first
    __ Ldr(forward_reg_, MemOperand(context_reg_, OFFSET_CTX_A64_CODE_CACHE));
    __ Cbz(forward_reg_, &code_lookup_label);
    // clear
    __ Str(xzr, MemOperand(context_reg_, OFFSET_CTX_A64_CODE_CACHE));
    __ Br(forward_reg_);
    __ Bind(&code_lookup_label);
    // to code cache lookup stub
    __ Mov(forward_reg_, forward_code_cache_);
    __ Br(forward_reg_);

    __ FinalizeCode();

    auto stub_size = __ GetBuffer()->GetSizeInBytes();
    VAddr buffer_start = full_interrupt_;
    VAddr tmp_code_start = __ GetBuffer()->GetStartAddress<VAddr>();
    std::memcpy(reinterpret_cast<void *>(buffer_start),
                reinterpret_cast<const void *>(tmp_code_start), stub_size);
    __builtin___clear_cache(reinterpret_cast<char *>(buffer_start),
                            reinterpret_cast<char *>(buffer_start + stub_size));
}

void GlobalStubs::BuildABIInterruptStub() {
    MacroAssembler masm_;
    Label code_lookup_label;
    auto tmp = forward_reg_;
    // restore forward reg first
    __ Ldr(forward_reg_, MemOperand(context_reg_, forward_reg_.RealCode() * 8));

    ABISaveGuestContext(masm_, tmp);

    // prepare interrupt sp
    __ Ldr(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_INTERRUPT_SP));
    __ Mov(sp, tmp);

    __ Mov(x0, context_reg_);
    __ Mov(forward_reg_, reinterpret_cast<VAddr>(ABIStub));
    __ Blr(forward_reg_);

    // If returned, direct to kernel_to_guest_trampoline
    __ Mov(context_reg_, x0);

    ABIRestoreGuestContext(masm_, tmp);

    // try load code cache first
    __ Ldr(forward_reg_, MemOperand(context_reg_, OFFSET_CTX_A64_CODE_CACHE));
    __ Cbz(forward_reg_, &code_lookup_label);
    // clear
    __ Str(xzr, MemOperand(context_reg_, OFFSET_CTX_A64_CODE_CACHE));
    __ Br(forward_reg_);
    __ Bind(&code_lookup_label);
    // to code cache lookup stub
    __ Mov(forward_reg_, forward_code_cache_);
    __ Br(forward_reg_);

    __ FinalizeCode();

    auto stub_size = __ GetBuffer()->GetSizeInBytes();
    VAddr buffer_start = abi_interrupt_;
    VAddr tmp_code_start = __ GetBuffer()->GetStartAddress<VAddr>();
    std::memcpy(reinterpret_cast<void *>(buffer_start),
                reinterpret_cast<const void *>(tmp_code_start), stub_size);
    __builtin___clear_cache(reinterpret_cast<char *>(buffer_start),
                            reinterpret_cast<char *>(buffer_start + stub_size));
}

void GlobalStubs::BuildHostToGuestStub() {
    MacroAssembler masm_;
    auto tmp = x0;
    Label code_lookup_label;

    SaveHostContext(masm_);

    // load context ptr
    __ Mov(context_reg_, x0);

    // save host sp
    __ Mov(tmp, sp);
    __ Str(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_HOST_SP));

    // load guest context
    FullRestoreGuestContext(masm_, tmp);

    // try load code cache first
    __ Ldr(forward_reg_, MemOperand(context_reg_, OFFSET_CTX_A64_CODE_CACHE));
    __ Cbz(forward_reg_, &code_lookup_label);
    // clear
    __ Str(xzr, MemOperand(context_reg_, OFFSET_CTX_A64_CODE_CACHE));
    __ Br(forward_reg_);
    __ Bind(&code_lookup_label);
    // to code cache lookup stub
    __ Mov(forward_reg_, forward_code_cache_);
    __ Br(forward_reg_);

    __ FinalizeCode();

    auto stub_size = __ GetBuffer()->GetSizeInBytes();
    VAddr buffer_start = reinterpret_cast<VAddr>(host_to_guest_);
    VAddr tmp_code_start = __ GetBuffer()->GetStartAddress<VAddr>();
    std::memcpy(reinterpret_cast<void *>(buffer_start),
                reinterpret_cast<const void *>(tmp_code_start), stub_size);
    __builtin___clear_cache(reinterpret_cast<char *>(buffer_start),
                            reinterpret_cast<char *>(buffer_start + stub_size));
}

void GlobalStubs::BuildReturnToHostStub() {
    MacroAssembler masm_;
    auto tmp = forward_reg_;
    // restore forward reg first
    __ Ldr(forward_reg_, MemOperand(context_reg_, forward_reg_.RealCode() * 8));

    // save guest context
    FullSaveGuestContext(masm_, tmp);

    // load host sp
    __ Ldr(tmp, MemOperand(context_reg_, OFFSET_CTX_A64_HOST_SP));
    __ Mov(sp, tmp);

    // restore host context
    RestoreHostContext(masm_);

    // ret lr
    __ Ret();

    __ FinalizeCode();

    auto stub_size = __ GetBuffer()->GetSizeInBytes();
    VAddr buffer_start = return_to_host_;
    VAddr tmp_code_start = __ GetBuffer()->GetStartAddress<VAddr>();
    std::memcpy(reinterpret_cast<void *>(buffer_start),
                reinterpret_cast<const void *>(tmp_code_start), stub_size);
    __builtin___clear_cache(reinterpret_cast<char *>(buffer_start),
                            reinterpret_cast<char *>(buffer_start + stub_size));
}

const u32 GlobalStubs::FullInterruptOffset() {
    return OFFSET_OF(GlobalStubs, full_interrupt_);
}

const u32 GlobalStubs::ForwardCodeCacheOffset() {
    return OFFSET_OF(GlobalStubs, forward_code_cache_);
}

const u32 GlobalStubs::ReturnToHostOffset() {
    return OFFSET_OF(GlobalStubs, return_to_host_);
}

const u32 GlobalStubs::ABIInterruptOffset() {
    return OFFSET_OF(GlobalStubs, abi_interrupt_);
}
