#pragma once

#include <vector>
#include <cstddef>

namespace blur {

std::vector<float> generateGaussianKernel(int radius, float sigma = -1.0f);

} // namespace blur
