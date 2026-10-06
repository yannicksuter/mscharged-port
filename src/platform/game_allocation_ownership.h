#pragma once

#include <cstddef>
#include <cstdint>

class MemoryAllocator;

namespace mscharged::platform
{
// Native ownership metadata lives outside the original game arena. Reserve it
// before the original allocator mutates its ring, so host metadata OOM cannot
// turn a successful game allocation into an unowned block.
class GameAllocationReservation
{
public:
    GameAllocationReservation(MemoryAllocator& owner, unsigned long bytes);
    ~GameAllocationReservation();
    GameAllocationReservation(const GameAllocationReservation&) = delete;
    GameAllocationReservation& operator=(const GameAllocationReservation&) = delete;
    void Commit(void* pointer);

private:
    void* pending_;
};

// Native storage facts, not game readiness or GL suballocation/frame lifetime.
// Positive byte ranges choose the innermost actual original allocation. A
// range crossing a child allocation is rejected, never assigned to its parent.
struct GameAllocationSpan
{
    void* base;
    std::size_t bytes;
    MemoryAllocator* owner;
    std::uint64_t incarnation;
};
bool FindGameAllocationSpan(const void* pointer, std::size_t bytes, GameAllocationSpan& result);

enum class GameByteDomain { WiiSerialized, NativeHeader, NativePayload };
struct GameCompletedSpan
{
    const void* base;
    std::size_t bytes;
    GameAllocationSpan allocation;
};
// Every requested byte must belong to one completed logical source span;
// splitting a domain for native conversion preserves that source extent.
bool FindGameCompletedSpan(const void* pointer, std::size_t bytes, GameCompletedSpan& result);
GameByteDomain FindGameByteDomain(const void* pointer, std::size_t bytes);

// Reserve every metadata node before bytes are touched. physicalBytes includes
// real DMA padding; only logicalBytes is published at completion. Partial writes
// preserve unaffected domains. Concurrent overlapping producers are unsupported.
// Native conversions can preserve an existing completed logical source extent;
// new serialized reads always publish their actual requested logical extent.
class GameByteWriteReservation
{
public:
    GameByteWriteReservation() noexcept : pending_(nullptr) {}
    GameByteWriteReservation(void* destination, std::size_t logicalBytes,
                            std::size_t physicalBytes = 0);
    ~GameByteWriteReservation();
    GameByteWriteReservation(const GameByteWriteReservation&) = delete;
    GameByteWriteReservation& operator=(const GameByteWriteReservation&) = delete;
    GameByteWriteReservation(GameByteWriteReservation&& other) noexcept;
    GameByteWriteReservation& operator=(GameByteWriteReservation&& other) noexcept;
    bool Tracked() const noexcept { return pending_ != nullptr; }
    void Complete(GameByteDomain domain);
    void Reset() noexcept;
private:
    void* pending_;
};
std::uint32_t ReadGameChunkWord(const void* header, unsigned word);

MemoryAllocator* FindGameAllocationOwner(const void* pointer);
void ValidateGameAllocationFree(MemoryAllocator& owner, const void* pointer);
void FinishGameAllocationFree(const void* pointer);
void DiscardGameAllocatorRecords(MemoryAllocator& owner);
void FreeGameAllocation(void* pointer);
}
