#pragma once
#include <cstddef>
#include <cstdint>
class nlChunk;
namespace mscharged::platform {
struct NativeAnimRetargetView {
    nlChunk* source{};
    std::size_t source_bytes{};
    void* data{};
    std::size_t native_bytes{};
    std::uint64_t incarnation{};
};
// Only completed original serialized bytes with their retained source owner.
// Native backing shares that incarnation and grants no retained-reference lease.
NativeAnimRetargetView PrepareNativeAnimRetarget(nlChunk* source);
void* NativeAnimRetargetChunkData(const NativeAnimRetargetView&, nlChunk* child);
}
