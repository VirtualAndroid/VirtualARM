//
// Created by swift on 2020/3/1.
//

#include "svm_mmu.h"

using namespace SVM::A64;

A64MMU::A64MMU(u8 page_bits, u8 addr_with, bool tlb_per_thread) : MMU(
        page_bits, addr_with, tlb_per_thread) {}

VAddr A64MMU::GetPageStart(PTE &pte) {
    return pte.index_ << page_bits_;
}

bool A64MMU::PageReadable(PTE &pte) {
    return static_cast<bool>(pte.attrs_ & PageAttrs::Read);
}

bool A64MMU::PageWritable(PTE &pte) {
    return static_cast<bool>(pte.attrs_ & PageAttrs::Write);
}

void A64MMU::HostReadCallback(VAddr host_addr, std::size_t size) {
}

void A64MMU::HostWriteCallback(VAddr host_addr, std::size_t size) {
}

void A64MMU::InvalidRead(VAddr vaddr, std::size_t size) {
}

void A64MMU::InvalidWrite(VAddr vaddr, std::size_t size) {
    abort();
}
