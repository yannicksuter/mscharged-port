#include "NL/nlDLListContainer.inl"
#include "Game/Audio/AudioSystem.inl"
#include "Game/Audio/AudioBundleManager.h"
#include "Game/Audio/AudioGlobals.h"
#include "platform/game_allocation_ownership.h"
#include <stdexcept>

// Read actual source predicates only. Backend-initialized and completed bundle
// loading are separate; source Shutdown deliberately does not clear either.
extern "C" __attribute__((visibility("default"))) unsigned charged_original_audio_observe() {
    if (!g_pAudioSystem) return 0;
    return 1u | (g_pAudioSystem->IsInitialized() ? 2u : 0u) |
        (g_pAudioSystem->GetBundleManager() && g_pAudioSystem->GetBundleManager()->IsLoaded() ? 4u : 0u);
}

extern "C" __attribute__((visibility("default"))) void charged_original_audio_shutdown() {
    if (charged_original_audio_observe() != 7u)
        throw std::logic_error("Original audio shutdown requires its genuinely loaded source owner");
    g_pAudioSystem->Shutdown();
}

// Only the native device lease is retired. The original allocation, source
// pointer and initialized flags remain untouched. Called after AI/HALT/Close.
extern "C" __attribute__((visibility("default"))) void charged_original_audio_retire_silence_pin() {
    mscharged::platform::RetireGameAllocationDevicePins(g_pAudioSilenceBuffer);
}
