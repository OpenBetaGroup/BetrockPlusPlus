/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "../player_conn/player_session_mp.h"
#include "enums/blocks.h"
#include "runtime.h"
#include "world/world.h"
#include <numeric_structs.h>

// Behavioral overrides for the base block behaviors.
// If its nullptr here, use the base block overrides. Else, use what is here.
namespace ServerBlock {
struct BlockBehavior {
	bool (*onBlockActivated)(WorldManager& _world, Int3 _position, PlayerSessionMP& _session,
	                         Runtime& _gameRuntime) = nullptr;
};
extern BlockBehavior blockBehaviors[BLOCK_MAX];

void Initialize();
} // namespace ServerBlock