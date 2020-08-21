//
// Created by SwiftGan on 2020/8/21.
//

#pragma once

#include <vector>
#include <unordered_map>
#include <shared_mutex>

namespace Jit {

    template<typename T, size_t page_bits>
    class JitCache : public BaseObject {
    public:
        static_assert(page_bits <= 16);
        static constexpr size_t page_size = u64(1) << page_bits;
        static constexpr size_t page_mask = page_size - 1;

        struct Entry {
            size_t addr_start;
            size_t addr_end;
            T data;

            constexpr bool Overlaps(VAddr start, VAddr end) const noexcept {
                return start < addr_end && addr_start < end;
            }
        };

        explicit JitCache(size_t capacity, size_t step) : step_{step} {
            block_data_.resize(capacity);
        }

        Entry *Get(size_t addr) const {
            std::shared_lock guard(lock_);
            auto &it = code_page_cache_.find(addr << page_bits);
            if (it != code_page_cache_.end()) {
                auto &entry = it[addr & page_mask];
                if (entry && entry->addr_start != addr) {
                    entry = nullptr;
                }
                return entry;
            } else {
                return nullptr;
            }
        }

        void Put(const Entry &entry) {
            std::unique_lock guard(lock_);
            const size_t page_end = (entry.addr_end + page_size - 1) >> page_bits;
            auto new_entry = AllocEntry();
            *new_entry = entry;
            for (size_t page = entry.addr_start >> page_bits; page < page_end; ++page) {
                code_page_cache_[page][entry.addr_start & page_mask] = new_entry;
            }
        }

        void Invalid(size_t addr, size_t size) {
            std::unique_lock guard(lock_);
            const VAddr addr_end = addr + size;
            const u64 page_end = (addr_end + page_size - 1) >> page_bits;
            for (u64 page = addr >> page_bits; page < page_end; ++page) {
                auto it = code_page_cache_.find(page);
                if (it == code_page_cache_.end()) {
                    continue;
                }
                for (auto sub_it = it->second.begin(); sub_it != it->second.end(); sub_it++) {
                    auto entry = sub_it->second;
                    if (!entry->addr_start) {
                        it->second.erase(sub_it);
                    } else if (entry->Overlaps(addr, addr_end)) {
                        FreeEntry(entry);
                        it->second.erase(sub_it);
                    }
                }
            }
        }

        void InvalidPage(size_t page_index) {
            std::unique_lock guard(lock_);
            auto &it = code_page_cache_.find(page_index);
            if (it != code_page_cache_.end()) {
                for (auto entry : it->second) {
                    FreeEntry(entry);
                }
                code_page_cache_.erase(it);
            }
        }

    private:

        Entry *AllocEntry() {
            auto old_size = block_data_.size();
            if (last_data_pos_ < old_size - 1) {
                last_data_pos_++;
                return &block_data_[last_data_pos_];
            }
            for (size_t i = 0; i < block_data_.size(); ++i) {
                Entry &entry = block_data_[i];
                if (entry.addr_start) {
                    last_data_pos_ = i;
                    return &entry;
                }
            }
            block_data_.resize(old_size + step_);
            last_data_pos_ = old_size;
            return &block_data_[old_size];
        }

        void FreeEntry(Entry *entry) {
            if (entry) {
                entry->data.Destroy();
                *entry = {};
            }
        }

        std::unordered_map<size_t, std::unordered_map<u16, Entry*>> code_page_cache_;
        std::vector<Entry> block_data_;
        size_t last_data_pos_{0};
        size_t step_{0};
        mutable std::shared_mutex lock_;
    };

}
