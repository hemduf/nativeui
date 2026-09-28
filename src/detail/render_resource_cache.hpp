#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace ui::detail {

template <class Key,
          class Resource,
          class Hash = std::hash<Key>,
          class Equal = std::equal_to<Key>>
class RenderResourceCache {
public:
    struct Limits {
        std::size_t max_entries{512};
        std::size_t max_accounted_bytes{128U * 1024U * 1024U};
    };

    struct Acquisition {
        std::shared_ptr<const Resource> resource;
        bool hit{false};
        bool retained{false};

        [[nodiscard]] explicit operator bool() const noexcept {
            return static_cast<bool>(resource);
        }
    };

    explicit RenderResourceCache(Limits limits = {})
        : limits_(limits) {
        index_.reserve(limits_.max_entries);
    }

    RenderResourceCache(const RenderResourceCache&) = delete;
    RenderResourceCache& operator=(const RenderResourceCache&) = delete;
    RenderResourceCache(RenderResourceCache&&) = delete;
    RenderResourceCache& operator=(RenderResourceCache&&) = delete;

    ~RenderResourceCache() noexcept = default;

    template <class Factory>
    [[nodiscard]] Acquisition acquire(const Key& key,
                                      std::size_t accounted_bytes,
                                      Factory&& factory) {
        if (auto found = index_.find(key); found != index_.end()) {
            entries_.splice(entries_.end(), entries_, found->second);
            return {found->second->resource, true, true};
        }

        std::shared_ptr<const Resource> resource =
            std::invoke(std::forward<Factory>(factory));
        if (!resource) return {};

        if (!can_retain(accounted_bytes) ||
            !can_make_room(accounted_bytes)) {
            return {std::move(resource), false, false};
        }

        evict_until_room(accounted_bytes);

        entries_.push_back(Entry{key, resource, accounted_bytes});
        const auto inserted_entry = std::prev(entries_.end());
        try {
            const auto [position, inserted] =
                index_.emplace(inserted_entry->key, inserted_entry);
            if (!inserted) {
                throw std::logic_error(
                    "RenderResourceCache key equality changed during insertion");
            }
            (void)position;
        } catch (...) {
            entries_.erase(inserted_entry);
            throw;
        }

        retained_accounted_bytes_ += accounted_bytes;
        return {std::move(resource), false, true};
    }

    [[nodiscard]] std::size_t retained_entries() const noexcept {
        return entries_.size();
    }

    [[nodiscard]] std::size_t retained_accounted_bytes() const noexcept {
        return retained_accounted_bytes_;
    }

    [[nodiscard]] bool contains(const Key& key) const {
        return index_.find(key) != index_.end();
    }

    void clear() noexcept {
        index_.clear();
        entries_.clear();
        retained_accounted_bytes_ = 0;
    }

private:
    struct Entry {
        Key key;
        std::shared_ptr<const Resource> resource;
        std::size_t accounted_bytes{};
    };

    using EntryList = std::list<Entry>;
    using EntryIterator = typename EntryList::iterator;

    [[nodiscard]] bool can_retain(std::size_t accounted_bytes) const noexcept {
        return limits_.max_entries != 0 &&
               accounted_bytes <= limits_.max_accounted_bytes;
    }

    [[nodiscard]] bool has_room(std::size_t entry_count,
                                std::size_t accounted_bytes,
                                std::size_t incoming_bytes) const noexcept {
        return entry_count < limits_.max_entries &&
               accounted_bytes <= limits_.max_accounted_bytes &&
               incoming_bytes <=
                   limits_.max_accounted_bytes - accounted_bytes;
    }

    [[nodiscard]] bool can_make_room(
        std::size_t incoming_bytes) const noexcept {
        std::size_t simulated_entries = entries_.size();
        std::size_t simulated_bytes = retained_accounted_bytes_;
        if (has_room(simulated_entries, simulated_bytes, incoming_bytes)) {
            return true;
        }

        for (const auto& entry : entries_) {
            if (entry.resource.use_count() != 1) continue;
            --simulated_entries;
            if (entry.accounted_bytes > simulated_bytes) return false;
            simulated_bytes -= entry.accounted_bytes;
            if (has_room(simulated_entries, simulated_bytes, incoming_bytes)) {
                return true;
            }
        }
        return false;
    }

    void evict_until_room(std::size_t incoming_bytes) {
        for (auto it = entries_.begin();
             !has_room(entries_.size(),
                       retained_accounted_bytes_,
                       incoming_bytes);) {
            if (it == entries_.end()) {
                throw std::logic_error(
                    "RenderResourceCache eviction preflight was inconsistent");
            }
            if (it->resource.use_count() != 1) {
                ++it;
                continue;
            }

            auto victim = it++;
            const auto found = index_.find(victim->key);
            if (found == index_.end()) {
                throw std::logic_error(
                    "RenderResourceCache index lost an LRU entry");
            }
            index_.erase(found);
            retained_accounted_bytes_ -= victim->accounted_bytes;
            entries_.erase(victim);
        }
    }

    Limits limits_;
    EntryList entries_;
    std::unordered_map<Key, EntryIterator, Hash, Equal> index_;
    std::size_t retained_accounted_bytes_{0};
};

} // namespace ui::detail
