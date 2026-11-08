/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../feature_gen.h"

bool FeatureGenerator::GenerateOre(BlockType _type, WorldWrapper& _world, Java::Random& _rand, Int3 _pos,
                                   int32_t _blobSize) {
	float angle = _rand.NextFloat() * JavaMath::PI_FLOAT;
	double xStart = double(float(_pos.x + 8) + MathHelper::Sin(angle) * float(_blobSize) / 8.0F);
	double xEnd = double(float(_pos.x + 8) - MathHelper::Sin(angle) * float(_blobSize) / 8.0F);
	double zStart = double(float(_pos.z + 8) + MathHelper::Cos(angle) * float(_blobSize) / 8.0F);
	double zEnd = double(float(_pos.z + 8) - MathHelper::Cos(angle) * float(_blobSize) / 8.0F);
	double yStart = double(_pos.y + _rand.NextInt(3) + 2);
	double yEnd = double(_pos.y + _rand.NextInt(3) + 2);

	for (int32_t i = 0; i <= _blobSize; ++i) {
		double xC = xStart + (xEnd - xStart) * double(i) / double(_blobSize);
		double yC = yStart + (yEnd - yStart) * double(i) / double(_blobSize);
		double zC = zStart + (zEnd - zStart) * double(i) / double(_blobSize);
		double blobScale = _rand.NextDouble() * double(_blobSize) / 16.0;
		double radXZ = double(MathHelper::Sin(float(i) * JavaMath::PI_FLOAT / float(_blobSize)) + 1.0F) * blobScale +
		               1.0;
		double radY = double(MathHelper::Sin(float(i) * JavaMath::PI_FLOAT / float(_blobSize)) + 1.0F) * blobScale + 1.0;
		int32_t minX = MathHelper::FloorDouble(xC - radXZ / 2.0);
		int32_t maxX = MathHelper::FloorDouble(xC + radXZ / 2.0);
		int32_t minY = MathHelper::FloorDouble(yC - radY / 2.0);
		int32_t maxY = MathHelper::FloorDouble(yC + radY / 2.0);
		int32_t minZ = MathHelper::FloorDouble(zC - radXZ / 2.0);
		int32_t maxZ = MathHelper::FloorDouble(zC + radXZ / 2.0);
		for (int32_t x = minX; x <= maxX; ++x) {
			double dx = (double(x) + 0.5 - xC) / (radXZ / 2.0);
			if (dx * dx >= 1.0)
				continue;
			for (int32_t y = minY; y <= maxY; ++y) {
				double dy = (double(y) + 0.5 - yC) / (radY / 2.0);
				if (dx * dx + dy * dy >= 1.0)
					continue;
				for (int32_t z = minZ; z <= maxZ; ++z) {
					double dz = (double(z) + 0.5 - zC) / (radXZ / 2.0);
					if (dx * dx + dy * dy + dz * dz < 1.0 && _world.GetBlockId({ x, y, z }) == BLOCK_STONE)
						_world.SetBlock({ x, y, z }, _type);
				}
			}
		}
	}
	return true;
}