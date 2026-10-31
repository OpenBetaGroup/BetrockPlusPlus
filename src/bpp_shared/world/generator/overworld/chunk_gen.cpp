/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "chunk_gen.h"
#include "chunk.h"
#include "generator/overworld/biome_gen.h"
#include "generator/overworld/tree_gen.h"
#include "tree_gen.h"
#include <memory>

/**
 * @brief Construct a new Beta 1.7.3 Overworld Generator
 *
 * @param pSeed The seed of the generated world
 * @param pWorld The world that the OverworldGenerator belongs to
 */
OverworldGenerator::OverworldGenerator(int64_t _pSeed) : Generator(_pSeed) {
	rand = Java::Random(seed);
	// Init Terrain Noise
	lowNoiseGen = NoiseOctavesPerlin(rand, 16);
	highNoiseGen = NoiseOctavesPerlin(rand, 16);
	selectorNoiseGen = NoiseOctavesPerlin(rand, 8);
	sandGravelNoiseGen = NoiseOctavesPerlin(rand, 4);
	stoneNoiseGen = NoiseOctavesPerlin(rand, 4);
	continentalnessNoiseGen = NoiseOctavesPerlin(rand, 10);
	depthNoiseGen = NoiseOctavesPerlin(rand, 16);
	treeDensityNoiseGen = NoiseOctavesPerlin(rand, 8);
	biomeGen = BiomeGenerator(seed);
}

/**
 * @brief Generate a non-populated chunk
 *
 * @param chunkPos The x,z coordinate of the chunk
 * @return std::shared_ptr<Chunk>
 */
void OverworldGenerator::GenerateChunk(Chunk& _chunk) {
	rand.SetSeed(int64_t(_chunk.cpos.x) * 341873128712L + int64_t(_chunk.cpos.z) * 132897987541L);

	// Allocate empty chunk
	//_chunk.Clear();

	// Generate Biomes
	biomeGen.GenerateBiomeMap(biomeMap, temperature, humidity, weirdness,
	                          Int2{ _chunk.cpos.x * CHUNK_WIDTH, _chunk.cpos.z * CHUNK_WIDTH });

	// Store the final temperature and humidity in the chunk so PopulateChunk
	// (which runs on a different thread_local OverworldGenerator) can reconstruct the
	// biome map via GetBiomeFromLookup without re-running the noise generators.
	if (!_chunk.climate)
		_chunk.climate = std::make_unique<ChunkAreaClimate>();
	for (size_t i = 0; i < CHUNK_AREA; ++i) {
		_chunk.climate->temperature[i] = float(temperature[i]);
		_chunk.climate->humidity[i] = float(humidity[i]);
		_chunk.biomes.Set(i, biomeMap.Get(i));
	}

	// Generate the Terrain, minus any caves, as just stone
	GenerateTerrain(_chunk);
	// Replace some of the stone with Biome-appropriate blocks
	ReplaceBlocksForBiome(_chunk);
	// Carve caves
	caver.GenerateCavesForChunk(_chunk, seed);
	// Generate heightmap
	//_chunk.GenerateHeightMap();

	_chunk.isModified = true;
}

/**
 * @brief Replace some of the stone with Biome-appropriate blocks
 *
 * @param chunkPos The x,z coordinate of the chunk
 * @param c The chunk that should gets its blocks replaced
 */
