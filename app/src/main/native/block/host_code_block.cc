//
// Created by swift on 2020/3/3.
//

#include <sys/mman.h>
#include "host_code_block.h"
#include "platform/memory.h"
#include <aarch64/macro-assembler-aarch64.h>
#include <base/log.h>

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
    buffer->offset_ = current_offset_;
    current_offset_ += AlignUp(buffer->size_, 2);
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
    current_offset_ = AlignUp(current_offset_, size);
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
                std::min(BLOCK_SIZE_A64_MAX, AlignUp(block_size, PAGE_SIZE)))),
        std::min(BLOCK_SIZE_A64_MAX, AlignUp(block_size, PAGE_SIZE))) {
    // init block base
    assert(Base() % PAGE_SIZE == 0);
    module_base_ = reinterpret_cast<VAddr *>(Base());
    // init dispatcher table
    buffer_count_ = std::min<u32>(block_size >> 8, UINT16_MAX);
    buffers_.resize(buffer_count_);
    dispatchers_ = reinterpret_cast<Dispatcher *>(start_ + sizeof(VAddr));
    current_offset_ = AlignUp((sizeof(Dispatcher) * buffer_count_ + sizeof(VAddr)) >> 2, 2);
}

A64::CodeBlock::~CodeBlock() {
    Platform::UnMapExecutableMemory(start_, size_);
}

void A64::CodeBlock::GenDispatcher(Buffer *buffer) {
    LockGuard guard(lock_);
    if (buffer->id_ >= buffer_count_) {
        LOGE("ID overflow: id: %d， max: %d", buffer->id_, buffer_count_);
    }
    assert(buffer->id_ < buffer_count_);
    auto &dispatcher = dispatchers_[buffer->id_].go_forward_;
    auto delta = GetBufferStart(buffer) - reinterpret_cast<VAddr>(&dispatcher);

    // B offset
    dispatcher = 0x14000000 | (0x03ffffff & (static_cast<u32>(delta) >> 2));

    ClearCachePlatform(reinterpret_cast<VAddr>(&dispatcher), 4);
}

VAddr A64::CodeBlock::GetDispatcherAddr(Buffer *buffer) {
    return reinterpret_cast<VAddr>(&dispatchers_[buffer->id_].go_forward_);
}

VAddr A64::CodeBlock::GetDispatcherOffset(Buffer *buffer) {
    return start_ - GetDispatcherAddr(buffer);
}

void A64::CodeBlock::FlushCodeBuffer(Buffer *buffer, u32 size) {
    BaseBlock::FlushCodeBuffer(buffer, size);
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

    GenDispatcher(buffer);

    ClearCachePlatform(GetBufferStart(buffer), stub_size);
}

Buffer *A64::CodeBlock::AllocCodeBuffer(VAddr source) {
    auto buffer = BaseBlock::AllocCodeBuffer(source);
    if (buffer->id_ != 0) {
        auto &dispatcher = dispatchers_[buffer->id_].go_forward_;
        auto delta = GetBufferStart(GetBuffer(0)) - reinterpret_cast<VAddr>(&dispatcher);
        // B offset
        dispatcher = 0x14000000 | (0x03ffffff & (static_cast<u32>(delta) >> 2));

        ClearCachePlatform(reinterpret_cast<VAddr>(&dispatcher), 4);
    }
    return buffer;
}
