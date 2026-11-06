/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "tree_gen.h"
#include <cassert>
#include <cstdint>

namespace TreeGenerator {
/**
 * @brief Attempts to generate an oak or birch tree.
 * 
 * @param world Pointer to the world where it'll generate
 * @param rand Pointer to the Java::Random that'll be utilized
 * @param pos Position of the lowest trunk-block
 * @param birch If the tree should be birch or oak
 * @return If tree successfully generated
 */
bool GenerateTree(WorldAccess& _world, Java::Random& _rand, Int3 _pos, bool _birch) {
	// Decide on the tree height (birches are one block taller)
	int32_t treeHeight = _rand.NextInt(3) + 4;
	if (_birch)
		treeHeight++;

	// Check if there's space to generate the tree
	// If any block in the desired area is neither air or
	// leaves, we fail the placement check
	if (_pos.y >= 1 && _pos.y + treeHeight + 1 <= CHUNK_HEIGHT) {
		Int3 check;
		for (check.y = _pos.y; check.y <= _pos.y + 1 + treeHeight; ++check.y) {
			int8_t width = 1;
			if (check.y == _pos.y)
				width = 0;

			if (check.y >= _pos.y + 1 + treeHeight - 2)
				width = 2;

			for (check.x = _pos.x - width; check.x <= _pos.x + width; ++check.x) {
				for (check.z = _pos.z - width; check.z <= _pos.z + width; ++check.z) {
					// Only test blocks that're within chunk boundaries
					if (check.y >= 0 && check.y < CHUNK_HEIGHT) {
						BlockType blockTest = _world.GetBlockId(check);
						if (blockTest != BLOCK_AIR && blockTest != BLOCK_LEAVES) {
							return false;
						}
					} else {
						return false;
					}
				}
			}
		}

		// Check if the bock under the source block is grass or dirt
		BlockType soilType = _world.GetBlockId(Int3{ _pos.x, _pos.y - 1, _pos.z });
		if ((soilType == BLOCK_GRASS || soilType == BLOCK_DIRT) && _pos.y < CHUNK_HEIGHT - treeHeight - 1) {
			// Replace the underlying block with dir
			_world.SetBlock(Int3{ _pos.x, _pos.y - 1, _pos.z }, BLOCK_DIRT);

			// Place leaves
			Int3 offset;
			for (offset.y = _pos.y - 3 + treeHeight; offset.y <= _pos.y + treeHeight; ++offset.y) {
				int32_t widthBase = offset.y - (_pos.y + treeHeight);
				int32_t treeWidth = 1 - widthBase / 2;

				for (offset.x = _pos.x - treeWidth; offset.x <= _pos.x + treeWidth; ++offset.x) {
					int32_t xLeaf = offset.x - _pos.x;

					for (offset.z = _pos.z - treeWidth; offset.z <= _pos.z + treeWidth; ++offset.z) {
						int32_t zLeaf = offset.z - _pos.z;
						// Leaves are placed within the tree width
						// and replace any non-opaque block
						if (((JavaMath::Abs(xLeaf) != treeWidth || JavaMath::Abs(zLeaf) != treeWidth ||
						      (_rand.NextInt(2) != 0 && widthBase != 0))) &&
						    !IsOpaque(_world.GetBlockId(offset))) {
							_world.SetBlock(offset, BLOCK_LEAVES, uint8_t(_birch ? TreeType::Birch : TreeType::Oak));
						}
					}
				}
			}

			// Replace air and leaves with trunk
			for (int32_t h = 0; h < treeHeight; ++h) {
				BlockType futureLog = _world.GetBlockId(Int3{ _pos.x, _pos.y + h, _pos.z });
				if (futureLog == BLOCK_AIR || futureLog == BLOCK_LEAVES) {
					_world.SetBlock(Int3{ _pos.x, _pos.y + h, _pos.z }, BLOCK_LOG,
					                uint8_t(_birch ? TreeType::Birch : TreeType::Oak));
				}
			}

			return true;
		}
		return false;
	}
	return false;
}

/**
 * @brief Configures the settings of a BigTree
 * 
 * @param pTreeHeight Sets the maximum tree height (this is internally multiplied by 12)
 * @param pBranchLength Sets the maximum branch length
 * @param pTrunkShape Determines the trunk shape
 */
void BigTree::Configure(double _pTreeHeight, double _pBranchLength, double _pTrunkShape) {
	branchStartEnd = {};
	branchNodeCount = 0;
	totalHeight = 0;
	// Default Java
	maximumTreeHeight = Java::DoubleToInt32(_pTreeHeight * 12.0);
	if (_pTreeHeight > 0.5) {
		trunkThickness = 5;
	}

	branchLength = _pBranchLength;
	trunkShape = _pTrunkShape;
}

/**
 * @brief Attempts to generate a big oak tree
 * 
 * @param pWorld Pointer to the world where it'll generate
 * @param pRand Pointer to the Java::Random that'll be utilized
 * @param pPos Position of the lowest trunk-block
 * @param pBirch If the tree should be birch or oak (not used for big trees)
 * @return If tree successfully generated
 */
bool BigTree::Generate(WorldAccess& _pWorld, Java::Random& _pRand, Int3 _pPos) {
	Configure(1.0, 1.0, 1.0);
	wm = &_pWorld;
	int64_t seed = _pRand.NextLong();
	rand.SetSeed(seed);
	basePos = _pPos;
	// If the height wasn't set, generate a new random height
	if (totalHeight == 0) {
		totalHeight = 5 + rand.NextInt(maximumTreeHeight);
	}

	// Check if tree can be placed
	if (!ValidPlacement())
		return false;
	GenerateBranchPositions();
	GenerateLeafClusters();
	GenerateTrunk();
	GenerateBranches();
	return true;
}

/**
 * @brief Determine where branches will go
 * 
 */
void BigTree::GenerateBranchPositions() {
	height = Java::DoubleToInt32(double(totalHeight) * heightFactor);
	if (height >= totalHeight) {
		height = totalHeight - 1;
	}

	int32_t branchesPerLayer = Java::DoubleToInt32(1.382 + std::pow(trunkShape * double(totalHeight) / 13.0, 2.0));
	if (branchesPerLayer < 1) {
		branchesPerLayer = 1;
	}

	// NOTE: branchCapacity is always 32
	/*
	const int branchCapacity = static_cast<size_t>(branchesPerLayer * totalHeight);
	if (branchCapacity <= 0)
		throw "Branch capacity is 0!";
	*/

	std::array<BranchPos, 32> candidateBranches;
	int32_t currentY = basePos.y + totalHeight - trunkThickness;
	int32_t branchCount = 1;
	int32_t targetY = basePos.y + height;
	int32_t canpoyLayer = currentY - basePos.y;
	candidateBranches[0].pos.x = basePos.x;
	candidateBranches[0].pos.y = currentY;
	candidateBranches[0].pos.z = basePos.z;
	candidateBranches[0].trunkY = targetY;
	--currentY;

	while (true) {
		for (; canpoyLayer >= 0; --canpoyLayer) {
			float canopyRadius = GetCanopyRadius(canpoyLayer);
			if (canopyRadius >= 0.0F) {
				for (int32_t attempts = 0; attempts < branchesPerLayer; ++attempts) {
					double radialDistance = branchLength * double(canopyRadius) * (double(rand.NextFloat()) + 0.328);
					// Oh hey, look! An approximation of pi!
					double angle = double(rand.NextFloat()) * 2.0 * 3.14159;
					int32_t branchX = MathHelper::FloorDouble(radialDistance * double(std::sin(angle)) +
					                                          double(basePos.x) + 0.5);
					int32_t branchZ = MathHelper::FloorDouble(radialDistance * double(std::cos(angle)) +
					                                          double(basePos.z) + 0.5);
					Int3 branchBase = Int3{ branchX, currentY, branchZ };
					Int3 branchTop = Int3{ branchX, currentY + trunkThickness, branchZ };
					if (CheckIfPathClear(branchBase, branchTop) == -1) {
						Int3 trunkConnection = basePos;
						double horizontalDistance = std::sqrt(
						    std::pow(double(JavaMath::Abs(basePos.x - branchBase.x)), 2.0) +
						    std::pow(double(JavaMath::Abs(basePos.z - branchBase.z)), 2.0));
						double verticalDrop = horizontalDistance * trunkSlopeFactor;
						if (double(branchBase.y) - verticalDrop > double(targetY)) {
							trunkConnection.y = targetY;
						} else {
							trunkConnection.y = Java::DoubleToInt32(double(branchBase.y) - verticalDrop);
						}

						if (CheckIfPathClear(trunkConnection, branchBase) == -1) {
							candidateBranches[branchCount].pos.x = branchX;
							candidateBranches[branchCount].pos.y = currentY;
							candidateBranches[branchCount].pos.z = branchZ;
							candidateBranches[branchCount].trunkY = trunkConnection.y;
							++branchCount;
						}
					}
				}
			}
			--currentY;
		}

		std::copy(candidateBranches.begin(), candidateBranches.begin() + branchCount, branchStartEnd.begin());
		branchNodeCount = size_t(branchCount);
		return;
	}
}

/**
 * @brief Place a circle of blocks along the specified plane
 * 
 * @param centerPos Center position of the circle
 * @param radius Radius of the circle
 * @param axis Axis along which the circle will grow
 * @param blockType Blocktype of the circle
 */
void BigTree::PlaceCircularLayer(Int3 _centerPos, float _radius, BranchAxis _axis, BlockType _blockType) {
	int32_t intRadius = Java::DoubleToInt32(double(_radius) + 0.618);
	BranchAxis axisU = BRANCH_ORIENTATION[_axis];
	BranchAxis axisV = BRANCH_ORIENTATION[_axis + AXIS_OFFSET];
	Int3 currentPos{ 0, 0, 0 };
	currentPos[_axis] = _centerPos[_axis];
	for (int32_t du = -intRadius; du <= intRadius; ++du) {
		currentPos[axisU] = _centerPos[axisU] + du;

		for (int32_t dv = -intRadius; dv <= intRadius; ++dv) {
			// Pythagorean Theorem
			double distance = std::sqrt(std::pow(JavaMath::Abs(du) + 0.5, 2.0) + std::pow(JavaMath::Abs(dv) + 0.5, 2.0));

			if (distance > double(_radius))
				continue;

			currentPos[axisV] = _centerPos[axisV] + dv;
			BlockType otherType = wm->GetBlockId(currentPos);

			// Only place if block can be overwritten
			if (otherType == BLOCK_AIR || otherType == BLOCK_LEAVES) {
				wm->SetBlock(currentPos, _blockType);
			}
		}
	}
}

/**
 * @brief Get the radius of the leaf canopy at the desired height
 * 
 * @param y Height to check
 * @return Radius in blocks
 */
float BigTree::GetCanopyRadius(int32_t _y) {
	if (double(_y) < double(float(totalHeight) * 0.3f)) {
		return -1.618F;
	} else {
		float halfHeight = float(totalHeight) / 2.0F;
		float distanceFromCenter = float(totalHeight) / 2.0F - float(_y);
		float radius;
		if (distanceFromCenter == 0.0F) {
			radius = halfHeight;
		} else if (MathHelper::Abs(distanceFromCenter) >= halfHeight) {
			radius = 0.0F;
		} else {
			radius = float(std::sqrt(std::pow(double(MathHelper::Abs(halfHeight)), 2.0) -
			                         std::pow(double(MathHelper::Abs(distanceFromCenter)), 2.0)));
		}

		radius *= 0.5F;
		return radius;
	}
}

/**
 * @brief Get the radius of the current trunk layer
 * 
 * @param layerIndex The index of the layer
 * @return float Trunk layer radius
 */
float BigTree::GetTrunkLayerRadius(int32_t _layerIndex) {
	if (_layerIndex < 0 || _layerIndex >= trunkThickness)
		return -1.0f;
	// Top and bottom of trunk are thinner
	if (_layerIndex == 0 || _layerIndex == trunkThickness - 1)
		return 2.0f;
	// Middle of trunk is thicker
	return 3.0f;
}

/**
 * @brief Iterate over the height 
 * 
 * @param base 
 */
void BigTree::PlaceLeavesAroundPoint(Int3 _base) {
	int32_t baseHeight = _base.y + trunkThickness;

	for (int32_t y = _base.y; y < baseHeight; ++y) {
		float trunkRadius = GetTrunkLayerRadius(y - _base.y);
		PlaceCircularLayer(Int3{ _base.x, y, _base.z }, trunkRadius, AXIS_Y, BLOCK_LEAVES);
	}
}

/**
 * @brief Draws a line of blockType between two coordinates
 * 
 * @param startPos The start position
 * @param endPos The end position
 * @param blockType The block that should be drawn along this line
 */
void BigTree::DrawBlockLine(Int3 _startPos, Int3 _endPos, BlockType _blockType) {
	Int3 delta = INT3_ZERO;
	BranchAxis dominantAxis = AXIS_X;
	delta = _endPos - _startPos;
	// Determine which axis was the largest magnitude
	for (int8_t axis = 0; axis < 3; axis++) {
		if (JavaMath::Abs(delta[axis]) > JavaMath::Abs(delta[dominantAxis])) {
			dominantAxis = BranchAxis(axis);
		}
	}
	// If an axis was chosen, we can continue
	if (delta[dominantAxis] == 0)
		return;
	// Determine secondary axes
	// X -> Y/Z
	// Y -> X/Z
	// Z -> X/Y
	BranchAxis secondaryA = BRANCH_ORIENTATION[dominantAxis];
	BranchAxis secondaryB = BRANCH_ORIENTATION[dominantAxis + AXIS_OFFSET];
	int8_t step = -1;
	if (delta[dominantAxis] > 0)
		step = 1;

	double secondaryRatioA = double(delta[secondaryA]) / double(delta[dominantAxis]);
	double secondaryRatioB = double(delta[secondaryB]) / double(delta[dominantAxis]);
	Int3 blockPos = INT3_ZERO;
	int32_t distanceAlongAxis = 0;

	for (int32_t totalSteps = delta[dominantAxis] + step; distanceAlongAxis != totalSteps; distanceAlongAxis += step) {
		blockPos[dominantAxis] = MathHelper::FloorDouble(double(_startPos[dominantAxis] + distanceAlongAxis) + 0.5);
		blockPos[secondaryA] = MathHelper::FloorDouble(double(_startPos[secondaryA]) +
		                                               double(distanceAlongAxis) * secondaryRatioA + 0.5);
		blockPos[secondaryB] = MathHelper::FloorDouble(double(_startPos[secondaryB]) +
		                                               double(distanceAlongAxis) * secondaryRatioB + 0.5);
		wm->SetBlock(blockPos, _blockType);
	}
}

/**
 * @brief Iterate through the branches and generate the leaves
 * 
 */
void BigTree::GenerateLeafClusters() {
	for (size_t i = 0; i < branchNodeCount; ++i) {
		Int3 pos = branchStartEnd[i].pos;
		PlaceLeavesAroundPoint(pos);
	}
}

bool BigTree::CanGenerateBranchAtHeight(int32_t _y) {
	return double(_y) >= (double(totalHeight) * 0.2);
}

void BigTree::GenerateTrunk() {
	Int3 startPos = basePos;
	Int3 endPos = basePos + Int3{ 0, height, 0 };
	DrawBlockLine(startPos, endPos, BLOCK_LOG);
	if (branchDensity == 2) {
		startPos.x++;
		endPos.x++;
		DrawBlockLine(startPos, endPos, BLOCK_LOG);
		startPos.z++;
		endPos.z++;
		DrawBlockLine(startPos, endPos, BLOCK_LOG);
		startPos.x--;
		endPos.x--;
		DrawBlockLine(startPos, endPos, BLOCK_LOG);
	}
}

void BigTree::GenerateBranches() {
	Int3 base = basePos;
	for (size_t branchIndex = 0; branchIndex < branchNodeCount; ++branchIndex) {
		Int3 branchPos = branchStartEnd[branchIndex].pos;
		int32_t trunkY = branchStartEnd[branchIndex].trunkY;
		base.y = trunkY;
		int32_t yHeight = base.y - basePos.y;
		if (CanGenerateBranchAtHeight(yHeight)) {
			DrawBlockLine(base, branchPos, BLOCK_LOG);
		}
	}
}

/**
 * @brief Check if the path is unobstructed between start and end in a straight line
 * 
 * @param startPos The start position
 * @param endPos The end position
 * @return int32_t 
 */
int32_t BigTree::CheckIfPathClear(Int3 _startPos, Int3 _endPos) {
	Int3 delta = INT3_ZERO;

	BranchAxis dominantAxis = AXIS_X;
	delta = _endPos - _startPos;
	for (int8_t axis = 0; axis < 3; ++axis) {
		if (JavaMath::Abs(delta[axis]) > JavaMath::Abs(delta[dominantAxis])) {
			dominantAxis = BranchAxis(axis);
		}
	}

	if (delta[dominantAxis] == 0)
		return -1;
	// Determine secondary axes
	BranchAxis secondaryA = BRANCH_ORIENTATION[dominantAxis];
	BranchAxis secondaryB = BRANCH_ORIENTATION[dominantAxis + AXIS_OFFSET];
	int8_t step = -1;
	if (delta[dominantAxis] > 0)
		step = 1;

	double secondaryRatioA = double(delta[secondaryA]) / double(delta[dominantAxis]);
	double secondaryRatioB = double(delta[secondaryB]) / double(delta[dominantAxis]);
	Int3 currentPos = INT3_ZERO;
	int32_t distanceAlongAxis = 0;

	int32_t totalSteps = delta[dominantAxis] + step;
	for (; distanceAlongAxis != totalSteps; distanceAlongAxis += step) {
		currentPos[dominantAxis] = _startPos[dominantAxis] + distanceAlongAxis;
		currentPos[secondaryA] = MathHelper::FloorDouble(double(_startPos[secondaryA]) +
		                                                 double(distanceAlongAxis) * secondaryRatioA);
		currentPos[secondaryB] = MathHelper::FloorDouble(double(_startPos[secondaryB]) +
		                                                 double(distanceAlongAxis) * secondaryRatioB);
		BlockType blockType = wm->GetBlockId(currentPos);
		if (blockType != BLOCK_AIR && blockType != BLOCK_LEAVES) {
			break;
		}
	}

	if (distanceAlongAxis == totalSteps)
		return -1;
	return JavaMath::Abs(distanceAlongAxis);
}

/**
 * @brief Test if the desired tree placement is valid along the vertical axis
 * 
 * @return If the placement is valid
 */
bool BigTree::ValidPlacement() {
	Int3 endPos = Int3{ basePos.x, basePos.y + totalHeight - 1, basePos.z };
	// Check if ground block is valid
	BlockType soilType = wm->GetBlockId(Int3{ basePos.x, basePos.y - 1, basePos.z });
	if (soilType != BLOCK_GRASS && soilType != BLOCK_DIRT)
		return false;
	int32_t clearLength = CheckIfPathClear(basePos, endPos);
	// Path isn't clear
	if (clearLength == -1)
		return true;
	// Path is too short
	if (clearLength < 6)
		return false;
	// Path is valid
	totalHeight = clearLength;
	return true;
}

/**
 * @brief Attempts to generate a taiga tree
 * 
 * @param pWorld Pointer to the world where it'll generate
 * @param pRand Pointer to the Java::Random that'll be utilized
 * @param pPos Position of the lowest trunk-block
 * @return If tree successfully generated
 */
bool GenerateTaiga(WorldAccess& _world, Java::Random& _rand, Int3 _pos) {
	int32_t height = _rand.NextInt(5) + 7;
	int32_t trunkHeight = height - _rand.NextInt(2) - 3;
	int32_t leavesHeight = height - trunkHeight;
	int32_t maxLeafRadius = 1 + _rand.NextInt(leavesHeight + 1);
	// Check if tree can be placed
	if (_pos.y >= 1 && _pos.y + height + 1 <= CHUNK_HEIGHT) {
		int32_t currentLeafRadius;
		for (int32_t y = _pos.y; y <= _pos.y + 1 + height; ++y) {
			if (y - _pos.y < trunkHeight) {
				currentLeafRadius = 0;
			} else {
				currentLeafRadius = maxLeafRadius;
			}

			for (int32_t x = _pos.x - currentLeafRadius; x <= _pos.x + currentLeafRadius; ++x) {
				for (int32_t z = _pos.z - currentLeafRadius; z <= _pos.z + currentLeafRadius; ++z) {
					if (y >= 0 && y < CHUNK_HEIGHT) {
						BlockType blockType = _world.GetBlockId(Int3{ x, y, z });
						if (blockType != BLOCK_AIR && blockType != BLOCK_LEAVES) {
							return false;
						}
					} else {
						return false;
					}
				}
			}
		}

		// Check if we're attempting to place it on a valid soil block
		BlockType soilType = _world.GetBlockId(Int3{ _pos.x, _pos.y - 1, _pos.z });
		if ((soilType == BLOCK_GRASS || soilType == BLOCK_DIRT) && _pos.y < CHUNK_HEIGHT - height - 1) {
			_world.SetBlock(Int3{ _pos.x, _pos.y - 1, _pos.z }, BLOCK_DIRT);
			currentLeafRadius = 0;
			// Generate the leaves
			for (int32_t y = _pos.y + height; y >= _pos.y + trunkHeight; --y) {
				for (int32_t x = _pos.x - currentLeafRadius; x <= _pos.x + currentLeafRadius; ++x) {
					int32_t xOffset = x - _pos.x;

					for (int32_t z = _pos.z - currentLeafRadius; z <= _pos.z + currentLeafRadius; ++z) {
						int32_t zOffset = z - _pos.z;
						if ((JavaMath::Abs(xOffset) != currentLeafRadius ||
						     JavaMath::Abs(zOffset) != currentLeafRadius || currentLeafRadius <= 0) &&
						    !IsOpaque(_world.GetBlockId(Int3{ x, y, z }))) {
							// Spruce leaves
							_world.SetBlock(Int3{ x, y, z }, BLOCK_LEAVES, uint8_t(TreeType::Spruce));
						}
					}
				}

				if (currentLeafRadius >= 1 && y == _pos.y + trunkHeight + 1) {
					--currentLeafRadius;
				} else if (currentLeafRadius < maxLeafRadius) {
					++currentLeafRadius;
				}
			}
			// Place the trunk
			for (int32_t logY = 0; logY < height - 1; ++logY) {
				BlockType type = _world.GetBlockId(Int3{ _pos.x, _pos.y + logY, _pos.z });
				if (type == BLOCK_AIR || type == BLOCK_LEAVES) {
					_world.SetBlock(Int3{ _pos.x, _pos.y + logY, _pos.z }, BLOCK_LOG, uint8_t(TreeType::Spruce));
				}
			}

			return true;
		}
		return false;
	}
	return false;
}

/**
 * @brief Attempts to generate an alt taiga tree
 * 
 * @param pWorld Pointer to the world where it'll generate
 * @param pRand Pointer to the Java::Random that'll be utilized
 * @param pPos Position of the lowest trunk-block
 * @return If tree successfully generated
 */
bool GenerateAltTaiga(WorldAccess& _world, Java::Random& _rand, Int3 _pos) {
	int32_t height = _rand.NextInt(4) + 6;
	int32_t trunkHeight = 1 + _rand.NextInt(2);
	int32_t leavesHeight = height - trunkHeight;
	int32_t maxLeafRadius = 2 + _rand.NextInt(2);
	// Check if tree can be placed
	if (_pos.y >= 1 && _pos.y + height + 1 <= CHUNK_HEIGHT) {
		for (int32_t y = _pos.y; y <= _pos.y + 1 + height; ++y) {
			int32_t currentLeafRadius = maxLeafRadius;
			if (y - _pos.y < trunkHeight) {
				currentLeafRadius = 0;
			}

			for (int32_t xOffset = _pos.x - currentLeafRadius; xOffset <= _pos.x + currentLeafRadius; ++xOffset) {
				for (int32_t zOffset = _pos.z - currentLeafRadius; zOffset <= _pos.z + currentLeafRadius; ++zOffset) {
					if (y >= 0 && y < CHUNK_HEIGHT) {
						BlockType type = _world.GetBlockId(Int3{ xOffset, y, zOffset });
						if (type != 0 && type != BLOCK_LEAVES) {
							return false;
						}
					} else {
						return false;
					}
				}
			}
		}

		// Check if we're attempting to place it on a valid soil block
		BlockType soilType = _world.GetBlockId(Int3{ _pos.x, _pos.y - 1, _pos.z });
		if ((soilType == BLOCK_GRASS || soilType == BLOCK_DIRT) && _pos.y < CHUNK_HEIGHT - height - 1) {
			_world.SetBlock(Int3{ _pos.x, _pos.y - 1, _pos.z }, BLOCK_DIRT);
			int32_t currentLeafRadius = _rand.NextInt(2);
			int32_t leafRadiusIncrementThreshold = 1;
			int32_t leafRadiusSwitch = 0;

			for (int32_t leafLayer = 0; leafLayer <= leavesHeight; ++leafLayer) {
				int32_t yLevel = _pos.y + height - leafLayer;

				for (int32_t x = _pos.x - currentLeafRadius; x <= _pos.x + currentLeafRadius; ++x) {
					int32_t xOffset = x - _pos.x;

					for (int32_t z = _pos.z - currentLeafRadius; z <= _pos.z + currentLeafRadius; ++z) {
						int32_t zOffset = z - _pos.z;
						if ((JavaMath::Abs(xOffset) != currentLeafRadius ||
						     JavaMath::Abs(zOffset) != currentLeafRadius || currentLeafRadius <= 0) &&
						    !IsOpaque(_world.GetBlockId(Int3{ x, yLevel, z }))) {
							_world.SetBlock(Int3{ x, yLevel, z }, BLOCK_LEAVES, uint8_t(TreeType::Spruce));
						}
					}
				}

				if (currentLeafRadius >= leafRadiusIncrementThreshold) {
					currentLeafRadius = leafRadiusSwitch;
					leafRadiusSwitch = 1;
					++leafRadiusIncrementThreshold;
					if (leafRadiusIncrementThreshold > maxLeafRadius) {
						leafRadiusIncrementThreshold = maxLeafRadius;
					}
				} else {
					++currentLeafRadius;
				}
			}

			int32_t logOffset = _rand.NextInt(3);
			for (int32_t logY = 0; logY < height - logOffset; ++logY) {
				BlockType type = _world.GetBlockId(Int3{ _pos.x, _pos.y + logY, _pos.z });
				if (type == BLOCK_AIR || type == BLOCK_LEAVES) {
					_world.SetBlock(Int3{ _pos.x, _pos.y + logY, _pos.z }, BLOCK_LOG, uint8_t(TreeType::Spruce));
				}
			}

			return true;
		}
		return false;
	}
	return false;
}
}; // namespace TreeGenerator