//
// Created by SwiftGan on 2020/8/20.
//

#pragma once

#include "base/marcos.h"

namespace Platform {
    void *MapExecutableMemory(size_t size, VAddr addr = 0);
    void UnMapExecutableMemory(VAddr addr, size_t size);
}
