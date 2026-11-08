/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/
#pragma once

#include "numeric_structs.h"
#include "world.h"
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <vector>

struct Node {
	Int3 pos;

	float g = std::numeric_limits<float>::max();
	float f = std::numeric_limits<float>::max();

	bool closed = false;

	Node* parent = nullptr;
};

class Pathfinder {
public:
	WorldManager* world = nullptr;

	Pathfinder() {};
	Pathfinder(WorldManager* _world) : world(_world) {};

	[[nodiscard]] std::vector<Int3> FindPath(Int3 _start, Int3 _goal, float _width = 0.6f, float _height = 1.8f,
	                                         float _maxDistance = 16.0f);

private:
	enum class ColumnResult {
		Blocked = 0,
		Open = 1,
		Water = -1,
		Lava = -2,
	};

	struct PQNode {
		float f;
		Node* node;

		bool operator>(const PQNode& _rhs) const {
			return f > _rhs.f;
		}
	};

	std::priority_queue<PQNode, std::vector<PQNode>, std::greater<PQNode>> open;

	std::unordered_map<Int3, Node> nodes;

	Int3 footprint{ 1, 2, 1 };

	Node* OpenNode(Int3 _pos);
	void Reset();

	ColumnResult GetVerticalOffset(Int3 _origin);

	std::optional<Int3> GetSafePoint(Int3 _pos, int _stepUp);

	int FindPathOptions(Int3 _current, Int3 _goal, float _maxDistance, Int3* _options);
};