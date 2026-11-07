/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// This is the internal C++ implementation of the API.

#pragma once

#include "addon_api.h"
#include "inventory/item_stack.h"
#include <unordered_map>

class Entity;
class PlayerSession;
class WorldManager;

struct Addon;

struct bp_entity {
	Entity* entity;
};

struct bp_player {
	PlayerSession* session;
	std::unordered_map<Addon*, void*> addonData;
};

struct bp_world {
	WorldManager* manager;
};

namespace AddonHelper {

inline bp_item_stack ToBpStack(const ItemStack* _item) noexcept {
	return _item ? bp_item_stack{ _item->id, _item->count, _item->data } : bp_item_stack{ -1, 0, 0 };
}

inline void FromBpStack(ItemStack* _dst, const bp_item_stack& _src) noexcept {
	if (_dst) {
		_dst->id = _src.id;
		_dst->count = _src.count;
		_dst->data = _src.data;
	}
}

} // namespace AddonHelper

bp_api MakeAddonAPI();