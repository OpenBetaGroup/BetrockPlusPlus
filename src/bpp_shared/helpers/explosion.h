/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/
#pragma once
#include "java/java_math.h"
#include "java/java_random.h"
#include <numeric_structs.h>
#include <unordered_set>

class WorldManager;
struct Entity;

// for handling explosions
namespace Explosion {

// This function destroys the blocks but does NOT create particle effects / sounds. That is the callers responsibility.
std::unordered_set<Int3> DoExplosion(WorldManager& _world, Entity* _exploder, Vec3 _position, float _size, bool _doFire);

} // namespace Explosion