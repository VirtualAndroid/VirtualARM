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
        static_assert(page_bits <= 32);
        static constexpr size_t page_size = u64(1) << page_bits;
        static constexpr size_t addr_mask = (u64(1) << 32) - 1;

        struct Entry {
            size_t addr_start{0};
            size_t addr_end{0};
            std::array<u8, sizeof(T)> data;

            bool used{false};
            // executable
            bool disabled{false};

            constexpr bool Overlaps(VAddr start, VAddr end) const noexcept {
                return start < addr_end && addr_start < end;
            }

            constexpr T &Data() {
                return *reinterpret_cast<T*>(data.data());
            }

            constexpr T *DataPtr() {
                return reinterpret_cast<T*>(data.data());
            }
        };

        explicit JitCache(size_t capacity, size_t step) : step_{step} {
            block_data_.resize(capacity);
        }

        Entry *GetUnsafe(size_t addr) {
            const auto &it = code_page_cache_.find(addr >> page_bits);
            if (it != code_page_cache_.end()) {
                auto &entry = it->second[addr & addr_mask];
                if (entry && entry->addr_start != addr) {
                    entry = nullptr;
                }
                return entry;
            } else {
                return nullptr;
            }
        }

        Entry *Get(size_t addr) {
            std::shared_lock guard(lock_);
            return GetUnsafe(addr);
        }

        Entry *TryGetUnsafe(size_t addr) {
            auto entry = GetUnsafe(addr);
            if (entry) {
                return entry;
            }
            entry = holding_cache_[addr];
            if (entry) {
                return entry;
            }
            return nullptr;
        }

        Entry *TryGet(size_t addr) {
            std::shared_lock guard(lock_);
            return TryGetUnsafe(addr);
        }

        template <typename ...Args>
        Entry *Emplace(size_t addr, Args... args) {
            auto entry = TryGet(addr);
            if (entry) {
                return entry;
            }
            std::unique_lock guard(lock_);
            auto holding = TryGetUnsafe(addr);
            if (holding) {
                return holding;
            }
            holding = AllocEntry();
            holding->addr_start = addr;
            new (holding->data.data()) T(std::forward<Args>(args)...);
            holding_cache_[addr] = holding;
            return holding;
        }

        void Flush(Entry *entry) {
            std::unique_lock guard(lock_);
            const auto &it = holding_cache_.find(entry->addr_start);
            if (it == holding_cache_.end()) {
                return;
            }
            holding_cache_.erase(it);
            const size_t page_end = (entry->addr_end + page_size - 1) >> page_bits;
            for (size_t page = entry->addr_start >> page_bits; page < page_end; ++page) {
                code_page_cache_[page][entry->addr_start & addr_mask] = entry;
            }
        }

        template <typename ...Args>
        Entry *Put(size_t addr, size_t size, Args... args) {
            std::unique_lock guard(lock_);
            auto new_entry = AllocEntry();
            new_entry->addr_start = addr;
            new_entry->addr_end = addr + size;
            new (new_entry->data.data()) T(std::forward<Args>(args)...);
            const size_t page_end = (addr + size + page_size - 1) >> page_bits;
            for (size_t page = addr >> page_bits; page < page_end; ++page) {
                code_page_cache_[page][addr & addr_mask] = new_entry;
            }
            return new_entry;
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
                    if (!entry->used) {
                        it->second.erase(sub_it);
                    } else if (entry->Overlaps(addr, addr_end)) {
                        FreeEntry(entry);
                        it->second.erase(sub_it);
                    }
                }
            }
        }

        void Enable(size_t addr, size_t size, bool enable) {
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
                    if (!entry->used) {
                        it->second.erase(sub_it);
                    } else if (entry->Overlaps(addr, addr_end)) {
                        entry->disabled = !enable;
                    }
                }
            }
        }

        void EnablePage(size_t page_index, bool enable) {
            Enable(page_index << page_bits, (page_index + 1) << page_bits, enable);
        }

    private:

        Entry *AllocEntry() {
            auto old_size = block_data_.size();
            if (last_data_pos_ < old_size - 1) {
                last_data_pos_++;
                auto res = &block_data_[last_data_pos_];
                if (!res->used) {
                    res->used = true;
                    return res;
                }
            }
            for (size_t i = 0; i < block_data_.size(); ++i) {
                Entry &entry = block_data_[i];
                if (!entry.used) {
                    entry.used = true;
                    last_data_pos_ = i;
                    return &entry;
                }
            }
            block_data_.resize(old_size + step_);
            last_data_pos_ = old_size;
            auto res = &block_data_[old_size];
            res->used = true;
            return res;
        }

        void FreeEntry(Entry *entry) {
            if (entry) {
                entry->Data().~T();
                entry->addr_start = 0;
                entry->addr_end = 0;
                entry->used = false;
                entry->disabled = false;
            }
        }

        std::unordered_map<size_t, Entry*> holding_cache_;
        std::unordered_map<size_t, std::unordered_map<u32, Entry*>> code_page_cache_;
        std::vector<Entry> block_data_;
        size_t last_data_pos_{0};
        size_t step_{0};
        mutable std::shared_mutex lock_;
    };

}
