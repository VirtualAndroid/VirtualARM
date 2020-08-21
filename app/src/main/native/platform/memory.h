//
// Created by SwiftGan on 2020/8/20.
//

#pragma once

#include "base/marcos.h"

namespace Platform {
    void *MapExecutableMemory(size_t size);
    void UnMapExecutableMemory(VAddr addr, size_t size);
}
