/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_lightning_bolt.h"
#include "blocks/block_behaviors/internal.h"
#include "world.h"

void LightningEntity::TryCatchFire() {
	if (!world)
		return;

	Int3 bpos = GetPositionAsBlockPosition(position);
	AABB colliderAsBlock = AABB{ double(bpos.x),       double(bpos.y),       double(bpos.z),
		                         double(bpos.x) + 1.0, double(bpos.y) + 1.0, double(bpos.z) + 1.0 };

	if (!this->world->AABBinValidChunks(colliderAsBlock.Expand(10.0, 0.0, 10.0)))
		return;

	// Set this block on fire
	if (world->GetBlockId(bpos) == BLOCK_AIR && Blocks::CanFireStay(*world, bpos)) {
		world->SetBlock(bpos, BLOCK_FIRE);
	}

	// Try and set surrounding blocks on fire
	for (int fireTry = 0; fireTry < 4; fireTry++) {
		Int3 newPos = bpos;
		newPos.x += rand.NextInt(3) - 1;
		newPos.y += rand.NextInt(3) - 1;
		newPos.z += rand.NextInt(3) - 1;

		if (world->GetBlockId(newPos) == BLOCK_AIR && Blocks::CanFireStay(*world, newPos)) {
			world->SetBlock(newPos, BLOCK_FIRE);
		}
	}
}

void LightningEntity::Tick() {
	Entity::Tick();

	lightningState--;
	if (lightningState < 0) {
		if (timeAlive <= 0) {
			this->isDead = true;
		} else if (lightningState < -rand.NextInt(10)) {
			timeAlive--;
			lightningState = 1;

			Int3 bpos = GetPositionAsBlockPosition(position);
			AABB colliderAsBlock = AABB{ double(bpos.x),       double(bpos.y),       double(bpos.z),
				                         double(bpos.x) + 1.0, double(bpos.y) + 1.0, double(bpos.z) + 1.0 };

			if (this->world->AABBinValidChunks(colliderAsBlock.Expand(10.0, 0.0, 10.0))) {
				// Set this block on fire
				if (world->GetBlockId(bpos) == BLOCK_AIR && Blocks::CanFireStay(*world, bpos)) {
					world->SetBlock(bpos, BLOCK_FIRE);
				}
			}
		}
	} else {
		double hurtRadius = 3.0;
		AABB hurtBox = { position.x - hurtRadius, position.y - hurtRadius,       position.z - hurtRadius,
			             position.x + hurtRadius, position.y + (hurtRadius * 3), position.z + hurtRadius };
		auto entities = world->entityManager.GetEntitiesWithinAabbExcluding(hurtBox, this->id);

		for (auto& entity : entities) {
			entity->OnStruckByLightning(*this);
		}
	}
}