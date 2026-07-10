// Exercises blur-core purely as an installed package (via find_package),
// outside the React Native / Gradle build, to prove the CMake package
// config actually works end to end rather than just "looking right".
#include "kernel.h"
#include <cstdio>

int main() {
    auto kernel = blur::generateGaussianKernel(3);
    std::printf("kernel size = %zu\n", kernel.size());
    return kernel.empty() ? 1 : 0;
}
