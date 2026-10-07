#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Only genuine completed WiiSerialized cells; native-created values stay
 * outside this explicit serialized-input boundary. */
uint16_t ChargedReadRFLTextureHalf(const void* source);
/* Original source supplies its unchanged computed pixel byte count. The raw
 * image offset and both exact owner extents are checked before the actual copy.
 * Header/source pixels are never modified. Only copied bytes are completed. */
void ChargedCopyRFLTextureImage(void* destination, const void* texture, size_t bytes);

#ifdef __cplusplus
}
#endif
