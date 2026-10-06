#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

// Storage descriptions only: actual original AX statics, before AX initialization.
// The host must retain their containing image and reserve physical backing before
// the original game captures the remaining arena. No source state is initialized.
typedef struct ChargedAXStorage {
    void* address;
    uint32_t bytes;
} ChargedAXStorage;
ChargedAXStorage ChargedAXGetCommandStorage(void);
void ChargedAXGetVoiceStorage(ChargedAXStorage output[2]); // AXPB, opaque ITD.
void ChargedAXGetAuxStorage(ChargedAXStorage output[3]);
ChargedAXStorage ChargedAXGetCompressorStorage(void);

#ifdef __cplusplus
}
#endif
