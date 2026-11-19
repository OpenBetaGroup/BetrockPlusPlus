/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_zombie.h"
#include "enums/items.h"
#include "java_random.h"

void ZombieEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	// Drop 0-2 feathers
	auto targetItem = Items::Id::FEATHER;
	int itemCount = this->rand.NextInt(3);

	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(targetItem, 1);
	}
}