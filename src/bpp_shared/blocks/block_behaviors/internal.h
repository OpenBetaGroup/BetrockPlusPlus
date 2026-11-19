/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "blocks/block_behaviors.h"
#include "direction.h"
#include "numeric_structs.h"

class WorldManager;

namespace Blocks {

void RegisterFluidBehaviors();
void RegisterRailBehaviors();
void RegisterRedstoneBehaviors();
void RegisterPlantBehaviors();
void RegisterDoorBehaviors();
void RegisterBedBehaviors();
void RegisterChestBehaviors();
void RegisterFallingBlockBehaviors();
void RegisterMiscBehaviors();
void RegisterFireBehaviors();
void RegisterBlockDrops();
void RegisterPistonBehaviors();
void RegisterPortalBehaviors();

// Defined in common.cpp
bool IsReplaceable(WorldManager& _world, Int3 _pos);
bool IsSupported(WorldManager& _world, Int3 _pos, Cardinal::Value _dir);

// Defined in plants.cpp
void TryGrowTree(WorldManager& _world, Int3 _pos);

// Defined in portal.cpp
void TryCreatePortal(WorldManager& _world, Int3 _pos);

// Defined in fire.cpp
bool CanBlockCatchFire(WorldManager& _world, Int3 _pos);
bool CanNeighborBurn(WorldManager& _world, Int3 _pos);
bool CanFireStay(WorldManager& _world, Int3 _pos);
}; // namespace Blocks