void OverworldGenerator::ReplaceBlocksForBiome(Chunk& _chunk) {
	const double oneThirtySecond = 1.0 / 32.0;
	// Init noise maps
	//sandNoise.resize(256, 0.0);
	//gravelNoise.resize(256, 0.0);
	//stoneNoise.resize(256, 0.0);

	// Populate noise maps
	sandGravelNoiseGen.GenerateOctaves(
	    sandNoise, Vec3{ double(_chunk.cpos.x * CHUNK_WIDTH), double(_chunk.cpos.z * CHUNK_WIDTH), 0.0 },
	    Int32_3{ 16, 16, 1 }, Vec3{ oneThirtySecond, oneThirtySecond, 1.0 });
	sandGravelNoiseGen.GenerateOctaves(
	    gravelNoise, Vec3{ double(_chunk.cpos.x * CHUNK_WIDTH), 109.0134, double(_chunk.cpos.z * CHUNK_WIDTH) },
	    Int32_3{ 16, 1, 16 }, Vec3{ oneThirtySecond, 1.0, oneThirtySecond });
	stoneNoiseGen.GenerateOctaves(stoneNoise,
	                              Vec3{ double(_chunk.cpos.x * CHUNK_WIDTH), double(_chunk.cpos.z * CHUNK_WIDTH), 0.0 },
	                              Int32_3{ 16, 16, 1 },
	                              Vec3{ oneThirtySecond * 2.0, oneThirtySecond * 2.0, oneThirtySecond * 2.0 });

	// Iterate through entire chunk
	for (int32_t x = 0; x < CHUNK_WIDTH; ++x) {
		for (int32_t z = 0; z < CHUNK_WIDTH; ++z) {
			// This is intentional, to match b1.7.3 behavior!
			std::size_t bindex = size_t(x + z * CHUNK_WIDTH);
			// Get values from noise maps
			const Biome biome = static_cast<Biome>(biomeMap.Get(bindex));
			const bool sandActive = sandNoise[bindex] + rand.NextDouble() * 0.2 > 0.0;
			const bool gravelActive = gravelNoise[bindex] + rand.NextDouble() * 0.2 > 3.0;
			const int32_t stoneActive = Java::DoubleToInt32(stoneNoise[bindex] / 3.0 + 3.0 + rand.NextDouble() * 0.25);
			int32_t stoneDepth = -1;
			// Get biome-appropriate top and filler blocks
			BlockType topBlock = GetTopBlock(biome);
			BlockType fillerBlock = GetFillerBlock(biome);

			// Iterate over column top to bottom
			for (int32_t y = CHUNK_HEIGHT - 1; y >= 0; --y) {
				// This is intentional, to match b1.7.3 behavior!
				const Int3 bpos{ z, y, x };
				// Place Bedrock at bottom with some randomness
				if (y <= 0 + rand.NextInt(5)) {
					_chunk.SetBlockRaw(bpos, BLOCK_BEDROCK);
					continue;
				}

				const BlockType currentBlock = _chunk.GetBlock(bpos);
				// Ignore air
				if (currentBlock == BLOCK_AIR) {
					stoneDepth = -1;
					continue;
				}

				// If we counter stone, start replacing it
				if (currentBlock == BLOCK_STONE) {
					if (stoneDepth == -1) {
						if (stoneActive <= 0) {
							topBlock = BLOCK_AIR;
							fillerBlock = BLOCK_STONE;
						} else if (y >= WATER_LEVEL - 4 && y <= WATER_LEVEL + 1) {
							// If we're close to the water level, apply gravel and sand
							topBlock = GetTopBlock(biome);
							fillerBlock = GetFillerBlock(biome);

							if (gravelActive) {
								topBlock = BLOCK_AIR;
								fillerBlock = BLOCK_GRAVEL;
							}
							if (sandActive) {
								topBlock = BLOCK_SAND;
								fillerBlock = BLOCK_SAND;
							}
						}

						// Add water if we're below water level
						if (y < WATER_LEVEL && topBlock == BLOCK_AIR) {
							topBlock = BLOCK_WATER_STILL;
						}

						stoneDepth = stoneActive;
						// Place filler block if we're underwater
						_chunk.SetBlockRaw(bpos, (y >= WATER_LEVEL - 1) ? topBlock : fillerBlock);
					} else if (stoneDepth > 0) {
						--stoneDepth;
						_chunk.SetBlockRaw(bpos, fillerBlock);
						if (stoneDepth == 0 && fillerBlock == BLOCK_SAND) {
							stoneDepth = rand.NextInt(4);
							fillerBlock = BLOCK_SANDSTONE;
						}
					}
				}
			}
		}
	}
}

