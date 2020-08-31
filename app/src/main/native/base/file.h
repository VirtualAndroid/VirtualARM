//
// Created by SwiftGan on 2020/8/29.
//

#pragma once

#include "marcos.h"

namespace FileSys {

    class File : public BaseObject {
    public:

        File(const std::string &path);

        virtual ~File();

        void Read(void *dest, size_t offset, size_t size);

        void Write(void *src, size_t offset, size_t size);

        template <typename T>
        T Read(size_t offset) {
            T t;
            Read(&t, offset, sizeof(T));
            return std::move(t);
        }

        template <typename T>
        void Write(T &t, size_t offset) {
            Write(&t, offset, sizeof(T));
        }

    private:
        std::string path_;
        int file_fd_;
    };

}
