//
// Created by swift on 2020/3/3.
//

#pragma once

#include <base/marcos.h>
#include <map>
#include <vector>

namespace Jit {

#define MAX_BUFFER UINT16_MAX
#define MAX_BLOCK_BIT 26

    struct BlockHeader {
        char magic_[4];
        u32 arch_;
        u32 version_;
        u32 timestamp_;
        u16 block_count_;
        u32 size_;
    };

    struct Buffer {
        u16 id_;
        struct {
            u32 offset_:24;
            u32 version_:8;
        };
        // 4 *
        u16 size_;
        VAddr source_;
    };

    class BaseBlock {
    public:

        BaseBlock(VAddr start, VAddr size);

        VAddr GetBufferStart(u16 id);

        VAddr GetBufferStart(Buffer *buffer);

        VAddr GetBufferEnd(Buffer *buffer);

        Buffer *GetBuffer(u16 id);

        virtual Buffer* AllocCodeBuffer(VAddr source);

        virtual void FlushCodeBuffer(Buffer *buffer, u32 size);

        void Align(u32 size);

        virtual bool SaveToDisk(std::string path);

        u16 GetCurrentId() const;

        std::mutex &Lock();

        VAddr Base();

        bool Full();

    protected:
        VAddr start_;
        VAddr size_;
        std::mutex lock_;
        u16 current_buffer_id_{0};
        u32 current_offset_{0};
        std::vector<Buffer> buffers_;
    };

    namespace A64 {

        /**
         * 考虑到指令的可修改性
         * 我们需要一个中间人来分发代码
         * 当指令被修改时，我们生成新的指令，并且将分发表指向新指令缓存
         */

        struct Dispatcher {
            // B label
            u32 go_forward_;
        };

#define BLOCK_SIZE_A64 UINT32_C(16 * 1024 * 1024)
#define BLOCK_SIZE_A64_MAX UINT32_C(128 * 1024 * 1024)

        class CodeBlock : public BaseBlock {
        public:
            CodeBlock(u32 block_size = BLOCK_SIZE_A64);
            virtual ~CodeBlock();

            void GenDispatcherStub(u8 reg_forward, VAddr dispatcher_trampoline);

            void FlushCodeBuffer(Buffer *buffer, u32 size) override;

            void GenDispatcher(Buffer *buffer);

            VAddr GetDispatcherAddr(Buffer*buffer);

            VAddr GetDispatcherOffset(Buffer *buffer);

            void SetModuleMapAddress(VAddr addr);

            VAddr ModuleMapAddressAddress();

        protected:

            u32 buffer_count_;
            u32 forward_reg_rec_size_;
            VAddr *module_base_;
            u32 dispatcher_stub_offset_{0};
            Dispatcher *dispatchers_;
        };

        using CodeBlockRef = SharedPtr<CodeBlock>;

    }

}
