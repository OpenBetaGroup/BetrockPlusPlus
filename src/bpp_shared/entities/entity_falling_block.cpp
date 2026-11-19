/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_falling_block.h"
#include "blocks/block_properties.h"
#include "blocks/materials.h"
#include "constants.h"
#include "direction.h"
#include "java_math.h"
#include "world/world.h"

void FallingBlockEntity::Tick() {
	if (block == BLOCK_AIR) {
		isDead = true;
		return;
	}

	ticksFallen++;
	velocity.y -= 0.04;
	Move(this->velocity);
	velocity *= { 0.98, 0.98, 0.98 };

	auto fd = MathHelper::FloorDouble;
	Int3 blockPosition = { fd(position.x), fd(position.y), fd(position.z) };

	// Remove the block at our position
	if (this->world->GetBlockId(blockPosition) == this->block)
		this->world->SetBlock(blockPosition, BLOCK_AIR, 0);

	if (onGround) {
		velocity *= { 0.7, -0.5, 0.7 };
		isDead = true;

		BlockType target = this->world->GetBlockId(blockPosition);
		bool placeable = blockPosition.y > 0 && blockPosition.y < CHUNK_HEIGHT &&
		                 (target == BLOCK_AIR || Blocks::blockProperties[target].material.isLiquid);

		bool unsupported = Blocks::CanFallAt(*this->world, blockPosition.WithOffset(Cardinal::Value::Down));

		if (!placeable || unsupported) {
			this->DropItemAtEntity(this->block, 1);
			return;
		}
		this->world->SetBlock(blockPosition, this->block, 0);
		return;
	}

	if (ticksFallen > 100) {
		this->DropItemAtEntity(this->block, 1);
		isDead = true;
	}
}