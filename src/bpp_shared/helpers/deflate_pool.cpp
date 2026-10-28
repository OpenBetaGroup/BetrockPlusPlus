/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */

#include "deflate_pool.h"
#include <libdeflate.h>

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
#include <vector>

namespace {

constexpr int K_MIN_LEVEL = 0;
constexpr int K_MAX_LEVEL = 12;

struct CompressorDeleter {
    void operator()(libdeflate_compressor* _c) const { libdeflate_free_compressor(_c); }
};
struct DecompressorDeleter {
    void operator()(libdeflate_decompressor* _d) const { libdeflate_free_decompressor(_d); }
};

using CompressorPtr   = std::unique_ptr<libdeflate_compressor, CompressorDeleter>;
using DecompressorPtr = std::unique_ptr<libdeflate_decompressor, DecompressorDeleter>;

// TODO: This feels a bit overkill?
class CompressorPool {
public:
    class Lease {
    public:
        Lease(CompressorPool& _pool, int _level, CompressorPtr _ptr)
            : pool(&_pool), level(_level), ptr(std::move(_ptr)) {}
        Lease(Lease&&) noexcept = default;
        Lease& operator=(Lease&&) = delete;
        ~Lease() { if (ptr) pool->Release(level, std::move(ptr)); }

        libdeflate_compressor* Get() const { return ptr.get(); }
        explicit operator bool() const { return ptr != nullptr; }

    private:
        CompressorPool* pool;
        int level;
        CompressorPtr ptr;
    };

    Lease Acquire(int _level) {
        _level = std::clamp(_level, K_MIN_LEVEL, K_MAX_LEVEL);
        {
            std::lock_guard lock(mutex);
            auto& list = free[_level];
            if (!list.empty()) {
                CompressorPtr p = std::move(list.back());
                list.pop_back();
                return Lease(*this, _level, std::move(p));
            }
        }
        // Allocate outside the lock; may return nullptr on OOM.
        return Lease(*this, _level, CompressorPtr(libdeflate_alloc_compressor(_level)));
    }

private:
    void Release(int _level, CompressorPtr _ptr) {
        std::lock_guard lock(mutex);
        free[_level].push_back(std::move(_ptr));
    }

    std::mutex mutex;
    std::array<std::vector<CompressorPtr>, K_MAX_LEVEL + 1> free;
};

class DecompressorPool {
public:
    class Lease {
    public:
        Lease(DecompressorPool& _pool, DecompressorPtr _ptr)
            : pool(&_pool), ptr(std::move(_ptr)) {}
        Lease(Lease&&) noexcept = default;
        Lease& operator=(Lease&&) = delete;
        ~Lease() { if (ptr) pool->Release(std::move(ptr)); }

        libdeflate_decompressor* Get() const { return ptr.get(); }
        explicit operator bool() const { return ptr != nullptr; }

    private:
        DecompressorPool* pool;
        DecompressorPtr ptr;
    };

    Lease Acquire() {
        {
            std::lock_guard lock(mutex);
            if (!free.empty()) {
                DecompressorPtr p = std::move(free.back());
                free.pop_back();
                return Lease(*this, std::move(p));
            }
        }
        return Lease(*this, DecompressorPtr(libdeflate_alloc_decompressor()));
    }

private:
    void Release(DecompressorPtr _ptr) {
        std::lock_guard lock(mutex);
        free.push_back(std::move(_ptr));
    }

    std::mutex mutex;
    std::vector<DecompressorPtr> free;
};

CompressorPool   compressorPool;
DecompressorPool decompressorPool;

}

bool Deflate::Decompress(const Format _format, std::span<const uint8_t> _input,
                         std::span<uint8_t> _output, size_t& _written) {
    auto lease = decompressorPool.Acquire();
    if (!lease) return false;

    libdeflate_result res;
    switch (_format) {
    case Format::Raw:
        res = libdeflate_deflate_decompress(lease.Get(), _input.data(), _input.size(),
                                            _output.data(), _output.size(), &_written);
        break;
    case Format::Zlib:
        res = libdeflate_zlib_decompress(lease.Get(), _input.data(), _input.size(),
                                         _output.data(), _output.size(), &_written);
        break;
    case Format::Gzip:
        res = libdeflate_gzip_decompress(lease.Get(), _input.data(), _input.size(),
                                         _output.data(), _output.size(), &_written);
        break;
    default:
        return false;
    }
    return res == LIBDEFLATE_SUCCESS;
}

bool Deflate::Compress(const Format _format, std::span<const uint8_t> _input,
                       std::span<uint8_t> _output, size_t& _written, const int _level) {
    auto lease = compressorPool.Acquire(_level);
    if (!lease) return false;

    switch (_format) {
    case Format::Raw:
        _written = libdeflate_deflate_compress(lease.Get(), _input.data(), _input.size(),
                                               _output.data(), _output.size());
        break;
    case Format::Zlib:
        _written = libdeflate_zlib_compress(lease.Get(), _input.data(), _input.size(),
                                            _output.data(), _output.size());
        break;
    case Format::Gzip:
        _written = libdeflate_gzip_compress(lease.Get(), _input.data(), _input.size(),
                                            _output.data(), _output.size());
        break;
    default:
        return false;
    }
    return _written != 0; // 0 means the output buffer was too small
}

size_t Deflate::CompressBound(const Format _format, const size_t _inputSize, const int _level) {
    auto lease = compressorPool.Acquire(_level);
    if (!lease) return 0;

    switch (_format) {
    case Format::Raw:  return libdeflate_deflate_compress_bound(lease.Get(), _inputSize);
    case Format::Zlib: return libdeflate_zlib_compress_bound(lease.Get(), _inputSize);
    case Format::Gzip: return libdeflate_gzip_compress_bound(lease.Get(), _inputSize);
    }
    return 0;
}