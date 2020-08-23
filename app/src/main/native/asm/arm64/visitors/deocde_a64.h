//
// Created by SwiftGan on 2020/8/20.
//

#pragma once

namespace Decode::A64 {

    enum BrunchFlags {
        Link    = 1 << 0,
        TestBit = 1 << 1,
        Comp    = 1 << 2,
        Negate  = 1 << 3,
        CompW   = 1 << 4
    };

    enum LoadStoreFlags {
        WriteBack   = 1 << 0,
        ImmSigned   = 1 << 1,
        PostIndex   = 1 << 2,
        Float       = 1 << 3,
        Bit128Float = 1 << 4,
        Release     = 1 << 5,
        Exclusive   = 1 << 6,
        ExtendRes   = 1 << 7,
        ExtendTo64  = 1 << 8,
        Acquire     = 1 << 9,
        Prfm        = 1 << 10,
        LoadSigned  = 1 << 11
    };

    enum ExceptionFlags {
        Svc     = 1 << 0,
        Brk     = 1 << 1,
        Hvc     = 1 << 2,
    };

}
