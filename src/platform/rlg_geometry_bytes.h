#pragma once
#include <cstddef>
namespace mscharged::platform
{
// Preserve opaque Wii GPU vertex bytes and publish their real completed source
// copy. Index cells are numeric u16 consumed by original CPU/display-list code.
void CopyRLGWireVertices(void* output, const void* source, std::size_t bytes);
void CopyRLGWireSkin(void* output, const void* source, std::size_t bytes);
void CopyRLGNativeIndices(void* output, const void* source, std::size_t bytes);
// Original GLMatrix records are sixteen numeric float words consumed by the CPU.
// The caller retains its original 64-byte size mask and owning allocation.
void CopyRLGNativeMatrices(void* output, const void* source, std::size_t bytes);

enum class RLGWeightAccess { ReadOnly, ReadWrite };
// Only original CPU float consumers use this scoped row view. The original
// loops still compare and swap; StoreWire encodes their result back into the
// same owned Wii vertex bytes, without converting the GPU stream or stride.
class NativeRLGWeightRows
{
public:
    NativeRLGWeightRows(const void* source, std::size_t vertices, RLGWeightAccess access);
    ~NativeRLGWeightRows();
    NativeRLGWeightRows(const NativeRLGWeightRows&) = delete;
    NativeRLGWeightRows& operator=(const NativeRLGWeightRows&) = delete;
    float* Data() const noexcept;
    void StoreWire();
private:
    void* state_;
};
}
