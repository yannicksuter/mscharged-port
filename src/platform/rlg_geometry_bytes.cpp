#include "platform/rlg_geometry_bytes.h"
#include "platform/game_allocation_ownership.h"
#include "platform/host_metadata.h"
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace mscharged::platform
{
namespace
{
void RequireWiiCopy(const void* source, std::size_t bytes)
{
    if (FindGameByteDomain(source, bytes) != GameByteDomain::WiiSerialized)
        throw std::invalid_argument("RLG source copy requires completed owned Wii bytes");
}
struct WeightRows
{
    const void* raw;
    std::size_t bytes;
    GameCompletedSpan source;
    GameByteWriteReservation write;
    float* Data() noexcept { return reinterpret_cast<float*>(this + 1); }
};
static_assert(sizeof(float) == sizeof(std::uint32_t));
static_assert(std::numeric_limits<float>::is_iec559);
static_assert(alignof(WeightRows) >= alignof(float));
}
void CopyRLGWireVertices(void* output, const void* source, std::size_t bytes)
{
    if (!bytes) return;
    RequireWiiCopy(source, bytes);
    GameByteWriteReservation write(output, bytes);
    std::memcpy(output, source, bytes);
    write.Complete(GameByteDomain::WiiSerialized);
}
void CopyRLGWireSkin(void* output, const void* source, std::size_t bytes)
{
    // The caller retains its exact original sizeof(nlChunk)+GetDataSize request.
    CopyRLGWireVertices(output, source, bytes);
}
void CopyRLGNativeIndices(void* output, const void* source, std::size_t bytes)
{
    if (!bytes) return;
    RequireWiiCopy(source, bytes);
    GameByteWriteReservation write(output, bytes);
    const auto* raw = static_cast<const unsigned char*>(source);
    auto* native = static_cast<unsigned char*>(output);
    for (std::size_t offset = 0; offset + 1 < bytes; offset += sizeof(std::uint16_t))
    {
        const auto value = std::uint16_t((std::uint16_t(raw[offset]) << 8) | raw[offset + 1]);
        std::memcpy(native + offset, &value, sizeof(value));
    }
    // Original memcpy copies an odd terminal byte too; do not reject, round or
    // manufacture an extra index cell when preserving that raw size quirk.
    if (bytes & 1) native[bytes - 1] = raw[bytes - 1];
    write.Complete(GameByteDomain::NativePayload);
}
void CopyRLGNativeMatrices(void* output, const void* source, std::size_t bytes)
{
    if (!bytes) return;
    if (bytes % 64)
        throw std::invalid_argument("RLG matrix copy requires complete original 64-byte records");
    RequireWiiCopy(source, bytes);
    GameByteWriteReservation write(output, bytes);
    if (!write.Tracked())
        throw std::invalid_argument("RLG matrix output has no genuine original owner");
    const auto* raw = static_cast<const unsigned char*>(source);
    auto* native = static_cast<unsigned char*>(output);
    for (std::size_t offset = 0; offset < bytes; offset += sizeof(std::uint32_t))
    {
        const std::uint32_t bits = (std::uint32_t(raw[offset]) << 24)
            | (std::uint32_t(raw[offset + 1]) << 16)
            | (std::uint32_t(raw[offset + 2]) << 8) | raw[offset + 3];
        // No float arithmetic: retain authored zero, infinity and NaN bits.
        std::memcpy(native + offset, &bits, sizeof(bits));
    }
    write.Complete(GameByteDomain::NativeHeader);
}

NativeRLGWeightRows::NativeRLGWeightRows(const void* source, std::size_t vertices,
    RLGWeightAccess access) : state_(nullptr)
{
    if (!vertices) return;
    if (access != RLGWeightAccess::ReadOnly && access != RLGWeightAccess::ReadWrite)
        throw std::invalid_argument("Unknown RLG weight access domain");
    if (vertices > (std::numeric_limits<std::size_t>::max() - sizeof(WeightRows)) / 16)
        throw std::length_error("Original RLG weight row byte extent overflows");
    const std::size_t bytes = vertices * 16;
    GameCompletedSpan span{};
    if (!FindGameCompletedSpan(source, bytes, span))
        throw std::invalid_argument("RLG weight rows leave their actual completed logical span");
    RequireWiiCopy(source, bytes);
    if (access == RLGWeightAccess::ReadWrite)
    {
        // The original index writes share this completed vertex-copy span.
        // Preserve its full logical extent when republishing the source swaps.
        GameCompletedSpan complete{};
        if (!FindGameCompletedSpan(span.base, span.bytes, complete)
            || complete.base != span.base || complete.bytes != span.bytes)
            throw std::invalid_argument("RLG weight rewrite has no complete original vertex copy");
        RequireWiiCopy(span.base, span.bytes);
    }
    void* storage = ChargedNativeMetadataAllocate(sizeof(WeightRows) + bytes);
    auto* rows = new (storage) WeightRows{source, bytes, span, {}};
    try
    {
        const auto* raw = static_cast<const unsigned char*>(source);
        for (std::size_t at = 0; at < bytes; at += 4)
        {
            const std::uint32_t bits = (std::uint32_t(raw[at]) << 24)
                | (std::uint32_t(raw[at + 1]) << 16)
                | (std::uint32_t(raw[at + 2]) << 8) | raw[at + 3];
            std::memcpy(reinterpret_cast<unsigned char*>(rows->Data()) + at, &bits, 4);
        }
        if (access == RLGWeightAccess::ReadWrite)
            rows->write = GameByteWriteReservation(const_cast<void*>(span.base), span.bytes);
        state_ = rows;
    }
    catch (...)
    {
        rows->~WeightRows();
        ChargedNativeMetadataRelease(storage);
        throw;
    }
}

NativeRLGWeightRows::~NativeRLGWeightRows()
{
    if (!state_) return;
    auto* rows = static_cast<WeightRows*>(state_);
    rows->~WeightRows();
    ChargedNativeMetadataRelease(rows);
}

float* NativeRLGWeightRows::Data() const noexcept
{
    return state_ ? static_cast<WeightRows*>(state_)->Data() : nullptr;
}

void NativeRLGWeightRows::StoreWire()
{
    if (!state_) return;
    auto& rows = *static_cast<WeightRows*>(state_);
    if (!rows.write.Tracked())
        throw std::logic_error("RLG weight view has no original rewrite reservation");
    GameAllocationSpan owner{};
    if (!FindGameAllocationSpan(rows.raw, rows.bytes, owner)
        || owner.base != rows.source.allocation.base
        || owner.owner != rows.source.allocation.owner
        || owner.incarnation != rows.source.allocation.incarnation)
        throw std::invalid_argument("RLG weight source allocation was retired or reused");
    auto* output = static_cast<unsigned char*>(const_cast<void*>(rows.raw));
    const auto* input = reinterpret_cast<const unsigned char*>(rows.Data());
    for (std::size_t at = 0; at < rows.bytes; at += 4)
    {
        std::uint32_t bits;
        std::memcpy(&bits, input + at, 4);
        output[at] = static_cast<unsigned char>(bits >> 24);
        output[at + 1] = static_cast<unsigned char>(bits >> 16);
        output[at + 2] = static_cast<unsigned char>(bits >> 8);
        output[at + 3] = static_cast<unsigned char>(bits);
    }
    rows.write.Complete(GameByteDomain::WiiSerialized);
    rows.write.Reset();
}

}
