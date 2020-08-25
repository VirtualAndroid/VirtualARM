//
// Created by SwiftGan on 2020/8/20.
//

#include <sys/mman.h>
#include "memory.h"

void *Platform::MapExecutableMemory(size_t size) {
    return mmap(0, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS,
                -1, 0);
}

void Platform::UnMapExecutableMemory(VAddr addr, size_t size) {
    munmap(reinterpret_cast<void *>(addr), size);
}
