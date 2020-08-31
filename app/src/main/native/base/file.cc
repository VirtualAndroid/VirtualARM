//
// Created by SwiftGan on 2020/8/29.
//

#include <fcntl.h>
#include "file.h"

using namespace FileSys;

File::File(const std::string &path) : path_(path) {
    file_fd_ = open(path.c_str(), O_RDONLY);
}

File::~File() {
    if (file_fd_ > 0) {
        close(file_fd_);
    }
}

void File::Read(void *dest, size_t offset, size_t size) {
    pread(file_fd_, dest, size, offset);
}

void File::Write(void *src, size_t offset, size_t size) {
    pwrite(file_fd_, src, size, offset);
}
