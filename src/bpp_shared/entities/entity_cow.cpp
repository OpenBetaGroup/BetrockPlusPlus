/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 */
#include "entity_cow.h"
#include "enums/items.h"
#include "java_random.h"

void CowEntity::OnDeath(Entity* /*_killer*/, DamageType /*_type*/) {
	// Drop 0-2 leather
	auto targetItem = Items::Id::LEATHER;
	int itemCount = this->rand.NextInt(3);

	for (int i = 0; i < itemCount; i++) {
		DropItemAtEntity(targetItem, 1);
	}
}
