#pragma once

#include "bitmap.h"
#include <cstdint>

namespace blur {

int boxRadiusFromGaussian(int gaussianRadius);

void boxBlur1D(const Bitmap& src, Bitmap& dst, int radius);

void boxBlur3Pass(Bitmap& bitmap, int gaussianRadius);

} // namespace blur
