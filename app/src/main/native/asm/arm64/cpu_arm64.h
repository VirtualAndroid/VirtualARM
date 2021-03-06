//
// Created by 甘尧 on 2019-09-25.
//

#pragma once

#include "base/marcos.h"

namespace CPU::A64 {


    union HalfFloat {
        u16 bits;
        s16 bits_signed; // TODO: is this guaranteed to be two's complement?
        struct {
            u16 mantissa : 10;
            u16 exponent : 5;
            u16 sign : 1;
        };

        struct {
            u16 : 9;
            u16 is_quiet : 1;
            u16 : 6;
        };

        static const u64 max_exp = 0x1F;
    };

    union SingleFloat {
        u32 bits;
        s32 bits_signed; // TODO: is this guaranteed to be two's complement?
        float val;
        struct {
            u32 mantissa : 23;
            u32 exponent : 8;
            u32 sign : 1;
        };

        struct {
            u32 : 22;
            u32 is_quiet : 1;
            u32 : 9;
        };

        static const u64 max_exp = 0xFF;
    };

    union Double {
        u64 bits;
        s64 bits_signed; // TODO: is this guaranteed to be two's complement?
        double val;
        struct {
            u64 mantissa : 52;
            u64 exponent : 11;
            u64 sign : 1;
        };

        struct {
            u64 : 51;
            u64 is_quiet : 1;
            u64 : 12;
        };


        static const u64 maxExp = 0x7FF;
    };

    typedef union {
        u128 V;
        u64 Q;
        u64 D[2];
        u32 S[4];
        u16 H[8];
        u8 B[16];

        HalfFloat float_h;
        SingleFloat float_s;
        Double float_d;
    } Vec;

    typedef union {
        u64 X;
        struct {
            u64 W : 32;
            u64 R : 32;
        };
    } Reg;

    struct PSTATE {
        u32 NZCV;
        int SS;
        int IL;
        int nRW;
        int EL;
        int SP;
        int D;
        int A, I, F;
    };

    struct VirtualTLB {
        VAddr vaddr;
        VAddr target;
    };

    struct InterruptHelp {
        enum Reason : u64 {
            Svc,
            Hvc,
            Brk,
            Hlt,
            ErrorInstr,
            PageFatal
        };
        Reason reason;
        union {
            u64 data;
            u16 exception;
            u32 error_instr;
            u64 fatal_addr;
        };
    };

    struct ABICallHelp {
        enum Reason : u64 {
            IC_IVAU,
            GetTicks
        };
        Reason reason;
        union {
            u64 data;
            u64 ticks;
            u64 ivau_xt;
        };
    };

    struct TLBEntry {
        VAddr page_index;
        VAddr pte;
    };

    constexpr size_t l1_page_bits = 6;
    constexpr size_t l1_page_count = 1 << l1_page_bits;

    struct CPUContext {
        Reg cpu_registers[29];
        Reg fp; // x29
        u64 lr; // x30
        u64 sp;
        u64 pc;
        PSTATE pstate;
        Vec vector_registers[32];
        u32 fpcr;
        u32 fpsr;
        u64 tpidr;
        u64 tpidrro;
        u64 cntfreq;
        // dispatcher
        VAddr pre_pc;
        VAddr forward;
        VAddr code_cache;
        VAddr dispatcher_table;
        // memory
        VAddr tlb;
        VAddr page_table;
        // host stubs
        VAddr host_stubs;
        union {
            // interrupt
            InterruptHelp interrupt;
            ABICallHelp abi_call;
        };
        VAddr interrupt_sp;
        VAddr host_sp;
        // flags
        u64 suspend_flag;
        // ticks
        u64 ticks_now;
        u64 ticks_max;
        // help fields
        VAddr context_ptr;
        // L1 Data Cache, for sp
        // exchange when context switch
        std::array<TLBEntry, l1_page_count> l1_dcache;
    };
}

extern "C" const VAddr OFFSET_CTX_A64_CPU_REG;
extern "C" const VAddr OFFSET_CTX_A64_SP;
extern "C" const VAddr OFFSET_CTX_A64_PC;
extern "C" const VAddr OFFSET_CTX_A64_LR;
extern "C" const VAddr OFFSET_CTX_A64_PSTATE;
extern "C" const VAddr OFFSET_CTX_A64_VEC_REG;
extern "C" const VAddr OFFSET_CTX_A64_FPCR;
extern "C" const VAddr OFFSET_CTX_A64_FPSR;
extern "C" const VAddr OFFSET_CTX_A64_TPIDR;
extern "C" const VAddr OFFSET_CTX_A64_TPIDRRO;
extern "C" const VAddr OFFSET_CTX_A64_FORWARD;
extern "C" const VAddr OFFSET_CTX_A64_CODE_CACHE;
extern "C" const VAddr OFFSET_CTX_A64_QUERY_PAGE;
extern "C" const VAddr OFFSET_CTX_A64_TLB;
extern "C" const VAddr OFFSET_CTX_A64_PAGE_TABLE;
extern "C" const VAddr OFFSET_CTX_A64_SUSPEND_ADDR;
extern "C" const VAddr OFFSET_CTX_A64_DISPATCHER_TABLE;
extern "C" const VAddr OFFSET_CTX_A64_HOST_SP;
extern "C" const VAddr OFFSET_CTX_A64_INTERRUPT_SP;
extern "C" const VAddr OFFSET_CTX_A64_TMP_PC;