/**
 * @brief Generate the Terrain, minus any caves, as just stone
 *
 * @param chunkPos The x,z coordinate of the chunk
 * @param c The chunk that should get its terrain generated
 */
void OverworldGenerator::GenerateTerrain(Chunk& _chunk) {
	// Generate 4x16x4 low resolution noise map
	GenerateTerrainNoise(Int3{ _chunk.cpos.x * 4, 0, _chunk.cpos.z * 4 }, MAX);

	// Fill subchunks with water
	_chunk.FillSlices(0, WATER_LEVEL-1, BLOCK_WATER_STILL);

	// Write water surface
	for (int32_t x = 0; x < CHUNK_WIDTH; ++x) {
		for (int32_t z = 0; z < CHUNK_WIDTH; ++z) {
			if (temperature[size_t(x * CHUNK_WIDTH + z)] < 0.5)
				_chunk.SetBlockRaw(Int3{ x, WATER_LEVEL - 1, z }, BLOCK_ICE);
		}
	}

	// Terrain noise is interpolated and only sampled every 4 blocks
	for (int32_t sampleX = 0; sampleX < 4; ++sampleX) {
		for (int32_t sampleZ = 0; sampleZ < 4; ++sampleZ) {
			for (int32_t sampleY = 0; sampleY < 16; ++sampleY) {
				const double verticalLerpStep = 0.125;

				// Get noise cube corners
				double corner000 = terrainNoiseField[size_t(((sampleX + 0) * MAX.z + sampleZ + 0) * MAX.y + sampleY + 0)];
				double corner010 = terrainNoiseField[size_t(((sampleX + 0) * MAX.z + sampleZ + 1) * MAX.y + sampleY + 0)];
				double corner100 = terrainNoiseField[size_t(((sampleX + 1) * MAX.z + sampleZ + 0) * MAX.y + sampleY + 0)];
				double corner110 = terrainNoiseField[size_t(((sampleX + 1) * MAX.z + sampleZ + 1) * MAX.y + sampleY + 0)];
				const double corner001 =
				    (terrainNoiseField[size_t(((sampleX + 0) * MAX.z + sampleZ + 0) * MAX.y + sampleY + 1)] -
				     corner000) *
				    verticalLerpStep;
				const double corner011 =
				    (terrainNoiseField[size_t(((sampleX + 0) * MAX.z + sampleZ + 1) * MAX.y + sampleY + 1)] -
				     corner010) *
				    verticalLerpStep;
				const double corner101 =
				    (terrainNoiseField[size_t(((sampleX + 1) * MAX.z + sampleZ + 0) * MAX.y + sampleY + 1)] -
				     corner100) *
				    verticalLerpStep;
				const double corner111 =
				    (terrainNoiseField[size_t(((sampleX + 1) * MAX.z + sampleZ + 1) * MAX.y + sampleY + 1)] -
				     corner110) *
				    verticalLerpStep;
				
				const bool allAir =	corner000 <= 0.0 &&
									corner010 <= 0.0 &&
									corner100 <= 0.0 &&
									corner110 <= 0.0 &&
									corner001 <= 0.0 &&
									corner011 <= 0.0 &&
									corner101 <= 0.0 &&
									corner111 <= 0.0;
				if (allAir)
					continue;

				// Interpolate the 1/4th scale noise
				for (int32_t subY = 0; subY < 8; ++subY) {
					const double horizontalLerpStep = 0.25;
					double terrainX0 = corner000;
					double terrainX1 = corner010;
					const double terrainStepX0 = (corner100 - corner000) * horizontalLerpStep;
					const double terrainStepX1 = (corner110 - corner010) * horizontalLerpStep;

					for (int32_t subX = 0; subX < 4; ++subX) {
						Int3 bpos{ (subX + sampleX * 4), ((sampleY * 8) + subY), (sampleZ * 4) };
						double terrainDensity = terrainX0;
						const double densityStepZ = (terrainX1 - terrainX0) * horizontalLerpStep;

						for (int32_t subZ = 0; subZ < 4; ++subZ) {
							// If the terrain density falls below,
							// replace block with stone
							if (terrainDensity > 0.0)
								_chunk.SetBlockRaw(bpos, BLOCK_STONE);
							// Prep for next iteration
							bpos.z += 1;
							terrainDensity += densityStepZ;
						}

						terrainX0 += terrainStepX0;
						terrainX1 += terrainStepX1;
					}

					corner000 += corner001;
					corner010 += corner011;
					corner100 += corner101;
					corner110 += corner111;
				}
			}
		}
	}
}

