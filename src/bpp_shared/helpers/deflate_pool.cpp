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

namespace {

constexpr int K_MIN_LEVEL = 1;
constexpr int K_MAX_LEVEL = 6;

// Upper limit for automatically grown decompression buffers, 16MB
constexpr size_t K_MAX_DECOMPRESSED_SIZE = 16ull * 1024 * 1024;

struct CompressorDeleter {
    void operator()(libdeflate_compressor* _c) const { libdeflate_free_compressor(_c); }
};
struct DecompressorDeleter {
    void operator()(libdeflate_decompressor* _d) const { libdeflate_free_decompressor(_d); }
};

using CompressorPtr   = std::unique_ptr<libdeflate_compressor, CompressorDeleter>;
using DecompressorPtr = std::unique_ptr<libdeflate_decompressor, DecompressorDeleter>;

// One compressor per level per thread, allocated on first use. No locking needed.
// Returns nullptr on OOM.
libdeflate_compressor* GetCompressor(int _level) {
    _level = std::clamp(_level, K_MIN_LEVEL, K_MAX_LEVEL);
    thread_local std::array<CompressorPtr, K_MAX_LEVEL + 1> compressors;
    CompressorPtr& c = compressors[static_cast<size_t>(_level)];
    if (!c)
        c.reset(libdeflate_alloc_compressor(_level));
    return c.get();
}

// One decompressor per thread. Returns nullptr on OOM.
libdeflate_decompressor* GetDecompressor() {
    thread_local DecompressorPtr decompressor;
    if (!decompressor)
        decompressor.reset(libdeflate_alloc_decompressor());
    return decompressor.get();
}

size_t Bound(libdeflate_compressor* _c, Deflate::Format _format, size_t _inputSize) {
    switch (_format) {
    case Deflate::Format::Raw:  return libdeflate_deflate_compress_bound(_c, _inputSize);
    case Deflate::Format::Zlib: return libdeflate_zlib_compress_bound(_c, _inputSize);
    case Deflate::Format::Gzip: return libdeflate_gzip_compress_bound(_c, _inputSize);
    }
    return 0;
}

// Returns the number of bytes written, 0 on failure (including output too small)
size_t CompressInto(libdeflate_compressor* _c, Deflate::Format _format, std::span<const uint8_t> _input,
                    std::span<uint8_t> _output) {
    switch (_format) {
    case Deflate::Format::Raw:
        return libdeflate_deflate_compress(_c, _input.data(), _input.size(), _output.data(), _output.size());
    case Deflate::Format::Zlib:
        return libdeflate_zlib_compress(_c, _input.data(), _input.size(), _output.data(), _output.size());
    case Deflate::Format::Gzip:
        return libdeflate_gzip_compress(_c, _input.data(), _input.size(), _output.data(), _output.size());
    }
    return 0;
}

libdeflate_result DecompressInto(libdeflate_decompressor* _d, Deflate::Format _format,
                                 std::span<const uint8_t> _input, std::span<uint8_t> _output,
                                 size_t& _written) {
    switch (_format) {
    case Deflate::Format::Raw:
        return libdeflate_deflate_decompress(_d, _input.data(), _input.size(),
                                             _output.data(), _output.size(), &_written);
    case Deflate::Format::Zlib:
        return libdeflate_zlib_decompress(_d, _input.data(), _input.size(),
                                          _output.data(), _output.size(), &_written);
    case Deflate::Format::Gzip:
        return libdeflate_gzip_decompress(_d, _input.data(), _input.size(),
                                          _output.data(), _output.size(), &_written);
    }
    return LIBDEFLATE_BAD_DATA;
}

// Gzip stores the uncompressed size (mod 2^32) in the last 4 bytes, little-endian.
// Minimum gzip stream: 10 byte header + 8 byte trailer.
size_t GzipTrailerSize(std::span<const uint8_t> _input) {
    if (_input.size() < 18)
        return 0;
    const uint8_t* p = _input.data() + _input.size() - 4;
    return size_t(p[0]) | (size_t(p[1]) << 8) | (size_t(p[2]) << 16) | (size_t(p[3]) << 24);
}
}

std::vector<uint8_t> Deflate::Compress(const Format _format, std::span<const uint8_t> _input,
                                       const int _level) {
    libdeflate_compressor* c = GetCompressor(_level);
    if (!c)
        return {};

    const size_t bound = Bound(c, _format, _input.size());
    if (bound == 0)
        return {};

    // Compress into a reusable per-thread scratch buffer, then copy out only the bytes
    // actually used, so callers never hold on to a worst-case sized allocation.
    thread_local std::vector<uint8_t> scratch;
    if (scratch.size() < bound)
        scratch.resize(bound);

    const size_t written = CompressInto(c, _format, _input, std::span<uint8_t>(scratch.data(), bound));
    if (written == 0)
        return {};
    return { scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(written) };
}

std::optional<std::vector<uint8_t>> Deflate::Decompress(const Format _format, std::span<const uint8_t> _input,
                                                        size_t _sizeHint) {
    libdeflate_decompressor* d = GetDecompressor();
    if (!d)
        return std::nullopt;

    if (_sizeHint == 0 && _format == Format::Gzip)
        _sizeHint = GzipTrailerSize(_input);
    if (_sizeHint == 0)
        _sizeHint = std::max<size_t>(_input.size() * 4, 4096);

    thread_local std::vector<uint8_t> scratch;
    size_t capacity = std::max<size_t>(_sizeHint, 1);

    while (capacity <= K_MAX_DECOMPRESSED_SIZE) {
        if (scratch.size() < capacity)
            scratch.resize(capacity);

        size_t written = 0;
        const libdeflate_result res =
            DecompressInto(d, _format, _input, std::span<uint8_t>(scratch.data(), capacity), written);

        if (res == LIBDEFLATE_SUCCESS)
            return std::vector<uint8_t>(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(written));
        if (res != LIBDEFLATE_INSUFFICIENT_SPACE)
            return std::nullopt; // Corrupt data, don't keep retrying

        capacity *= 2;
    }
    return std::nullopt;
}

bool Deflate::Decompress(const Format _format, std::span<const uint8_t> _input,
                         std::span<uint8_t> _output, size_t& _written) {
    libdeflate_decompressor* d = GetDecompressor();
    if (!d)
        return false;
    return DecompressInto(d, _format, _input, _output, _written) == LIBDEFLATE_SUCCESS;
}

bool Deflate::Compress(const Format _format, std::span<const uint8_t> _input,
                       std::span<uint8_t> _output, size_t& _written, const int _level) {
    libdeflate_compressor* c = GetCompressor(_level);
    if (!c)
        return false;
    _written = CompressInto(c, _format, _input, _output);
    return _written != 0; // 0 means the output buffer was too small
}

size_t Deflate::CompressBound(const Format _format, const size_t _inputSize, const int _level) {
    libdeflate_compressor* c = GetCompressor(_level);
    if (!c)
        return 0;
    return Bound(c, _format, _inputSize);
}