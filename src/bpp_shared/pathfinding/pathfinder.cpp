/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/
#include <algorithm>
#include <cmath>
#include <optional>
#include <queue>
#include <vector>
#include "world/world.h"

#include "blocks/block_properties.h"
#include "numeric_structs.h"
#include "pathfinder.h"

Node* Pathfinder::OpenNode(Int3 _pos) {
	auto [it, inserted] = nodes.try_emplace(_pos);

	if (inserted) {
		it->second.pos = _pos;
	}

	return &it->second;
}

void Pathfinder::Reset() {
	while (!open.empty())
		open.pop();

	nodes.clear();
}

Pathfinder::ColumnResult Pathfinder::GetVerticalOffset(Int3 _origin) {
	for (int x = _origin.x; x < _origin.x + footprint.x; x++) {
		for (int y = _origin.y; y < _origin.y + footprint.y; y++) {
			for (int z = _origin.z; z < _origin.z + footprint.z; z++) {
				Int3 pos{ x, y, z };
				BlockType block = world->GetBlockId(pos);
				if (block == BlockType::BLOCK_AIR)
					continue;

				if (block == BlockType::BLOCK_DOOR_WOOD || block == BlockType::BLOCK_DOOR_IRON) {
					// Bit 0x4 is the open/closed flag on either half of the door.
					// An open door doesn't block movement; a closed one does.
					if (!(world->GetMetadata(pos) & 4))
						return ColumnResult::Blocked;
					continue;
				}

				const Material& mat = Blocks::blockProperties[block].material;
				if (mat.isSolid)
					return ColumnResult::Blocked;
				if (mat.type == MaterialType::Water)
					return ColumnResult::Water;
				if (mat.type == MaterialType::Lava)
					return ColumnResult::Lava;
			}
		}
	}

	return ColumnResult::Open;
}

std::optional<Int3> Pathfinder::GetSafePoint(Int3 _pos, int _stepUp) {
	std::optional<Int3> found;

	if (GetVerticalOffset(_pos) == ColumnResult::Open) {
		found = _pos;
	} else if (_stepUp > 0) {
		Int3 stepped = _pos;
		stepped.y += _stepUp;
		if (GetVerticalOffset(stepped) == ColumnResult::Open)
			found = stepped;
	}

	if (!found)
		return std::nullopt;

	// Let the entity fall through open space below
	Int3 p = *found;
	ColumnResult below = ColumnResult::Blocked;
	int fallSteps = 0;

	while (p.y > 0) {
		Int3 belowPos = p;
		belowPos.y -= 1;
		below = GetVerticalOffset(belowPos);
		if (below != ColumnResult::Open)
			break;

		p.y -= 1;
		if (++fallSteps >= 4)
			return std::nullopt;
	}

	if (below == ColumnResult::Lava)
		return std::nullopt;

	return p;
}

// z+1, x-1, x+1, z-1
const Int2 CARDINAL_DIRS[4] = { { 0, 1 }, { -1, 0 }, { 1, 0 }, { 0, -1 } };

int Pathfinder::FindPathOptions(Int3 _current, Int3 _goal, float _maxDistance, Int3* _options) {
	int count = 0;

	// Stepping up a block is only allowed at all if there's headroom
	int stepUp = 0;
	Int3 headroom = _current;
	headroom.y += 1;
	if (GetVerticalOffset(headroom) == ColumnResult::Open)
		stepUp = 1;

	for (const auto& dir : CARDINAL_DIRS) {
		Int3 candidatePos = _current;
		candidatePos.x += dir.x;
		candidatePos.z += dir.y;

		auto safe = GetSafePoint(candidatePos, stepUp);
		if (!safe)
			continue;

		auto it = nodes.find(*safe);
		if (it != nodes.end() && it->second.closed)
			continue;

		if (safe->Distance(_goal) >= _maxDistance)
			continue;

		_options[count++] = *safe;
	}

	return count;
}

std::vector<Int3> Pathfinder::FindPath(Int3 _start, Int3 _goal, float _width, float _height, float _maxDistance) {
	Reset();

	footprint = { std::max(1, int(std::floor(_width + 1.0f))), std::max(1, int(std::floor(_height + 1.0f))),
		          std::max(1, int(std::floor(_width + 1.0f))) };

	Node* startNode = OpenNode(_start);
	startNode->g = 0.0f;
	startNode->f = _start.Distance(_goal);

	open.push({ startNode->f, startNode });

	// Tracks the closest to goal node seen so far
	Node* bestNode = startNode;
	float bestDistance = _start.Distance(_goal);

	Int3 options[4];

	while (!open.empty()) {
		Node* current = open.top().node;
		open.pop();

		if (current->closed)
			continue;

		current->closed = true;

		float distToGoal = current->pos.Distance(_goal);
		if (distToGoal < bestDistance) {
			bestDistance = distToGoal;
			bestNode = current;
		}

		if (current->pos == _goal)
			break;

		int optionCount = FindPathOptions(current->pos, _goal, _maxDistance, options);
		for (int i = 0; i < optionCount; i++) {
			Node* next = OpenNode(options[i]);
			if (next->closed)
				continue;

			float g = current->g + current->pos.Distance(options[i]);
			if (g < next->g) {
				next->g = g;
				next->f = g + options[i].Distance(_goal);
				next->parent = current;

				open.push({ next->f, next });
			}
		}
	}

	if (bestNode == startNode)
		return {};

	std::vector<Int3> path;

	for (Node* n = bestNode; n != startNode; n = n->parent)
		path.push_back(n->pos);

	std::reverse(path.begin(), path.end());

	return path;
}
