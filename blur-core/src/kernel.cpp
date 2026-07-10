#include "kernel.h"
#include <cmath>
#include <algorithm>

namespace blur {

std::vector<float> generateGaussianKernel(int radius, float sigma) {
    if (radius <= 0) {
        return {1.0f};
    }

    if (sigma <= 0.0f) {
        sigma = static_cast<float>(radius) / 3.0f;
    }

    int size = 2 * radius + 1;
    std::vector<float> kernel(size);

    float sum = 0.0f;
    float twoSigmaSq = 2.0f * sigma * sigma;

    for (int i = -radius; i <= radius; ++i) {
        float value = std::exp(-static_cast<float>(i * i) / twoSigmaSq);
        kernel[static_cast<size_t>(i + radius)] = value;
        sum += value;
    }

    if (sum > 0.0f) {
        for (int i = 0; i < size; ++i) {
            kernel[static_cast<size_t>(i)] /= sum;
        }
    }

    return kernel;
}

} // namespace blur
