#pragma once
#include "platform/dsp_control.h"
#include "platform/dsp_memory.h"
#include <cstdint>
#include <memory>

namespace mscharged::platform {
enum class NativeAXBootstrapPhase { Cold, LoaderReady, Loading, InitPrefixCompleted, Faulted, Retired };
struct NativeAXBootstrapStatus {
    NativeAXBootstrapPhase phase;
    std::uint16_t loader_words;
    std::uint64_t firmware_instructions, resets;
    bool hardware_halted;
};
// Explicit processor conformance endpoint, NOT a full AX kernel or production
// ready service. All callers retain actual source-image/pin/SDK lifetimes.
// Only the exact8192-byte source firmware and its ten-word loader are accepted.
// Its real21instruction init prefix emits INIT+DIRQ; original source __DSPHandler
// owns all task flags/callbacks. No command/frame/active voice is accepted yet.
class NativeAXBootstrapDevice {
public:
    NativeAXBootstrapDevice(NativeDSPMemoryEndpoint memory,
                            NativeDSPMailboxEndpoint mailboxes,
                            NativeDSPControlEndpoint control,
                            std::uint32_t firmware_address);
    ~NativeAXBootstrapDevice();
    NativeAXBootstrapDevice(const NativeAXBootstrapDevice&)=delete;
    NativeAXBootstrapDevice& operator=(const NativeAXBootstrapDevice&)=delete;
    void ServiceOwner();
    NativeAXBootstrapStatus Status() const;
    // Requires actual HALT; no thread/job/source flags are repaired here.
    void Close();
private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace mscharged::platform
