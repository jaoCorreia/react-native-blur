#include "kernel.h"
#include <gtest/gtest.h>
#include <cmath>

using namespace blur;

TEST(KernelTest, ZeroRadiusReturnsSingleElement) {
    auto kernel = generateGaussianKernel(0);
    EXPECT_EQ(kernel.size(), 1u);
    EXPECT_NEAR(kernel[0], 1.0f, 1e-6f);
}

TEST(KernelTest, NegativeRadiusReturnsSingleElement) {
    auto kernel = generateGaussianKernel(-5);
    EXPECT_EQ(kernel.size(), 1u);
    EXPECT_NEAR(kernel[0], 1.0f, 1e-6f);
}

TEST(KernelTest, KernelSumEqualsOne) {
    for (int radius = 1; radius <= 10; ++radius) {
        auto kernel = generateGaussianKernel(radius);
        float sum = 0.0f;
        for (float v : kernel) sum += v;
        EXPECT_NEAR(sum, 1.0f, 1e-5f)
            << "Kernel sum != 1 for radius=" << radius;
    }
}

TEST(KernelTest, KernelIsSymmetric) {
    for (int radius = 1; radius <= 10; ++radius) {
        auto kernel = generateGaussianKernel(radius);
        int size = static_cast<int>(kernel.size());
        for (int i = 0; i < size / 2; ++i) {
            EXPECT_NEAR(kernel[i], kernel[size - 1 - i], 1e-6f)
                << "Asymmetry at i=" << i << " for radius=" << radius;
        }
    }
}

TEST(KernelTest, CenterValueIsLargest) {
    for (int radius = 1; radius <= 10; ++radius) {
        auto kernel = generateGaussianKernel(radius);
        int center = radius;
        for (int i = 0; i < static_cast<int>(kernel.size()); ++i) {
            EXPECT_GE(kernel[center], kernel[i])
                << "Center not largest at i=" << i << " radius=" << radius;
        }
    }
}

TEST(KernelTest, LargerSigmaProducesFlatterKernel) {
    auto kernelSmall = generateGaussianKernel(5, 1.0f);
    auto kernelLarge = generateGaussianKernel(5, 30.0f);

    int center = 5;

    EXPECT_GT(kernelSmall[center], kernelLarge[center])
        << "Small sigma should have larger center weight";

    EXPECT_GT(kernelLarge[1], kernelSmall[1])
        << "Large sigma should have larger edge weight";
}

TEST(KernelTest, CorrectSize) {
    for (int r = 0; r <= 8; ++r) {
        EXPECT_EQ(generateGaussianKernel(r).size(), 2u * static_cast<size_t>(r) + 1u);
    }
}

TEST(KernelTest, DefaultSigma) {
    auto k1 = generateGaussianKernel(3);
    auto k2 = generateGaussianKernel(3, 1.0f);
    EXPECT_FLOAT_EQ(k1[0], k2[0])
        << "Default sigma (r/3) should match explicit sigma=1 for r=3";
}
