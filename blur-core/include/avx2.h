#pragma once

#include "bitmap.h"
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)

namespace blur {

// Runtime cpuid check. Android's x86_64 NDK ABI baseline is SSE4.2, not
// AVX2, so this must be checked before ever calling the AVX2 functions below
// — calling them unconditionally would be an illegal-instruction crash on
// real x86_64 devices/emulators that don't happen to expose AVX2.
bool cpuSupportsAVX2();

void blurHorizontalAVX2(const Bitmap& src, Bitmap& dst,
                        const std::vector<float>& kernel, int radius,
                        int startRow, int endRow);

void blurVerticalAVX2(const Bitmap& src, Bitmap& dst,
                      const std::vector<float>& kernel, int radius,
                      int startCol, int endCol);

} // namespace blur

#endif
