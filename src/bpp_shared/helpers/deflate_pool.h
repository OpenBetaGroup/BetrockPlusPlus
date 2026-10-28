/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */

#pragma once
#include <cstddef>
#include <span>

namespace Deflate {
    enum class Format { Raw, Zlib, Gzip };

    // Output buffer must be large enough for the full decompressed data, otherwise returns false
    bool Decompress(Format _format, std::span<const uint8_t> _input,
                    std::span<uint8_t> _output, size_t& _written);

    // Compresses data, returns false if the output buffer is too small
    bool Compress(Format _format, std::span<const uint8_t> _input,
                  std::span<uint8_t> _output, size_t& _written, int _level = 1);

    // Worst-case compressed size, for sizing the output buffer
    size_t CompressBound(Format _format, size_t _inputSize, int _level = 1);
}