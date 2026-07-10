#pragma once

#include "bitmap.h"
#include "scale.h"
#include <vector>
#include <cstddef>

namespace blur {

void gaussianBlur(Bitmap& bitmap, int radius, float sigma = -1.0f);
void gaussianBlur(Bitmap& bitmap, const BlurOptions& options);

void blurHorizontal(const Bitmap& src, Bitmap& dst,
                    const std::vector<float>& kernel, int radius);

void blurVertical(const Bitmap& src, Bitmap& dst,
                  const std::vector<float>& kernel, int radius);

void blurHorizontalScalar(const Bitmap& src, Bitmap& dst,
                          const std::vector<float>& kernel, int radius,
                          int startRow, int endRow);

void blurVerticalScalar(const Bitmap& src, Bitmap& dst,
                        const std::vector<float>& kernel, int radius,
                        int startCol, int endCol);

#if defined(__ARM_NEON__) || defined(__aarch64__)
void blurHorizontalNEON(const Bitmap& src, Bitmap& dst,
                        const std::vector<float>& kernel, int radius,
                        int startRow, int endRow);

void blurVerticalNEON(const Bitmap& src, Bitmap& dst,
                      const std::vector<float>& kernel, int radius,
                      int startCol, int endCol);
#endif

} // namespace blur
