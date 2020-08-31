//
// Created by SwiftGan on 2020/8/20.
//

#include <sys/mman.h>
#include "memory.h"

void *Platform::MapExecutableMemory(size_t size, VAddr addr) {
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    if (addr != 0) {
        flags |= MAP_FIXED;
    }
    return mmap(reinterpret_cast<void *>(addr), size, PROT_READ | PROT_WRITE | PROT_EXEC,
                flags,
                -1, 0);
}

void Platform::UnMapExecutableMemory(VAddr addr, size_t size) {
    munmap(reinterpret_cast<void *>(addr), size);
}
