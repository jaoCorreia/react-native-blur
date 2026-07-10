#pragma once

#include "bitmap.h"
#include <cstdint>

namespace blur {

enum class ScaleMode {
    None = 0,
    Auto = 1,
};

enum class BlurMethod {
    Auto = 0,
    Gaussian = 1,
    Box3Pass = 2,
};

struct BlurOptions {
    int radius = 0;
    float sigma = -1.0f;
    ScaleMode scaleMode = ScaleMode::Auto;
    BlurMethod method = BlurMethod::Auto;
};

void downscale2x(const Bitmap& src, Bitmap& dst);
void upscale2x(const Bitmap& src, Bitmap& dst);

int autoScaleFactor(int radius);

} // namespace blur