/**
 * @brief Make terrain noise and updates the terrain map
 *
 * @param terrainMap The terrain map that the scaled-down terrain values will be written to
 * @param chunkPos The x,y,z coordinate of the sub-chunk
 * @param max Defines the area of the terrainMap
 */
void OverworldGenerator::GenerateTerrainNoise(Int3 _cpos, Int3 _max) {
	const double horiScale = 684.412;
	const double vertScale = 684.412;

	// We do this to need to generate noise as often
	continentalnessNoiseGen.GenerateOctaves(continentalnessNoiseField, Int32_2{ _cpos.x, _cpos.z },
	                                        Int32_2{ _max.x, _max.z }, Vec2{ 1.121, 1.121 }, 0.5);
	depthNoiseGen.GenerateOctaves(depthNoiseField, Int32_2{ _cpos.x, _cpos.z }, Int32_2{ _max.x, _max.z },
	                              Vec2{ 200.0, 200.0 }, 0.5);
	// TODO: Ideally skip low/high value calculation for regions that are only high/low noise
	selectorNoiseGen.GenerateOctaves(selectorNoiseField, Vec3{ double(_cpos.x), double(_cpos.y), double(_cpos.z) },
	                                 _max, Vec3{ horiScale / 80.0, vertScale / 160.0, horiScale / 80.0 });
	lowNoiseGen.GenerateOctaves(lowNoiseField, Vec3{ double(_cpos.x), double(_cpos.y), double(_cpos.z) }, _max,
	                            Vec3{ horiScale, vertScale, horiScale });
	highNoiseGen.GenerateOctaves(highNoiseField, Vec3{ double(_cpos.x), double(_cpos.y), double(_cpos.z) }, _max,
	                             Vec3{ horiScale, vertScale, horiScale });
	// Used to iterate 3D noise maps (low, high, selector)
	size_t xyzIndex = 0;
	// Used to iterate 2D Noise maps (depth, continentalness)
	size_t xzIndex = 0;
	const int32_t scaleFraction = 16 / _max.x;

	for (int32_t iX = 0; iX < _max.x; ++iX) {
		const int32_t sampleX = iX * scaleFraction + scaleFraction / 2;

		for (int32_t iZ = 0; iZ < _max.z; ++iZ) {
			// Sample 2D noises
			const int32_t sampleZ = iZ * scaleFraction + scaleFraction / 2;
			// Apply biome-noise-dependent variety
			const size_t sampleIndex = size_t(sampleX * CHUNK_WIDTH + sampleZ);
			const double temp = temperature[sampleIndex];
			double humi = humidity[sampleIndex] * temp;
			humi = 1.0 - humi;
			humi *= humi;
			humi *= humi;
			humi = 1.0 - humi;
			// Sample contientalness noise
			double continentalness = (continentalnessNoiseField[xzIndex] + 256.0) / 512.0;
			continentalness *= humi;
			continentalness = std::min(continentalness, 1.0);
			// Sample depth noise
			double depthNoise = depthNoiseField[xzIndex] / 8000.0;
			if (depthNoise < 0.0)
				depthNoise = -depthNoise * 0.3;
			depthNoise = depthNoise * 3.0 - 2.0;
			if (depthNoise < 0.0) {
				depthNoise /= 2.0;
				depthNoise = std::max(depthNoise, -1.0);
				depthNoise /= 1.4;
				depthNoise /= 2.0;
				continentalness = 0.0;
			} else {
				depthNoise = std::min(depthNoise, 1.0);
				depthNoise /= 8.0;
			}
			if (continentalness < 0.0)
				continentalness = 0.0;
			continentalness += 0.5;
			depthNoise = depthNoise * double(_max.y) / 16.0;
			const double elevationOffset = double(_max.y) / 2.0 + depthNoise * 4.0;
			++xzIndex;

			const double dividedContinentalness = 12.0 / continentalness;
			for (int32_t iY = 0; iY < _max.y; ++iY) {
				// Sample 3D noises
				double terrainDensity = 0.0;
				double densityOffset = (double(iY) - elevationOffset) * dividedContinentalness;
				if (densityOffset < 0.0) {
					densityOffset *= 4.0;
				}
				// Sample low noise
				const double lowNoise = lowNoiseField[xyzIndex] / 512.0;
				// Sample high noise
				const double highNoise = highNoiseField[xyzIndex] / 512.0;
				// Sample selector noise
				const double selectorNoise = (selectorNoiseField[xyzIndex] / 10.0 + 1.0) / 2.0;
				if (selectorNoise < 0.0) {
					terrainDensity = lowNoise;
				} else if (selectorNoise > 1.0) {
					terrainDensity = highNoise;
				} else {
					terrainDensity = Lerp(selectorNoise, lowNoise, highNoise);
				}

				terrainDensity -= densityOffset;
				// Reduce density towards max height
				if (iY > _max.y - 4) {
					double heightEdgeFade = double(float(iY - (_max.y - 4)) / 3.0F);
					terrainDensity = (terrainDensity * (1.0 - heightEdgeFade)) + (-10.0 * heightEdgeFade);
				}

				terrainNoiseField[xyzIndex] = terrainDensity;
				++xyzIndex;
			}
		}
	}
}

