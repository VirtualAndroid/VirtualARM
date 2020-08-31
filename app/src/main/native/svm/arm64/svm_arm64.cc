//
// Created by 甘尧 on 2020-02-29.
//

#include "svm_arm64.h"
#include "block/code_find_table.h"

using namespace SVM::A64;
using namespace Jit;

Instance::Instance() {
    jit_config_ = {
            .jit_thread_count = 2,
            .context_reg = 30, // lr
            .forward_reg = 16,
            .protect_code = true,
            .use_host_clock = true
    };
    mmu_config_ = {
            .enable = false,
            .addr_width = 48,
            .page_bits = 12,
            .readable_bit = 0,
            .writable_bit = 1,
            .executable_bit = 2
    };
}

Instance::Instance(const JitConfig &jit, const MmuConfig &mmu) : jit_config_(jit),
                                                                 mmu_config_(mmu) {}

void Instance::Initialize() {
    code_find_table_ = SharedPtr<FindTable<VAddr>>(new FindTable<VAddr>(mmu_config_.addr_width, 2));
    global_stubs_ = SharedPtr<GlobalStubs>(new GlobalStubs(SharedFrom(this)));
    jit_manager_ = SharedPtr<JitManager>(new JitManager(SharedFrom(this)));
    jit_manager_->Initialize();
    if (mmu_config_.enable) {
        mmu_ = SharedPtr<A64MMU>(new A64MMU(mmu_config_.addr_width, mmu_config_.page_bits));
    }
    isolate_cache_blocks_.push_back(AllocCacheBlock(BLOCK_SIZE_A64));
}

const JitConfig &Instance::GetJitConfig() const {
    return jit_config_;
}

const SharedPtr<Jit::FindTable<VAddr>> &Instance::GetCodeFindTable() const {
    return code_find_table_;
}

const SharedPtr<GlobalStubs> &Instance::GetGlobalStubs() const {
    return global_stubs_;
}

JitCacheEntry *Instance::FindAndJit(VAddr addr) {
    if (BOOST_UNLIKELY(!Executable(addr))) {
        return nullptr;
    }
    auto entry = jit_manager_->EmplaceJit(addr);
    return entry;
}

const MmuConfig &Instance::GetMmuConfig() const {
    return mmu_config_;
}

const SharedPtr<SVM::A64::A64MMU> &Instance::GetMmu() const {
    return mmu_;
}

const SharedPtr<JitManager> &Instance::GetJitManager() const {
    return jit_manager_;
}

void Instance::RegisterCodeSet(const std::shared_ptr<Jit::CodeSet> &code_set) {
    std::unique_lock guard(code_set_lock_);
    auto code_start = code_set->CodeSegment().addr;
    auto code_end = code_set->CodeSegment().addr + code_set->CodeSegment().size;
    code_sets_.push_back(code_set);
    auto alloc_size = AlignUp(reinterpret_cast<VAddr>((code_end - code_start) >> 4), 0x1000);
    auto code_block = AllocCacheBlock(std::max(alloc_size + 0x4000, (u64)BLOCK_SIZE_A64));
    cache_blocks_set_[code_set.get()] = code_block;
    const IntervalType interval{code_start, code_end};
    cache_blocks_addresses_.insert({interval, code_block});

    if (!mmu_config_.enable && jit_config_.protect_code) {
        ProtectCodeSegment(code_start, code_end);
    }
}

CodeBlock *Instance::AllocCacheBlock(u32 size) {
    auto block = std::make_unique<CodeBlock>(size);
    block->GenDispatcherStub(jit_config_.forward_reg, global_stubs_->GetForwardCodeCache());
    auto res = block.get();
    cache_blocks_.push_back(std::move(block));
    return res;
}

CodeBlock *Instance::PeekCacheBlock(VAddr pc) {
    std::shared_lock guard(code_set_lock_);
    const auto &it = cache_blocks_addresses_.find(pc);
    if (it == cache_blocks_addresses_.end()) {
        for (auto block : isolate_cache_blocks_) {
            if (!block->Full()) {
                return block;
            }
        }
        return nullptr;
    }
    return it->second;
}

void Instance::ProtectCodeSegment(VAddr start, VAddr end) {
    //TODO signal handler
}

bool Instance::Executable(VAddr vaddr) {
    if (mmu_) {
        auto pte = mmu_->GetPage(vaddr & mmu_->page_mask_);
        if (pte) {
            return true;
        } else {
            return false;
        }
    } else {
        return vaddr > PAGE_SIZE;
    }
}
