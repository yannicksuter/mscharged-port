#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Exact live MEM1 span only. Native module statics/MEM2 require their real
// shared physical-address ownership service; no truncation/token fallback.
uint32_t ChargedDSPTaskMemoryWord(const void* address, uint32_t bytes, int device_writes);
#ifdef __cplusplus
}
#endif
