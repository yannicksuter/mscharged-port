#pragma once

// The reconstructed AudioValues/Owner/Runtime types are prefix views over live
// bundle, calculation-table and slider objects. Derive their native geometry
// from those source classes; serialized audio records keep their own ABI.
#include "Game/Audio/AudioBundleManager.h"
#include "Game/Audio/AudioCalculation.h"
#include <cstddef>

namespace mscharged::platform::audio_settings_abi {

// This abstract descriptor is never instantiated and adds no object fields.
// Access is needed only to name the source class's protected member offset.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif
struct BundleOffsets : AudioBundleManager {
    static constexpr std::size_t CalculationTable() {
        return offsetof(BundleOffsets, m_Chunk13400);
    }
};
inline constexpr std::size_t BundleCalculations = BundleOffsets::CalculationTable();
inline constexpr std::size_t TableSliders = offsetof(AudioCalculationTable, sliders);
inline constexpr std::size_t Target = offsetof(Transition, target);
inline constexpr std::size_t Elapsed = offsetof(Transition, elapsed);
inline constexpr std::size_t Minimum = offsetof(Transition, minimum);
inline constexpr std::size_t Maximum = offsetof(Transition, maximum);
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
inline constexpr std::size_t MusicTarget = 2 * sizeof(AudioCalculationSlider) + Target;
inline constexpr std::size_t BetweenVolumeFields = sizeof(AudioCalculationSlider)
    - (Maximum - Target + sizeof(float));
static_assert(sizeof(BundleOffsets) == sizeof(AudioBundleManager));
static_assert(Elapsed == Target + sizeof(float));
static_assert(Minimum == Elapsed + 2 * sizeof(float));
static_assert(Maximum == Minimum + sizeof(float));

} // namespace mscharged::platform::audio_settings_abi
