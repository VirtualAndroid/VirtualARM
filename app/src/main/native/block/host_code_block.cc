//
// Created by swift on 2020/3/3.
//

#include <sys/mman.h>
#include "host_code_block.h"
#include "platform/memory.h"
#include <aarch64/macro-assembler-aarch64.h>

using namespace vixl::aarch64;
using namespace Jit;

#define __ masm_.

bool BaseBlock::SaveToDisk(std::string path) {
    return false;
}

Buffer *BaseBlock::AllocCodeBuffer(VAddr source) {
    LockGuard lck(lock_);
    if (Full()) {
        return nullptr;
    }
    Buffer &buffer = buffers_[current_buffer_id_];
    buffer.id_ = current_buffer_id_;
    buffer.source_ = source;
    buffer.version_ = 1;
    current_buffer_id_++;
    return &buffer;
}

void BaseBlock::FlushCodeBuffer(Buffer *buffer, u32 size) {
    assert(size <= (UINT16_MAX << 2));
    buffer->size_ = static_cast<u16>(size >> 2);
    LockGuard lck(lock_);
    buffer->offset_ = current_offset_ += buffer->size_;
}

VAddr BaseBlock::GetBufferStart(Buffer *buffer) {
    return start_ + (buffer->offset_ << 2);
}

VAddr BaseBlock::GetBufferStart(u16 id) {
    return GetBufferStart(GetBuffer(id));
}

VAddr BaseBlock::GetBufferEnd(Buffer *buffer) {
    return GetBufferStart(buffer) + buffer->size_ << 2;
}

BaseBlock::BaseBlock(VAddr start, VAddr size) : start_(start), size_(size) {}

Buffer *BaseBlock::GetBuffer(u16 id) {
    return &buffers_[id];
}

void BaseBlock::Align(u32 size) {
    LockGuard lck(lock_);
    current_offset_ = RoundUp(current_offset_, size);
}

u16 BaseBlock::GetCurrentId() const {
    return current_buffer_id_;
}

std::mutex &BaseBlock::Lock() {
    return lock_;
}

VAddr BaseBlock::Base() {
    return start_;
}

bool BaseBlock::Full() {
    bool max_id = current_buffer_id_ >= UINT16_MAX - 1;
    bool max_buffer = (current_offset_ << 2) > size_ - PAGE_SIZE;
    return max_id || max_buffer;
}

A64::CodeBlock::CodeBlock(u32 block_size) : BaseBlock(
        reinterpret_cast<VAddr>(Platform::MapExecutableMemory(
                std::min(BLOCK_SIZE_A64_MAX, block_size))),
        std::min(BLOCK_SIZE_A64_MAX, block_size)) {
    // init block base
    assert(Base() % PAGE_SIZE == 0);
    module_base_ = reinterpret_cast<VAddr *>(Base());
    // init dispatcher table
    buffer_count_ = std::min<u32>(block_size >> 8, UINT16_MAX);
    buffers_.resize(buffer_count_);
    dispatchers_ = reinterpret_cast<Dispatcher *>(start_ + sizeof(VAddr));
    current_offset_ = (sizeof(Dispatcher) * buffer_count_ + sizeof(VAddr)) >> 2;
}

A64::CodeBlock::~CodeBlock() {
    Platform::UnMapExecutableMemory(start_, size_);
}

void A64::CodeBlock::GenDispatcher(Buffer *buffer) {
    assert(buffer->id_ < buffer_count_);
    auto &dispatcher = dispatchers_[buffer->id_].go_forward_;
    auto delta = GetBufferStart(buffer) - reinterpret_cast<VAddr>(&dispatcher);
    bool need_flush = dispatcher != 0;

    // B offset
    dispatcher = 0x14000000 | (0x03ffffff & (static_cast<u32>(delta) >> 2));

    if (need_flush) {
        ClearCachePlatform(reinterpret_cast<VAddr>(&dispatcher), 4);
    }
}

VAddr A64::CodeBlock::GetDispatcherAddr(Buffer *buffer) {
    return reinterpret_cast<VAddr>(&dispatchers_[buffer->id_].go_forward_);
}

VAddr A64::CodeBlock::GetDispatcherOffset(Buffer *buffer) {
    return start_ - GetDispatcherAddr(buffer);
}

void A64::CodeBlock::FlushCodeBuffer(Buffer *buffer, u32 size) {
    BaseBlock::FlushCodeBuffer(buffer, size);
    GenDispatcher(buffer);
}

VAddr A64::CodeBlock::ModuleMapAddressAddress() {
    return Base();
}

void A64::CodeBlock::SetModuleMapAddress(VAddr addr) {
    *module_base_ = addr;
}

void A64::CodeBlock::GenDispatcherStub(u8 forward_reg, VAddr dispatcher_trampoline) {
    assert(current_buffer_id_ == 0);
    MacroAssembler masm_;

    auto forward = XRegister::GetXRegFromCode(forward_reg);
    __ Mov(forward, dispatcher_trampoline);
    __ Br(forward);

    __ FinalizeCode();

    u32 stub_size = static_cast<u32>(__ GetBuffer()->GetSizeInBytes());

    auto buffer = AllocCodeBuffer(dispatcher_trampoline);
    FlushCodeBuffer(buffer, stub_size);

    std::memcpy(reinterpret_cast<void *>(GetBufferStart(buffer)),
                __ GetBuffer()->GetStartAddress<void *>(), stub_size);

    for (int i = 1; i < buffer_count_; ++i) {
        auto &dispatcher = dispatchers_[i].go_forward_;
        auto delta = GetBufferStart(buffer) - reinterpret_cast<VAddr>(&dispatcher);
        // B offset
        dispatcher = 0x14000000 | (0x03ffffff & (static_cast<u32>(delta) >> 2));
    }
}
