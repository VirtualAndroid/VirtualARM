cmake_minimum_required(VERSION 3.4.1)
project(VirtualARM)

set(CMAKE_CXX11_EXTENSION_COMPILE_OPTION "-std=c++17")
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wno-narrowing")
ENABLE_LANGUAGE(ASM)

include_directories(.)

include_directories(externals/ext-boost)

add_subdirectory(externals/vixl)
include_directories(externals/vixl)

add_library(virtual_arm SHARED
        virtual_arm.cc
        base/marcos.cc
        base/hash_table.cc
        base/log.cc
        instruction/instruction.cc
        asm/arm64/instruction_fields.cc
        asm/arm64/instruction_aarch64.cc
        asm/arm64/instruction_table.cc
        asm/arm64/instruction_decode.cc
        asm/arm64/decodes/decode_system_branch.cc
        asm/arm64/decodes/decode_data_process_imm.cc
        asm/arm64/decodes/decode_load_and_store.cc
        asm/arm64/cpu_arm64.cc
        asm/arm64/visitors/decode_visitors.cc
        frontend/ir/opcode_ir.cc
        frontend/ir/argument_ir.cc
        frontend/ir/instruction_ir.cc
        frontend/ir/block_ir.cc
        frontend/ir/assembler_ir.cc
        frontend/arm64/ir_for_a64.cc
        asm/arm64/assembler_a64.cc
        backend/arm64/trampoline_a64.cc
        backend/arm64/trampoline.S
        backend/ir/interpreter_ir.cc
        svm/arm64/svm_arm64.cc
#        svm/arm64/dbi_jit_arm64.cc
#        svm/arm64/dbi_context_arm64.cc
        svm/arm64/svm_mmu.cc
#        svm/arm64/dbi_trampolines_arm64.cc
        svm/arm64/svm_diskcache_loader.cc
        svm/arm64/svm_jit_context.cc
        svm/arm64/svm_global_stubs.cc
        svm/arm64/decode/decode_vixl.cc
        svm/arm64/svm_block_stubs.cc
        svm/arm64/svm_thread.cc
        svm/arm64/svm_jit_manager.cc
        block/host_code_block.cc
        block/code_find_table.cc
        block/code_set.cc
        block/code_block.cc
        memory/mmu.cc
        memory/tlb.cc
        platform/memory.cc
)


target_link_libraries(virtual_arm android log)
target_link_libraries(virtual_arm vixl)