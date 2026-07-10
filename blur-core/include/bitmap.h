#pragma once

#include <cstdint>
#include <cstddef>

namespace blur {

struct Bitmap {
    uint8_t* pixels = nullptr;
    int width = 0;
    int height = 0;
    int stride = 0;
    int channels = 4;

    bool isValid() const {
        return pixels != nullptr && width > 0 && height > 0 && stride > 0;
    }

    uint8_t* getRow(int y) const {
        return pixels + static_cast<ptrdiff_t>(y) * stride;
    }

    const uint8_t* getRowConst(int y) const {
        return pixels + static_cast<ptrdiff_t>(y) * stride;
    }

    size_t totalBytes() const {
        return static_cast<size_t>(height) * stride;
    }
};

} // namespace blur
