/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "entity_dummy_player.h"
#include "../player_conn/player_session_mp.h"
#include "damage_types.h"
#include "entities/entity.h"
#include "entities/entity_manager.h"
#include "entities/entity_mobile.h"
#include "helpers/aabb.h"
#include "inventory/inventories.h"
#include "inventory/item_stack.h"
#include "player/player_session.h"
#include <vector>

void DummyMPPlayer::Tick() {
	if (!session) {
		dummySession.connState = ConnectionState::Playing;
		session = &dummySession;
		return;
	}

	// Set our held item and armor
	// Slots 5 -> 8 are for armor
	ItemStack none = {};
	auto heldItemPtr = session->inventory.GetHeldItem();
	this->heldItem = session->inventory.GetHeldItem() ? *heldItemPtr : none;
	for (int i = 0; i < 4; i++) {
		auto armorSlotPtr = session->inventory.GetStackInSlot(5 + i);
		this->armor[i] = armorSlotPtr ? armorSlotPtr : nullptr;
	}

	// Do living entity stuff
	MobileEntity::Tick();

	// If we fell out of the world then die
	if (position.y < -64.0)
		OnDeath(nullptr, DamageType::Void);

	// Tell entities we collided with a player
	if (entityManager) {
		auto entitiesCollidingWith = entityManager->GetEntitiesWithinAabbExcluding(collider.Expand(1.0, 0.0, 1.0),
		                                                                           this->id);
		for (const auto& entity : entitiesCollidingWith) {
			if (!entity->isDead)
				entity->OnCollideWithPlayer(*this);
		}
	}
}