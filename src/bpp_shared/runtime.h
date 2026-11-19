/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "base_types.h"
#include "crafting/recipe_manager.h"
#include "dimensions.h"
#include "items/item_properties.h"
#include "platform/platform_caps.h"
#include "spawner.h"
#include "tick_profiler.h"
#include "world/dimension/dimension.h"
#include "world/storage/save_manager.h"
#include "world/world.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct PlayerSession;

// General game runtime that the client and server can use so that way we don't reuse a bunch of code and have to maintain it in two places.
struct Runtime {
	// Storage
	SaveManager saveManager;
	std::string currentLevelPath = "";
	std::unordered_map<Dimension, std::unique_ptr<WorldManager>> dimensions;

	// Gameplay
	RecipeManager recipeManager;

	// Each world shares an entity id counter
	EntityId sharedEntityId = 2;

#ifdef BUILD_SERVER
	Server* server;
	Runtime(Server* _server);
#else
	Runtime();
#endif

	void InitializeDimension(std::unique_ptr<DimensionBase> _dimension);

	// Apply a function to a world with the world being the first paramater of the function
	void RunFunctionOnAllDimensions(const std::function<void(WorldManager*)>& _dim);

	WorldManager* GetWorldForDimension(Dimension _dim);

	void SetViewDistance(int _renderDistance);

	void SetSeed(int64_t _seed);

	void CloseCurrentWorld();

	void Tick(std::vector<std::shared_ptr<PlayerSession>>& _players, bool _advanceSimulation = true,
	          TickProfiler* profiler = nullptr);

	void Init(std::string _levelPath, std::string _seedOverride = "", int _renderDistance = BPP_DEFAULT_VIEW_DISTANCE);

private:
	void InitializeWorld(WorldManager* _world, LevelData& _levelData, std::string _levelPath);
};