/**
 * @brief Probes the biome map at the specified coordinates
 *
 * @param worldPos The x,z coordinate of the desired block column
 * @return The Biome at that column
 */
Biome OverworldGenerator::GetBiomeAt(Int2 _worldPos) {
	// biomeMap is always for the chunk whose origin is (cpos.x*16, cpos.z*16).
	// Convert world coords to chunk-local [0,15] and index directly.
	const int32_t localX = ((_worldPos.x % CHUNK_WIDTH) + CHUNK_WIDTH) % CHUNK_WIDTH;
	const int32_t localZ = ((_worldPos.z % CHUNK_WIDTH) + CHUNK_WIDTH) % CHUNK_WIDTH;
	return static_cast<Biome>(biomeMap.Get(localX * CHUNK_WIDTH + localZ));
}

// Exact port of BiomeGenBase.getRandomWorldGenForTrees() and per-biome overrides.
void OverworldGenerator::GenerateTreeForBiome(WorldWrapper& _world, Java::Random& _pRand, Int3 _pos, Biome _biome) {
	thread_local std::unique_ptr<TreeGenerator::BigTree> bigTree;
	if (!bigTree)
		bigTree = std::make_unique<TreeGenerator::BigTree>();
	TreeGenerator::BigTree& big = *bigTree;
	switch (_biome) {
	case BIOME_TAIGA:
		if (_pRand.NextInt(3) == 0)
			TreeGenerator::GenerateTaiga(_world, _pRand, _pos);
		else
			TreeGenerator::GenerateAltTaiga(_world, _pRand, _pos);
		break;
	case BIOME_FOREST:
		if (_pRand.NextInt(5) == 0) {
			TreeGenerator::GenerateTree(_world, _pRand, _pos, true);
		} else if (_pRand.NextInt(3) == 0) {
			big.Generate(_world, _pRand, _pos);
		} else {
			TreeGenerator::GenerateTree(_world, _pRand, _pos);
		}
		break;
	case BIOME_RAINFOREST:
		if (_pRand.NextInt(3) == 0) {
			big.Generate(_world, _pRand, _pos);
		} else {
			TreeGenerator::GenerateTree(_world, _pRand, _pos);
		}
		break;
	default:
		if (_pRand.NextInt(10) == 0) {
			big.Generate(_world, _pRand, _pos);
		} else {
			TreeGenerator::GenerateTree(_world, _pRand, _pos);
		}
		break;
	}
}

