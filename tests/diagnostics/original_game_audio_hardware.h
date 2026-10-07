#pragma once
#include "platform/ax_functional_device.h"
#include "platform/native_ax_module_memory.h"
#include <memory>

namespace mscharged::diagnostic {
// Original source owns AIInit/AXInit/MIXInit. Construct only after the actual
// module's priority101 hook reserved its thirteen spans and ConfirmLoaded ran.
// No boot request, source flag or callback is issued by this attachment.
class OriginalGameAudioHardware {
public:
    explicit OriginalGameAudioHardware(platform::NativeAXModuleMemory& memory);
    ~OriginalGameAudioHardware();
    OriginalGameAudioHardware(const OriginalGameAudioHardware&) = delete;
    OriginalGameAudioHardware& operator=(const OriginalGameAudioHardware&) = delete;
    platform::NativeAXFunctionalStatus Status() const;
    // After original source shutdown, stop borrowing the hardware owner before
    // that owner stops/drains AI. No source flag/callback or device request follows.
    void UnbindOwnerService();
    // Original THP Quit and source AudioSystem::Shutdown must already return,
    // then the sole AI endpoint must stop/drain. The module first loader handle
    // and original task/CRT owners remain live through terminal exit.
    void CloseAfterAIStop(void (*retire_silence_pin)());
private:
    static void ServiceDevice(void* context);
    bool service_bound_{};
    platform::NativeAXModuleMemory& memory_;
    std::unique_ptr<platform::NativeAXFunctionalDevice> device_;
};
} // namespace mscharged::diagnostic
