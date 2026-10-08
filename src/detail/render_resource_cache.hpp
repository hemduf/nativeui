#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <list>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

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
        // One additional bucket slot keeps index iterators stable while a
        // candidate entry is staged ahead of transactional eviction.
        const auto staged_capacity =
            limits_.max_entries < index_.max_size()
                ? limits_.max_entries + 1
                : limits_.max_entries;
        index_.reserve(staged_capacity);
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

        return acquire_miss(
            accounted_bytes,
            [&key] { return Key{key}; },
            std::forward<Factory>(factory));
    }

    template <class LookupKey, class KeyFactory, class Factory>
    [[nodiscard]] Acquisition acquire_with_lookup(
        const LookupKey& lookup_key,
        std::size_t accounted_bytes,
        KeyFactory&& key_factory,
        Factory&& factory) {
        if (auto found = index_.find(lookup_key); found != index_.end()) {
            entries_.splice(entries_.end(), entries_, found->second);
            return {found->second->resource, true, true};
        }

        return acquire_miss(
            accounted_bytes,
            std::forward<KeyFactory>(key_factory),
            std::forward<Factory>(factory));
    }

    [[nodiscard]] Acquisition find(const Key& key) {
        if (auto found = index_.find(key); found != index_.end()) {
            entries_.splice(entries_.end(), entries_, found->second);
            return {found->second->resource, true, true};
        }
        return {};
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

    template <class KeyFactory, class Factory>
    [[nodiscard]] Acquisition acquire_miss(
        std::size_t accounted_bytes,
        KeyFactory&& key_factory,
        Factory&& factory) {
        std::shared_ptr<const Resource> resource =
            std::invoke(std::forward<Factory>(factory));
        if (!resource) return {};

        if (!can_retain(accounted_bytes) ||
            !can_make_room(accounted_bytes)) {
            return {std::move(resource), false, false};
        }

        // Resolve every victim before publishing the candidate. Hash/equality,
        // eviction-plan allocation and owned-key construction may throw, but
        // retained state and LRU order remain untouched until staging succeeds.
        auto eviction_plan = prepare_evictions(accounted_bytes);

        // Recompute the exact post-eviction byte count before mutating retained
        // state. This keeps both subtraction and the final insertion addition
        // checked even if an internal accounting invariant is violated.
        std::size_t retained_after_eviction = retained_accounted_bytes_;
        for (const auto& victim : eviction_plan) {
            if (victim.entry->accounted_bytes > retained_after_eviction) {
                throw std::logic_error(
                    "RenderResourceCache retained-byte accounting underflow");
            }
            retained_after_eviction -= victim.entry->accounted_bytes;
        }
        if (retained_after_eviction > limits_.max_accounted_bytes ||
            accounted_bytes >
                limits_.max_accounted_bytes - retained_after_eviction) {
            throw std::logic_error(
                "RenderResourceCache retained-byte accounting overflow");
        }
        const auto retained_after_insert =
            retained_after_eviction + accounted_bytes;

        Key stored_key = std::invoke(std::forward<KeyFactory>(key_factory));

        // Stage every potentially-throwing insertion before evicting an
        // existing entry. The constructor reserves one extra map slot, so this
        // insertion cannot rehash and invalidate prepared index iterators.
        entries_.push_back(
            Entry{std::move(stored_key), resource, accounted_bytes});
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

        commit_evictions(eviction_plan);
        retained_accounted_bytes_ = retained_after_insert;
        return {std::move(resource), false, true};
    }

    using EntryList = std::list<Entry>;
    using EntryIterator = typename EntryList::iterator;
    using Index = std::unordered_map<Key, EntryIterator, Hash, Equal>;
    using IndexIterator = typename Index::iterator;

    struct EvictionVictim {
        EntryIterator entry;
        IndexIterator index;
    };

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

    [[nodiscard]] std::vector<EvictionVictim> prepare_evictions(
        std::size_t incoming_bytes) {
        std::vector<EvictionVictim> victims;
        std::size_t simulated_entries = entries_.size();
        std::size_t simulated_bytes = retained_accounted_bytes_;
        if (has_room(simulated_entries, simulated_bytes, incoming_bytes)) {
            return victims;
        }

        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->resource.use_count() != 1) continue;

            if (it->accounted_bytes > simulated_bytes) {
                throw std::logic_error(
                    "RenderResourceCache retained-byte accounting underflow");
            }
            const auto found = index_.find(it->key);
            if (found == index_.end() || found->second != it) {
                throw std::logic_error(
                    "RenderResourceCache index lost an LRU entry");
            }

            // Allocation here is intentionally still part of the prepare
            // phase. If it fails, no retained cache state has changed.
            victims.push_back(EvictionVictim{it, found});
            --simulated_entries;
            simulated_bytes -= it->accounted_bytes;
            if (has_room(simulated_entries, simulated_bytes, incoming_bytes)) {
                return victims;
            }
        }

        throw std::logic_error(
            "RenderResourceCache eviction preflight was inconsistent");
    }

    void commit_evictions(
        const std::vector<EvictionVictim>& victims) noexcept {
        for (const auto& victim : victims) {
            index_.erase(victim.index);
            entries_.erase(victim.entry);
        }
    }

    Limits limits_;
    EntryList entries_;
    Index index_;
    std::size_t retained_accounted_bytes_{0};
};

} // namespace ui::detail