/**
 * @brief Populates the specified chunk with biome-specific features.
 *
 * Direct port of ChunkProviderGenerate.populate() from Beta 1.7.3.
 * Biome is sampled at blockX+16, blockZ+16 from stored chunk climate data.
 * RNG seeding, section order, rand call counts, and coordinate offsets all
 * match the Java source exactly.
 */
bool OverworldGenerator::PopulateChunk(Chunk& _chunk, WorldWrapper& _world) {
	const int32_t blockX = _chunk.cpos.x * CHUNK_WIDTH;
	const int32_t blockZ = _chunk.cpos.z * CHUNK_WIDTH;
	Biome biome = biomeGen.GetBiomeAtPoint(Int2{ blockX + CHUNK_WIDTH, blockZ + CHUNK_WIDTH });
	// Java RNG seeding sequence
	rand.SetSeed(_world.GetSeed());
	int64_t xSalt = rand.NextLong() / 2L * 2L + 1L;
	int64_t zSalt = rand.NextLong() / 2L * 2L + 1L;
	// Use unsigned arithmetic to avoid overflow UB
	uint64_t xSaltU = static_cast<uint64_t>(xSalt);
	uint64_t zSaltU = static_cast<uint64_t>(zSalt);
	uint64_t xPart = static_cast<uint64_t>(static_cast<int64_t>(_chunk.cpos.x)) * xSaltU;
	uint64_t zPart = static_cast<uint64_t>(static_cast<int64_t>(_chunk.cpos.z)) * zSaltU;
	uint64_t combined = (xPart + zPart) ^ static_cast<uint64_t>(_world.GetSeed());

	rand.SetSeed(static_cast<int64_t>(combined));

	Int3 coord;

	// Water lakes
	if (rand.NextInt(4) == 0) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		FeatureGenerator::GenerateLake(BLOCK_WATER_STILL, _world, rand, coord);
	}

	// Lava lakes
	if (rand.NextInt(8) == 0) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(rand.NextInt(120) + 8);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		if (coord.y < WATER_LEVEL || rand.NextInt(10) == 0)
			FeatureGenerator::GenerateLake(BLOCK_LAVA_STILL, _world, rand, coord);
	}

	// Dungeons
	for (int32_t i = 0; i < 8; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		FeatureGenerator::GenerateDungeon(_world, rand, coord);
	}

	// Clay
	for (int32_t i = 0; i < 10; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH);
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH);
		FeatureGenerator::GenerateClay(_world, rand, coord, 32);
	}

	// Dirt blobs
	for (int32_t i = 0; i < 20; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH);
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH);
		FeatureGenerator::GenerateOre(BLOCK_DIRT, _world, rand, coord, 32);
	}

	// Gravel blobs
	for (int32_t i = 0; i < 10; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH);
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH);
		FeatureGenerator::GenerateOre(BLOCK_GRAVEL, _world, rand, coord, 32);
	}

	// Coal Ore blobs
	for (int32_t i = 0; i < 20; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH);
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH);
		FeatureGenerator::GenerateOre(BLOCK_ORE_COAL, _world, rand, coord, 16);
	}

	// Iron Ore blobs
	for (int32_t i = 0; i < 20; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH);
		coord.y = rand.NextInt(CHUNK_HEIGHT / 2);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH);
		FeatureGenerator::GenerateOre(BLOCK_ORE_IRON, _world, rand, coord, 8);
	}

	// Gold Ore blobs
	for (int32_t i = 0; i < 2; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH);
		coord.y = rand.NextInt(CHUNK_HEIGHT / 4);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH);
		FeatureGenerator::GenerateOre(BLOCK_ORE_GOLD, _world, rand, coord, 8);
	}

	// Redstone Ore blobs
	for (int32_t i = 0; i < 8; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH);
		coord.y = rand.NextInt(CHUNK_HEIGHT / 8);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH);
		FeatureGenerator::GenerateOre(BLOCK_ORE_REDSTONE_OFF, _world, rand, coord, 7);
	}

	// Diamond Ore blobs
	{
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH);
		coord.y = rand.NextInt(CHUNK_HEIGHT / 8);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH);
		FeatureGenerator::GenerateOre(BLOCK_ORE_DIAMOND, _world, rand, coord, 7);
	}

	// Lapis lazuli Ore blobs
	{
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH);
		coord.y = rand.NextInt(CHUNK_HEIGHT / 8) + rand.NextInt(CHUNK_HEIGHT / 8);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH);
		FeatureGenerator::GenerateOre(BLOCK_ORE_LAPIS_LAZULI, _world, rand, coord, 6);
	}

	// Tree count
	double noiseVal = treeDensityNoiseGen.GenerateOctaves({ double(blockX) * 0.5, double(blockZ) * 0.5 });
	int32_t baseTreeCount = Java::DoubleToInt32((noiseVal / 8.0 + rand.NextDouble() * 4.0 + 4.0) / 3.0);
	int32_t treeCount = 0;
	if (rand.NextInt(10) == 0)
		++treeCount;

	// Biome tree adjustments
	switch (biome) {
	case BIOME_FOREST:
	case BIOME_RAINFOREST:
	case BIOME_TAIGA:
		treeCount += baseTreeCount + 5;
		break;
	case BIOME_SEASONALFOREST:
		treeCount += baseTreeCount + 2;
		break;
	case BIOME_DESERT:
	case BIOME_TUNDRA:
	case BIOME_PLAINS:
		treeCount -= 20;
		break;
	case BIOME_NONE:
	case BIOME_SWAMPLAND:
	case BIOME_SAVANNA:
	case BIOME_SHRUBLAND:
	case BIOME_ICEDESERT:
	case BIOME_HELL:
	case BIOME_SKY:
		break;
	}

	for (int32_t i = 0; i < treeCount; ++i) {
		int32_t tx = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		int32_t tz = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		int32_t ty = _world.GetHeightValue(tx, tz);
		coord = { tx, ty, tz };
		GenerateTreeForBiome(_world, rand, coord, biome);
	}

	// Dandelion patches
	{
		int32_t count = 0;
		switch (biome) {
		case BIOME_FOREST:
			count = 2;
			break;
		case BIOME_SEASONALFOREST:
			count = 4;
			break;
		case BIOME_TAIGA:
			count = 2;
			break;
		case BIOME_PLAINS:
			count = 3;
			break;
		default:
			count = 0;
			break;
		}
		for (int32_t i = 0; i < count; ++i) {
			coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
			coord.y = rand.NextInt(CHUNK_HEIGHT);
			coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
			FeatureGenerator::GenerateFlowers(BLOCK_DANDELION, _world, rand, coord);
		}
	}

	// Tall grass / fern patches
	{
		int32_t count = 0;
		switch (biome) {
		case BIOME_FOREST:
			count = 2;
			break;
		case BIOME_RAINFOREST:
			count = 10;
			break;
		case BIOME_SEASONALFOREST:
			count = 2;
			break;
		case BIOME_TAIGA:
			count = 1;
			break;
		case BIOME_PLAINS:
			count = 10;
			break;
		default:
			count = 0;
			break;
		}
		for (int32_t i = 0; i < count; ++i) {
			uint8_t grassMeta = 1;
			if (biome == BIOME_RAINFOREST && rand.NextInt(3) != 0)
				grassMeta = 2; // fern
			coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
			coord.y = rand.NextInt(CHUNK_HEIGHT);
			coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
			FeatureGenerator::GenerateTallgrass(grassMeta, _world, rand, coord);
		}
	}

	// Deadbush patches
	{
		int32_t count = (biome == BIOME_DESERT) ? 2 : 0;
		for (int32_t i = 0; i < count; ++i) {
			coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
			coord.y = rand.NextInt(CHUNK_HEIGHT);
			coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
			FeatureGenerator::GenerateDeadbush(_world, rand, coord);
		}
	}

	// Rose patches
	if (rand.NextInt(2) == 0) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		FeatureGenerator::GenerateFlowers(BLOCK_ROSE, _world, rand, coord);
	}

	// Brown mushroom patches
	if (rand.NextInt(4) == 0) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		FeatureGenerator::GenerateFlowers(BLOCK_MUSHROOM_BROWN, _world, rand, coord);
	}

	// Red mushroom patches
	if (rand.NextInt(8) == 0) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		FeatureGenerator::GenerateFlowers(BLOCK_MUSHROOM_RED, _world, rand, coord);
	}

	// Sugar cane
	for (int32_t i = 0; i < 10; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		FeatureGenerator::GenerateSugarcane(_world, rand, coord);
	}

	// Pumpkin patches
	if (rand.NextInt(32) == 0) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(CHUNK_HEIGHT);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		FeatureGenerator::GeneratePumpkins(_world, rand, coord);
	}

	// Cacti
	{
		int32_t count = (biome == BIOME_DESERT) ? 10 : 0;
		for (int32_t i = 0; i < count; ++i) {
			coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
			coord.y = rand.NextInt(CHUNK_HEIGHT);
			coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
			FeatureGenerator::GenerateCacti(_world, rand, coord);
		}
	}

	// Water springs
	for (int32_t i = 0; i < 50; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(rand.NextInt(120) + 8);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		FeatureGenerator::GenerateLiquid(BLOCK_WATER_FLOWING, _world, rand, coord, false);
	}

	// Lava springs
	for (int32_t i = 0; i < 20; ++i) {
		coord.x = blockX + rand.NextInt(CHUNK_WIDTH) + 8;
		coord.y = rand.NextInt(rand.NextInt(rand.NextInt(112) + 8) + 8);
		coord.z = blockZ + rand.NextInt(CHUNK_WIDTH) + 8;
		FeatureGenerator::GenerateLiquid(BLOCK_LAVA_FLOWING, _world, rand, coord, false);
	}

	// Snow/ice placement for cold biomes
	for (int32_t x = blockX + 8; x < blockX + 8 + CHUNK_WIDTH; ++x) {
		for (int32_t z = blockZ + 8; z < blockZ + 8 + CHUNK_WIDTH; ++z) {
			int32_t topY = _world.FindTopSolidBlock(x, z);
			double temp = _world.GetTemperatureAt(x, z) - double(topY - 64) / 64.0 * 0.3;
			if (temp < 0.5 && topY > 0 && topY < CHUNK_HEIGHT) {
				const auto topBlock = _world.GetBlockId({ x, topY, z });
				if (topBlock != BLOCK_AIR)
					continue;
				const auto belowBlock = _world.GetBlockId({ x, topY - 1, z });
				if (belowBlock != BLOCK_ICE && IsSolid(belowBlock)) {
					_world.SetBlock({ x, topY, z }, BLOCK_SNOW_LAYER);
				}
			}
		}
	}
	// TODO: Maybe just have this as a reusable scratch buffer for the generator?
	#ifdef BUILD_SERVER
	// Now we don't need these anymore, at least as a server
	_chunk.climate.reset();
	#endif
	return true;
}