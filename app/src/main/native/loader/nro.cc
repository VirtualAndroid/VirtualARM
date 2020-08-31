//
// Created by SwiftGan on 2020/8/29.
//

#include "nro.h"
#include "vector"

using namespace Loader;

Nro::Nro(std::string path) {
    file_ = SharedPtr<FileSys::File> (new FileSys::File(path));

    header_ = file_->Read<NroHeader>(0);

    if (header_.magic != MakeMagic<u32>("NRO0"))
        return;

    text_.resize(header_.text.size);
    rodata_.resize(header_.ro.size);
    data_.resize(header_.data.size);

    file_->Read(text_.data(), header_.text.offset, header_.text.size);
    file_->Read(rodata_.data(), header_.ro.offset, header_.ro.size);
    file_->Read(data_.data(), header_.data.offset, header_.data.size);
}

bool Nro::Load(CodeSet *code_set) {
    code_set->CodeSegment().addr = code_set->base_addr;
    code_set->CodeSegment().offset = 0;
    code_set->CodeSegment().size = text_.size();
    std::memcpy(reinterpret_cast<void *>(code_set->CodeSegment().addr), text_.data(), text_.size());

    code_set->RODataSegment().addr = code_set->base_addr + text_.size();
    code_set->RODataSegment().offset = text_.size();
    code_set->RODataSegment().size = rodata_.size();
    std::memcpy(reinterpret_cast<void *>(code_set->RODataSegment().addr), rodata_.data(), rodata_.size());

    code_set->DataSegment().addr = code_set->base_addr + text_.size() + rodata_.size();
    code_set->DataSegment().offset = text_.size() + rodata_.size();
    code_set->DataSegment().size = rodata_.size();
    std::memcpy(reinterpret_cast<void *>(code_set->DataSegment().addr), data_.data(), data_.size());

    code_set->entrypoint = code_set->base_addr;
    return true;
}

std::vector<u8> Nro::GetSegment(const NroSegmentHeader &segment) {
    return std::vector<u8>();
}

size_t Nro::GetLoadSegmentsSize() {
    return text_.size() + rodata_.size() + data_.size();
}
