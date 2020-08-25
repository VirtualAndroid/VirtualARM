//
// Created by SwiftGan on 2020/8/23.
//

#pragma once

#include <base/marcos.h>
#include <base/threadsafe_queue.h>
#include <block/code_cache.h>
#include <block/host_code_block.h>
#include <list>

namespace SVM::A64 {
    class Instance;
    class JitThread;
}

namespace Jit::A64 {

    using namespace SVM::A64;

    constexpr static size_t page_bits = 12;

    class JitCacheBlock : NonCopyable {
    public:
        JitCacheBlock() = default;
        JitCacheBlock(CodeBlock *codeBlock);
        ~JitCacheBlock() = default;

        constexpr VAddr GetStub() const {
            if (!code_block || !id_in_block) {
                return 0;
            }
            auto buffer = code_block->GetBuffer(id_in_block);
            if (buffer) {
                code_block->GetDispatcherAddr(buffer);
            }
            return 0;
        }

        CodeBlock *code_block{nullptr};
        u16 id_in_block{0};
        bool ready{false};
        SpinMutex jit_lock;
    };

    using JitCacheA64 = Jit::JitCache<JitCacheBlock, page_bits>;
    using JitCacheEntry = JitCacheA64::Entry;

    class JitManager : public BaseObject {
    public:
        JitManager(const SharedPtr<Instance> &instance);

        void Initialize();

        const SharedPtr<Instance> &GetInstance() const;

        JitCacheEntry *EmplaceJit(VAddr addr);

        void CommitJit(JitCacheEntry *entry);

        void JitNow(JitCacheEntry *entry);

        void JitFromQueue();

    private:

        void JitUnsafe(JitCacheEntry *entry);

        SharedPtr<Instance> instance_;
        SharedPtr<JitCacheA64> jit_cache_;
        rigtorp::MPMCQueue<JitCacheEntry *> queue_;
        std::list<SharedPtr<JitThread>> jit_threads_;
    };

    class JitNestGuard {
    public:
        JitNestGuard();
        ~JitNestGuard();

        static int CurrentNest();
    };

}
