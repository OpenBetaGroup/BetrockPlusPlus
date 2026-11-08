/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 * 
*/

#pragma once
#include <cstdint>

// Used primarily for determining what kind of damage killed a mob, for death messages

enum class DamageType : int8_t {
	None,
	Fall,
	Drown,
	Burn,
	Lava,
	Punch,
	Arrow,
	Explosion,
	Lightning,
	Suffocation,
	Command,
	Void,
	Cactus,
	Item
};