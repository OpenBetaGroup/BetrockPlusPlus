/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include "../bpp_server/chunk_io/chunk_serializer.h"
#include "constants.h"
#include "world/chunk.h"

#include <chrono>
#include <cstddef>
#include <iostream>
#include <vector>

namespace ChunkBenchmark {

struct Result {
	size_t rawBytes = 0;
	size_t compressedBytes = 0;
	double seconds = 0.0;

	double Ratio() const {
		return rawBytes ? double(compressedBytes) / double(rawBytes) : 0.0;
	}

	double Savings() const {
		return 1.0 - Ratio();
	}

	double MBPerSecond() const {
		return seconds > 0.0 ? double(rawBytes) / (1024.0 * 1024.0) / seconds : 0.0;
	}
};

inline Result Benchmark(const std::vector<Chunk*>& chunks, int iterations = 1) {
	Result result;

	// Warmup (avoids measuring first-time allocations/cache effects)
	for (Chunk* chunk : chunks)
		ChunkSerializer::Serialize(*chunk);

	auto start = std::chrono::steady_clock::now();

	for (int i = 0; i < iterations; i++) {
		for (Chunk* chunk : chunks) {
			// This is the uncompressed size produced by Serialize internally:
			constexpr size_t rawChunkSize = (16 * CHUNK_HEIGHT * 16) + (((16 * CHUNK_HEIGHT * 16) + 1) / 2) * 3;

			auto compressed = ChunkSerializer::Serialize(*chunk);

			result.rawBytes += rawChunkSize;
			result.compressedBytes += compressed.size();
		}
	}

	auto end = std::chrono::steady_clock::now();

	result.seconds = std::chrono::duration<double>(end - start).count();

	return result;
}

inline void Print(const Result& result) {
	std::cout << "Raw:          " << result.rawBytes / (1024.0 * 1024.0) << " MB\n"

	          << "Compressed:   " << result.compressedBytes / (1024.0 * 1024.0) << " MB\n"

	          << "Ratio:        " << result.Ratio() * 100.0 << "%\n"

	          << "Saved:        " << result.Savings() * 100.0 << "%\n"

	          << "Speed:        " << result.MBPerSecond() << " MB/s\n";
}

} // namespace ChunkBenchmark