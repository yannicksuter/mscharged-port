#pragma once

namespace mscharged::diagnostic {

// Selected original Credits/MoviePlayer diagnostic only. The original source
// owns THPSimple initialization, reads, decoding, mixing and playback decisions.
// This endpoint owns the one host AI device and its owner-thread safe points.
void InitializeCreditsMovieHardware();
void ServiceCreditsMovieHardware();
void ShutdownCreditsMovieHardware();

} // namespace mscharged::diagnostic
