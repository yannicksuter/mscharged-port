#pragma once

// Retail public SDK headers may include this C++ transport inside extern "C".
extern "C++" {

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace mscharged::platform
{
// TPL fields remain the original four-byte cached-address cells. The native
// decoder checks the actual allocation and completed byte domain before a
// source pointer expression can borrow their native address.
void* DecodeNativeTPLAddress(std::uint32_t word, std::size_t bytes,
                             std::size_t alignment, bool header);

// The original layout compares a raw relative descriptor cell to the Wii
// cached-address boundary before TPLBind. Read that cell in its recorded byte
// domain, without decoding/dereferencing its not-yet-relocated target.
bool NativeTPLAddressLessThan(const void* cell, std::uintptr_t boundary);

template<class T> struct NativeTPLAddress32
{
    std::uint32_t word;
    T* Get() const
    {
        if constexpr (std::is_void_v<T>)
            return static_cast<T*>(DecodeNativeTPLAddress(word, 1, 1, false));
        else
            return static_cast<T*>(DecodeNativeTPLAddress(
                word, sizeof(T), alignof(T), !std::is_same_v<std::remove_cv_t<T>, char>));
    }
    bool operator<(T* boundary) const
    {
        return NativeTPLAddressLessThan(this, reinterpret_cast<std::uintptr_t>(boundary));
    }
    operator T*() const { return Get(); }
    T* operator->() const { return Get(); }
};
static_assert(sizeof(NativeTPLAddress32<char>) == 4);
static_assert(alignof(NativeTPLAddress32<char>) == 4);
static_assert(std::is_trivially_copyable_v<NativeTPLAddress32<char>>);
static_assert(std::is_standard_layout_v<NativeTPLAddress32<char>>);

// Original nonmip I4/IA4/IA8/RGB5A3 TPL header/address transport. Pixel bytes
// remain tiled Wii data for the original GX requests and NAND banner copies.
void BindNativeTPLImage(void* palette);
}

} // C++ linkage

// The original partial TPL declarations use an opaque native parameter only at
// this library boundary. Their palette/table/header strides stay Wii-sized.
extern "C" void TPLBind(void* palette);
