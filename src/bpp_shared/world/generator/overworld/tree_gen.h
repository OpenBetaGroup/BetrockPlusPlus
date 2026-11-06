/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#pragma once

#include "../shared/feature_gen.h" // brings in GenView, IsSolid, IsOpaque
#include "java_random.h"
#include "world_access.h"

enum class TreeType : int8_t {
	Oak = 0,
	Spruce = 1,
	Birch = 2,
};

/**
 * @brief Used for generating Oak or Birch Trees
 * 
 */
namespace TreeGenerator {
bool GenerateTree(WorldAccess& _world, Java::Random& _rand, Int3 _pos, bool _birch = false);
bool GenerateTaiga(WorldAccess& _world, Java::Random& _rand, Int3 _pos);
bool GenerateAltTaiga(WorldAccess& _world, Java::Random& _rand, Int3 _pos);

class BigTree {
private:
	struct BranchPos {
		Int3 pos;
		int32_t trunkY;
	};
	enum BranchAxis : int8_t {
		AXIS_X = 0,
		AXIS_Y = 1,
		AXIS_Z = 2,
		//TRUNK_Y = 3
	};
	static constexpr int8_t AXIS_OFFSET = 3;
	static constexpr BranchAxis BRANCH_ORIENTATION[6] = {
		AXIS_Z, // X to Z
		AXIS_X, // Y to X
		AXIS_X, // Z to X
		AXIS_Y, // X to Y
		AXIS_Z, // Y to Z
		AXIS_Y  // Z to Y
	};
	Java::Random rand = Java::Random();
	WorldAccess* wm = nullptr;
	Int3 basePos = INT3_ZERO;
	int32_t totalHeight = 0;
	int32_t height;
	static constexpr double heightFactor = 0.618;
	static constexpr double trunkSlopeFactor = 0.381;
	double branchLength = 1.0;
	double trunkShape = 1.0;
	static constexpr int32_t branchDensity = 1;
	int32_t maximumTreeHeight = 12;
	int32_t trunkThickness = 4;
	// branchStartEnd is always at most 15 entries large
	std::array<BranchPos, 15> branchStartEnd;
	size_t branchNodeCount = 0;

	bool ValidPlacement();
	void GenerateBranchPositions();
	void GenerateLeafClusters();
	void GenerateTrunk();
	void GenerateBranches();
	float GetCanopyRadius(int32_t _y);
	float GetTrunkLayerRadius(int32_t _layerIndex);
	void PlaceLeavesAroundPoint(Int3 _base);
	void DrawBlockLine(Int3 _startPos, Int3 _endPos, BlockType _blockType);
	int32_t CheckIfPathClear(Int3 _startPos, Int3 _endPos);
	void PlaceCircularLayer(Int3 _centerPos, float _radius, BranchAxis _axis, BlockType _blockType);
	bool CanGenerateBranchAtHeight(int32_t _y);
	void Configure(double _treeHeight, double _branchLength, double _trunkShape);

public:
	bool Generate(WorldAccess& _world, Java::Random& _rand, Int3 _pos);
};
}; // namespace TreeGenerator