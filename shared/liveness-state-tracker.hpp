#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace custom_types::liveness_debug {

enum class TrackResult {
    tracked,
    alreadyTracked,
    invalidState,
    full,
};

struct LookupResult {
    bool found;
    void* filter;
};

/// Allocation-free state-to-filter registry for IL2CPP liveness diagnostics.
///
/// A state value of 1 reserves a slot while its filter is being published or
/// removed. Real liveness states are aligned native allocations and cannot use
/// that value. Readers only observe a state after its filter has been stored.
template <std::size_t Capacity>
class LivenessStateTracker {
   public:
    static_assert(Capacity > 0);

    TrackResult track(void* state, void* filter) noexcept {
        const auto stateValue = reinterpret_cast<std::uintptr_t>(state);
        if (stateValue <= reservedState) return TrackResult::invalidState;

        for (auto& slot : slots) {
            if (slot.state.load(std::memory_order_acquire) == stateValue) {
                return TrackResult::alreadyTracked;
            }
        }

        for (auto& slot : slots) {
            std::uintptr_t expected = emptyState;
            if (!slot.state.compare_exchange_strong(expected, reservedState, std::memory_order_acq_rel)) continue;

            slot.sequence.fetch_add(1, std::memory_order_acq_rel);
            slot.filter.store(filter, std::memory_order_relaxed);
            slot.state.store(stateValue, std::memory_order_release);
            slot.sequence.fetch_add(1, std::memory_order_release);
            return TrackResult::tracked;
        }

        return TrackResult::full;
    }

    [[nodiscard]] LookupResult find(void* state) const noexcept {
        const auto stateValue = reinterpret_cast<std::uintptr_t>(state);
        if (stateValue <= reservedState) return {};

        for (const auto& slot : slots) {
            const auto sequenceBefore = slot.sequence.load(std::memory_order_acquire);
            if (sequenceBefore & 1) continue;

            if (slot.state.load(std::memory_order_acquire) == stateValue) {
                auto* const filter = slot.filter.load(std::memory_order_relaxed);
                const auto sequenceAfter = slot.sequence.load(std::memory_order_acquire);
                if (sequenceBefore == sequenceAfter) return { true, filter };
            }
        }

        return {};
    }

    bool untrack(void* state) noexcept {
        const auto stateValue = reinterpret_cast<std::uintptr_t>(state);
        if (stateValue <= reservedState) return false;

        for (auto& slot : slots) {
            std::uintptr_t expected = stateValue;
            if (!slot.state.compare_exchange_strong(expected, reservedState, std::memory_order_acq_rel)) continue;

            slot.sequence.fetch_add(1, std::memory_order_acq_rel);
            slot.filter.store(nullptr, std::memory_order_relaxed);
            slot.state.store(emptyState, std::memory_order_release);
            slot.sequence.fetch_add(1, std::memory_order_release);
            return true;
        }

        return false;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return Capacity;
    }

   private:
    static constexpr std::uintptr_t emptyState = 0;
    static constexpr std::uintptr_t reservedState = 1;

    struct Slot {
        std::atomic<std::uintptr_t> state{ emptyState };
        std::atomic<void*> filter{ nullptr };
        std::atomic<std::uint64_t> sequence{ 0 };
    };

    std::array<Slot, Capacity> slots{};
};

}  // namespace custom_types::liveness_debug
