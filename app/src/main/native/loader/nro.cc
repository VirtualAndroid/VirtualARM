//
// Created by SwiftGan on 2020/8/29.
//

#include "nro.h"

using namespace Loader;

Nro::Nro(std::string path) {
    file = std::fopen(path.c_str(), "r+");

    fread()

    if (header.magic != MakeMagic<u32>("NRO0"))
        return;

    // The homebrew asset section is appended to the end of an NRO file
    if (backing->size > header.size) {
        backing->Read(&assetHeader, header.size);

        if (assetHeader.magic != util::MakeMagic<u32>("ASET"))
            throw exception("Invalid ASET magic! 0x{0:X}", assetHeader.magic);

        NroAssetSection &nacpHeader = assetHeader.nacp;
        nacp = std::make_shared<vfs::NACP>(std::make_shared<vfs::RegionBacking>(backing, header.size + nacpHeader.offset, nacpHeader.size));

        NroAssetSection &romFsHeader = assetHeader.romFs;
        romFs = std::make_shared<vfs::RegionBacking>(backing, header.size + romFsHeader.offset, romFsHeader.size);
    }
}

void *Nro::Load() {
    return nullptr;
}

std::vector<u8> Nro::GetSegment(const NroSegmentHeader &segment) {
    return std::vector<u8>();
}
