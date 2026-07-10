#include "kernel.h"
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <mutex>

namespace blur {

static std::unordered_map<int, std::vector<float>> kernelCache;
static std::mutex kernelCacheMutex;

std::vector<float> generateGaussianKernel(int radius, float sigma) {
    if (radius <= 0) {
        return {1.0f};
    }

    if (sigma <= 0.0f) {
        sigma = static_cast<float>(radius) / 3.0f;
    }

    int cacheKey = (radius << 16) | static_cast<int>(sigma * 100.0f + 0.5f);

    {
        std::lock_guard<std::mutex> lock(kernelCacheMutex);
        auto it = kernelCache.find(cacheKey);
        if (it != kernelCache.end()) {
            return it->second;
        }
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

    {
        std::lock_guard<std::mutex> lock(kernelCacheMutex);
        kernelCache[cacheKey] = kernel;
    }

    return kernel;
}

} // namespace blur
