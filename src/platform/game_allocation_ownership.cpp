#include "platform/game_allocation_ownership.h"
#include "platform/host_metadata.h"
#include "NL/MemAlloc.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace mscharged::platform
{
namespace
{
template<class T> struct HostMetadataAllocator
{
    using value_type = T;
    HostMetadataAllocator() = default;
    template<class U> HostMetadataAllocator(const HostMetadataAllocator<U>&) noexcept {}
    T* allocate(std::size_t count)
    {
        static_assert(alignof(T) <= alignof(std::max_align_t));
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc();
        return static_cast<T*>(ChargedNativeMetadataAllocate(count * sizeof(T)));
    }
    void deallocate(T* pointer, std::size_t) noexcept { ChargedNativeMetadataRelease(pointer); }
    template<class U> bool operator==(const HostMetadataAllocator<U>&) const noexcept { return true; }
};
struct ByteSpan
{
    std::size_t bytes;
    GameByteDomain domain;
    std::uintptr_t logical_base;
    std::size_t logical_bytes;
    std::uint64_t logical_tag;
};
using ByteSpans = std::map<std::uintptr_t, ByteSpan, std::less<std::uintptr_t>,
    HostMetadataAllocator<std::pair<const std::uintptr_t, ByteSpan>>>;
struct PendingWrite { std::size_t bytes; std::uint64_t tag; };
using PendingWrites = std::map<std::uintptr_t, PendingWrite, std::less<std::uintptr_t>,
    HostMetadataAllocator<std::pair<const std::uintptr_t, PendingWrite>>>;
struct Allocation
{
    MemoryAllocator* owner;
    std::uintptr_t arena;
    unsigned int arena_bytes;
    unsigned long bytes;
    std::uint64_t incarnation;
    ByteSpans byte_spans;
    PendingWrites pending_writes;
    std::uint64_t next_write = 1;
};
using Records = std::map<std::uintptr_t, Allocation, std::less<std::uintptr_t>,
    HostMetadataAllocator<std::pair<const std::uintptr_t, Allocation>>>;
struct Registry
{
    std::mutex mutex;
    Records records;
    std::uint64_t next_incarnation = 1;
};
Registry& State()
{
    // Constructed on first use; no dependence on another TU's static order.
    // The explicit C allocation boundary keeps nodes in the host CRT even when
    // this registry is compiled inside the original-game module.
    static Registry registry;
    return registry;
}
struct Pending
{
    Records::node_type record;
    Pending(MemoryAllocator& owner, unsigned long bytes, std::uint64_t incarnation)
    {
        Records temporary;
        temporary.emplace(0, Allocation{&owner,
            reinterpret_cast<std::uintptr_t>(owner.m_memory), owner.m_memory_size, bytes, incarnation});
        record = temporary.extract(0);
    }
};
void EraseBackingRange(Records& records, std::uintptr_t address, unsigned long bytes)
{
    // Child heaps can be contained in an allocation from another original heap.
    // Retiring that backing invalidates their metadata without running new game
    // destructors or altering the original bulk-discard operation.
    auto first = records.lower_bound(address);
    auto last = records.lower_bound(address + bytes);
    records.erase(first, last);
}
bool Contains(std::uintptr_t base, std::size_t bytes, std::uintptr_t address, std::size_t count)
{
    return count && address >= base && address - base < bytes
        && count <= bytes - (address - base);
}
bool Overlaps(std::uintptr_t a, std::size_t n, std::uintptr_t b, std::size_t m)
{
    return n && m && (a <= b ? b - a < n : a - b < m);
}
Records::iterator Containing(Records& records, std::uintptr_t address, std::size_t count)
{
    if (!count) return records.end();
    auto candidate = records.upper_bound(address);
    while (candidate != records.begin())
    {
        --candidate;
        // Select by the starting byte first. An extent leaving this actual
        // child must fail; it must never fall back to an outer allocation.
        if (!Contains(candidate->first, candidate->second.bytes, address, 1)) continue;
        if (!Contains(candidate->first, candidate->second.bytes, address, count))
            throw std::invalid_argument("Byte span leaves its actual source allocation");
        auto child = records.upper_bound(address);
        if (child != records.end() && child->first - address < count)
            throw std::invalid_argument("Byte span crosses a live nested allocation");
        return candidate;
    }
    return records.end();
}
GameAllocationSpan Describe(Records::iterator found)
{
    return {reinterpret_cast<void*>(found->first), found->second.bytes,
            found->second.owner, found->second.incarnation};
}
bool Completed(Allocation& allocation, std::uintptr_t address, std::size_t count, ByteSpan& origin)
{
    if (!count) return false;
    auto span = allocation.byte_spans.upper_bound(address);
    if (span == allocation.byte_spans.begin()) return false;
    --span;
    if (!Contains(span->first, span->second.bytes, address, 1)) return false;
    origin = span->second;
    if (!Contains(origin.logical_base, origin.logical_bytes, address, count)) return false;
    auto cursor = address;
    while (count)
    {
        if (span == allocation.byte_spans.end()
            || !Contains(span->first, span->second.bytes, cursor, 1)
            || span->second.logical_tag != origin.logical_tag
            || span->second.logical_base != origin.logical_base
            || span->second.logical_bytes != origin.logical_bytes) return false;
        const auto available = span->second.bytes - (cursor - span->first);
        const auto take = count < available ? count : available;
        cursor += take; count -= take; ++span;
    }
    return true;
}
struct PendingBytes
{
    std::uintptr_t allocation;
    std::uint64_t incarnation;
    std::uint64_t tag;
    ByteSpans::node_type completed;
    PendingWrites::node_type active;
    ByteSpan prior_origin{};
    bool has_prior_origin;
    PendingBytes(Records::iterator owner, std::uintptr_t address,
                 std::size_t logical, std::size_t physical, std::uint64_t write)
        : allocation(owner->first), incarnation(owner->second.incarnation), tag(write),
          has_prior_origin(Completed(owner->second, address, logical, prior_origin))
    {
        ByteSpans temporary;
        temporary.emplace(address, ByteSpan{logical, GameByteDomain::WiiSerialized, address, logical, write});
        completed = temporary.extract(address);
        PendingWrites writes;
        writes.emplace(address, PendingWrite{physical, write});
        active = writes.extract(address);
    }
};
ByteSpans RetainOutside(const ByteSpans& spans, std::uintptr_t address, std::size_t bytes)
{
    ByteSpans retained;
    for (const auto& [base, span] : spans)
    {
        if (!Overlaps(base, span.bytes, address, bytes)) { retained.emplace(base, span); continue; }
        if (base < address)
        {
            auto left = span; left.bytes = address - base;
            retained.emplace(base, left);
        }
        const auto end = address + bytes;
        if (end - base < span.bytes)
        {
            auto right = span; right.bytes = span.bytes - (end - base);
            retained.emplace(end, right);
        }
    }
    return retained;
}
std::uint32_t HeaderWord(const unsigned char* bytes, GameByteDomain domain)
{
    if (domain == GameByteDomain::WiiSerialized)
        return (std::uint32_t(bytes[0]) << 24) | (std::uint32_t(bytes[1]) << 16)
            | (std::uint32_t(bytes[2]) << 8) | std::uint32_t(bytes[3]);
    if (domain != GameByteDomain::NativeHeader)
        throw std::invalid_argument("Native payload is not a chunk-header domain");
    std::uint32_t result; std::memcpy(&result, bytes, sizeof(result)); return result;
}
}

GameAllocationReservation::GameAllocationReservation(MemoryAllocator& owner, unsigned long bytes)
    : pending_(nullptr)
{
    auto& state = State();
    std::uint64_t incarnation;
    {
        std::lock_guard lock(state.mutex);
        if (!state.next_incarnation) throw std::overflow_error("Allocation incarnation exhausted");
        incarnation = state.next_incarnation++;
    }
    void* storage = ChargedNativeMetadataAllocate(sizeof(Pending));
    try { pending_ = new (storage) Pending(owner, bytes, incarnation); }
    catch (...) { ChargedNativeMetadataRelease(storage); throw; }
}
GameAllocationReservation::~GameAllocationReservation()
{
    if (pending_)
    {
        static_cast<Pending*>(pending_)->~Pending();
        ChargedNativeMetadataRelease(pending_);
    }
}
void GameAllocationReservation::Commit(void* pointer)
{
    auto& pending = *static_cast<Pending*>(pending_);
    pending.record.key() = reinterpret_cast<std::uintptr_t>(pointer);
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto insertion = state.records.insert(std::move(pending.record));
    if (!insertion.inserted)
    {
        pending.record = std::move(insertion.node);
        throw std::logic_error("Original allocator returned an already live allocation");
    }
}
bool FindGameAllocationSpan(const void* pointer, std::size_t bytes, GameAllocationSpan& result)
{
    auto& state = State(); std::lock_guard lock(state.mutex);
    auto found = Containing(state.records, reinterpret_cast<std::uintptr_t>(pointer), bytes);
    if (found == state.records.end()) return false;
    result = Describe(found); return true;
}
bool FindGameCompletedSpan(const void* pointer, std::size_t bytes, GameCompletedSpan& result)
{
    auto& state = State(); std::lock_guard lock(state.mutex);
    auto address = reinterpret_cast<std::uintptr_t>(pointer);
    auto found = Containing(state.records, address, bytes);
    if (found == state.records.end()) return false;
    ByteSpan origin{};
    if (!Completed(found->second, address, bytes, origin)) return false;
    result = {reinterpret_cast<const void*>(origin.logical_base), origin.logical_bytes, Describe(found)};
    return true;
}
GameByteDomain FindGameByteDomain(const void* pointer, std::size_t bytes)
{
    auto& state = State(); std::lock_guard lock(state.mutex);
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    auto found = Containing(state.records, address, bytes);
    if (found == state.records.end()) throw std::invalid_argument("Bytes have no actual source allocation");
    auto span = found->second.byte_spans.upper_bound(address);
    if (span != found->second.byte_spans.begin())
    {
        --span;
        if (Contains(span->first, span->second.bytes, address, bytes)) return span->second.domain;
    }
    throw std::invalid_argument("Bytes have no uniform completed explicit domain");
}
GameByteWriteReservation::GameByteWriteReservation(void* destination, std::size_t logicalBytes,
                                                 std::size_t physicalBytes) : pending_(nullptr)
{
    if (!logicalBytes) throw std::invalid_argument("A byte reservation requires positive logical bytes");
    if (!physicalBytes) physicalBytes = logicalBytes;
    if (physicalBytes < logicalBytes) throw std::invalid_argument("Physical write is shorter than logical bytes");
    auto& state = State(); std::lock_guard lock(state.mutex);
    const auto address = reinterpret_cast<std::uintptr_t>(destination);
    auto owner = Containing(state.records, address, physicalBytes);
    if (owner == state.records.end()) throw std::invalid_argument("Byte write has no actual source allocation");
    for (const auto& [base, active] : owner->second.pending_writes)
        if (Overlaps(base, active.bytes, address, physicalBytes))
            throw std::logic_error("Concurrent overlapping byte producers are unsupported");
    if (!owner->second.next_write) throw std::overflow_error("Byte write incarnation exhausted");
    void* storage = ChargedNativeMetadataAllocate(sizeof(PendingBytes));
    PendingBytes* token = nullptr;
    try
    {
        token = new (storage) PendingBytes(owner, address, logicalBytes, physicalBytes, owner->second.next_write);
        // Any host OOM occurs before completed-domain invalidation or the actual
        // producer write. After reservation, completion inserts reserved nodes.
        auto retained = RetainOutside(owner->second.byte_spans, address, physicalBytes);
        auto inserted = owner->second.pending_writes.insert(std::move(token->active));
        if (!inserted.inserted) throw std::logic_error("Byte reservation address conflict");
        owner->second.byte_spans.swap(retained);
        ++owner->second.next_write;
        pending_ = token;
    }
    catch (...)
    {
        if (token) token->~PendingBytes();
        ChargedNativeMetadataRelease(storage); throw;
    }
}
GameByteWriteReservation::~GameByteWriteReservation() { Reset(); }
GameByteWriteReservation::GameByteWriteReservation(GameByteWriteReservation&& other) noexcept
    : pending_(std::exchange(other.pending_, nullptr)) {}
GameByteWriteReservation& GameByteWriteReservation::operator=(GameByteWriteReservation&& other) noexcept
{
    if (this != &other) { Reset(); pending_ = std::exchange(other.pending_, nullptr); }
    return *this;
}
void GameByteWriteReservation::Reset() noexcept
{
    if (!pending_) return;
    auto& token = *static_cast<PendingBytes*>(pending_);
    if (!token.completed.empty())
    {
        auto& state = State(); std::lock_guard lock(state.mutex);
        auto owner = state.records.find(token.allocation);
        if (owner != state.records.end() && owner->second.incarnation == token.incarnation)
        {
            auto active = owner->second.pending_writes.find(token.completed.key());
            if (active != owner->second.pending_writes.end() && active->second.tag == token.tag)
                owner->second.pending_writes.erase(active);
        }
    }
    token.~PendingBytes(); ChargedNativeMetadataRelease(pending_); pending_ = nullptr;
}
void GameByteWriteReservation::Complete(GameByteDomain domain)
{
    if (domain != GameByteDomain::WiiSerialized && domain != GameByteDomain::NativeHeader
        && domain != GameByteDomain::NativePayload) throw std::invalid_argument("Unknown byte domain");
    if (!pending_) throw std::logic_error("Byte reservation is inactive");
    auto& token = *static_cast<PendingBytes*>(pending_);
    if (token.completed.empty()) throw std::logic_error("Byte reservation completed twice");
    auto& state = State(); std::lock_guard lock(state.mutex);
    auto owner = state.records.find(token.allocation);
    if (owner == state.records.end() || owner->second.incarnation != token.incarnation)
        throw std::invalid_argument("Byte write belongs to a retired allocation");
    const auto address = token.completed.key();
    auto active = owner->second.pending_writes.find(address);
    if (active == owner->second.pending_writes.end() || active->second.tag != token.tag
        || Containing(state.records, address, active->second.bytes) != owner)
        throw std::invalid_argument("Byte write source ownership changed");
    auto& complete = token.completed.mapped();
    complete.domain = domain;
    if (domain != GameByteDomain::WiiSerialized && token.has_prior_origin)
    {
        complete.logical_base = token.prior_origin.logical_base;
        complete.logical_bytes = token.prior_origin.logical_bytes;
        complete.logical_tag = token.prior_origin.logical_tag;
    }
    auto inserted = owner->second.byte_spans.insert(std::move(token.completed));
    if (!inserted.inserted)
    {
        token.completed = std::move(inserted.node);
        throw std::logic_error("Completed byte domain address conflict");
    }
    owner->second.pending_writes.erase(active);
}
std::uint32_t ReadGameChunkWord(const void* header, unsigned word)
{
    if (word > 1) throw std::invalid_argument("Chunk header has exactly two words");
    auto& state = State(); std::lock_guard lock(state.mutex);
    const auto address = reinterpret_cast<std::uintptr_t>(header);
    auto owner = Containing(state.records, address, 8);
    if (owner == state.records.end()) throw std::invalid_argument("Chunk has no live source allocation");
    auto span = owner->second.byte_spans.upper_bound(address);
    if (span == owner->second.byte_spans.begin()) throw std::invalid_argument("Chunk header has no completed domain");
    --span;
    if (!Contains(span->first, span->second.bytes, address, 8))
        throw std::invalid_argument("Chunk header has no uniform eight-byte domain");
    const auto* bytes = static_cast<const unsigned char*>(header);
    const auto id = HeaderWord(bytes, span->second.domain);
    const auto size = HeaderWord(bytes + 4, span->second.domain);
    const auto extent = std::size_t(size) + 8;
    ByteSpan origin{};
    if (Containing(state.records, address, extent) != owner
        || !Completed(owner->second, address, extent, origin))
        throw std::invalid_argument("Authored chunk leaves completed logical source bytes");
    const auto bits = (id >> 24) & 15;
    const auto alignment = bits ? std::uintptr_t(1) << bits : std::uintptr_t(1);
    const auto payload = (address + 8 + alignment - 1) & ~(alignment - 1);
    if (payload - address - 8 > size) throw std::invalid_argument("Chunk alignment leaves raw extent");
    return word ? size : id;
}
MemoryAllocator* FindGameAllocationOwner(const void* pointer)
{
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto found = state.records.find(reinterpret_cast<std::uintptr_t>(pointer));
    return found == state.records.end() ? nullptr : found->second.owner;
}
void ValidateGameAllocationFree(MemoryAllocator& owner, const void* pointer)
{
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto found = state.records.find(reinterpret_cast<std::uintptr_t>(pointer));
    if (found == state.records.end())
        throw std::invalid_argument("Game free has no exact live allocation record");
    // A source allocator copy can free through the same arena snapshot. This
    // check does not make conflicting mutations of two copies safe or transfer
    // the original game ownership/lifetime to a different allocator object.
    if (found->second.arena != reinterpret_cast<std::uintptr_t>(owner.m_memory)
        || found->second.arena_bytes != owner.m_memory_size)
        throw std::invalid_argument("Game free uses a different allocator arena");
}
void FinishGameAllocationFree(const void* pointer)
{
    auto& state = State();
    std::lock_guard lock(state.mutex);
    auto address = reinterpret_cast<std::uintptr_t>(pointer);
    auto found = state.records.find(address);
    if (found == state.records.end())
        throw std::logic_error("Game free lost its native ownership record");
    EraseBackingRange(state.records, address, found->second.bytes);
}
void DiscardGameAllocatorRecords(MemoryAllocator& owner)
{
    auto& state = State();
    std::lock_guard lock(state.mutex);
    // Initialize replaces the original free-list backing. Walk without holding
    // iterators across descendant retirement; reset is infrequent and allocates
    // no native metadata.
    for (;;)
    {
        auto found = state.records.begin();
        while (found != state.records.end() && found->second.owner != &owner) ++found;
        if (found == state.records.end()) break;
        EraseBackingRange(state.records, found->first, found->second.bytes);
    }
}
void FreeGameAllocation(void* pointer)
{
    if (!pointer) return;
    auto* owner = FindGameAllocationOwner(pointer);
    if (!owner) throw std::invalid_argument("nlFree has no exact live game allocation owner");
    owner->Free(pointer);
}
}
