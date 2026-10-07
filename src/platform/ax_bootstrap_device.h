#pragma once
#include "platform/dsp_control.h"
#include "platform/dsp_memory.h"
#include <array>
#include <cstdint>
#include <memory>

namespace mscharged::platform {
class DSPInstructionCore;
enum class NativeAXBootstrapPhase { Cold, LoaderReady, Loading, InitPrefixCompleted, Faulted, Retired };
struct NativeAXBootstrapStatus {
    NativeAXBootstrapPhase phase;
    std::uint16_t loader_words;
    std::uint64_t firmware_instructions, resets;
    bool hardware_halted;
};
enum class NativeAXFrameMode { BootstrapOnly, StoppedVoices };
enum class NativeAXFramePhase {
    Unavailable, ReadyForListSize, ReadyForListAddress,
    WaitingSyncAcknowledgment, WaitingContinue, Faulted
};
struct NativeAXStoppedVoiceStatus {
    NativeAXFramePhase phase;
    std::uint64_t processed_frames, completed_frames;
    std::uint64_t sync_interrupts, yield_interrupts, source_continues;
    std::uint32_t last_list_address, written_bytes;
    std::uint16_t stopped_voices, stereo_frames, remote_samples;
};
// Explicit processor conformance endpoint, NOT a full AX kernel or production
// ready service. All callers retain actual source-image/pin/SDK lifetimes.
// Only the exact8192-byte source firmware and its ten-word loader are accepted.
// Its real21instruction init prefix emits INIT+DIRQ; original source __DSPHandler
// owns all task flags/callbacks. The default still rejects every frame request.
// Explicit StoppedVoices mode implements native hardware command semantics
// for the qualified zero-input slice only, not full DSP firmware execution.
// Active voices/AUX/nonzero inputs remain errors, never silence or readiness.
class NativeAXBootstrapDevice {
public:
    NativeAXBootstrapDevice(NativeDSPMemoryEndpoint memory,
                            NativeDSPMailboxEndpoint mailboxes,
                            NativeDSPControlEndpoint control,
                            std::uint32_t firmware_address);
    NativeAXBootstrapDevice(NativeDSPMemoryEndpoint memory,
                            NativeDSPMailboxEndpoint mailboxes,
                            NativeDSPControlEndpoint control,
                            std::uint32_t firmware_address,
                            NativeAXFrameMode frame_mode);
    // Adopt the same halted chip after original OS boot. Reset/loader overwrite
    // instruction execution only; actual cold DRAM and supplied ROMs survive.
    NativeAXBootstrapDevice(NativeDSPMemoryEndpoint memory,
                            NativeDSPMailboxEndpoint mailboxes,
                            NativeDSPControlEndpoint control,
                            std::uint32_t firmware_address,
                            NativeAXFrameMode frame_mode,DSPInstructionCore& retained_chip);
    ~NativeAXBootstrapDevice();
    NativeAXBootstrapDevice(const NativeAXBootstrapDevice&)=delete;
    NativeAXBootstrapDevice& operator=(const NativeAXBootstrapDevice&)=delete;
    void ServiceOwner();
    NativeAXBootstrapStatus Status() const;
    NativeAXStoppedVoiceStatus FrameStatus() const;
    // Read actual initialized master/AUX words; reject cold/loading/fault/reset.
    // Does not expose or invent compressor history or command/kernel readiness.
    std::array<std::uint16_t,4> InitializedGainWords() const;
    // Requires actual HALT; no thread/job/source flags are repaired here.
    void Close();
private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace mscharged::platform
