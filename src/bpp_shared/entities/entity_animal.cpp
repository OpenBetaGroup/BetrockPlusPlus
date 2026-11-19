/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_animal.h"
#include "blocks.h"
#include "entity_mobile.h"
#include "helpers/aabb.h"
#include "helpers/java/java_math.h"
#include "numeric_structs.h"
#include "world.h"

bool AnimalEntity::CanSpawnAt() {
	if (!world)
		return false;
	auto fd = MathHelper::FloorDouble;
	Int3 footPos = { fd(position.x), fd(collider.minY), fd(position.z) };
	return world->GetBlockId({ footPos.x, footPos.y - 1, footPos.z }) == BLOCK_GRASS &&
	       world->GetBlockLightRaw(footPos) > 8 && MobileEntity::CanSpawnAt();
}