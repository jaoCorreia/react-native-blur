#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <unordered_map>

namespace blur {

class BufferPool {
public:
    static BufferPool& instance();

    std::vector<uint8_t> acquire(size_t sizeInBytes);
    void release(std::vector<uint8_t>& buffer);
    void clear();

private:
    BufferPool() = default;

    struct ThreadCache {
        std::unordered_map<size_t, std::vector<uint8_t>> buckets;
    };

    static ThreadCache& threadCache();
};

class ScopedBuffer {
public:
    explicit ScopedBuffer(size_t size)
        : data(BufferPool::instance().acquire(size)) {}

    ~ScopedBuffer() {
        BufferPool::instance().release(data);
    }

    uint8_t* get() { return data.data(); }
    const uint8_t* get() const { return data.data(); }
    size_t size() const { return data.size(); }

    ScopedBuffer(const ScopedBuffer&) = delete;
    ScopedBuffer& operator=(const ScopedBuffer&) = delete;

private:
    std::vector<uint8_t> data;
};

} // namespace blur
