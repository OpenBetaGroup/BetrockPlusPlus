/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Deflate {
    enum class Format { Raw, Zlib, Gzip };
    // For one-off buffers
    std::vector<uint8_t> Compress(Format _format, std::span<const uint8_t> _input, int _level = 6);
    std::optional<std::vector<uint8_t>> Decompress(Format _format, std::span<const uint8_t> _input, size_t _sizeHint = 0);
    // For re-using buffers
    bool Decompress(Format _format, std::span<const uint8_t> _input, std::span<uint8_t> _output, size_t& _written);
    bool Compress(Format _format, std::span<const uint8_t> _input, std::span<uint8_t> _output, size_t& _written, int _level = 6);
    size_t CompressBound(Format _format, size_t _inputSize, int _level = 6);
}