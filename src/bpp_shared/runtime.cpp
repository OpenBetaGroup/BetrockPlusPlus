/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "runtime.h"
#include "blocks/block_properties_behaviors.h"
#include "entities/entity_manager.h"
#include "items/tool_item_properties.h"
#include "java_random.h"
#include "logger.h"
#include "logstream.h"
#include "managers/weather_system.h"
#include "player/player_session.h"
#include "utilities.h"
#include "world.h"
#include "world/dimension/nether/dimension_nether.h"
#include "world/dimension/overworld/dimension_overworld.h"
#include "world/storage/region_manager.h"
#include "world/storage/save_manager.h"
#include <cstdlib>
#include <utility>

#ifdef BUILD_SERVER
Runtime::Runtime(Server* _server) : server(_server) {
#else
Runtime::Runtime() {
#endif
	Blocks::RegisterAll();
	Items::RegisterAll();
	recipeManager.AddVanillaRecipes();
	InitializeDimension(std::make_unique<DimensionOverworld>(0));
	InitializeDimension(std::make_unique<DimensionNether>(0));
	Logger::Info << "New game runtime created!\n";
}

void Runtime::InitializeDimension(std::unique_ptr<DimensionBase> _dimension) {
	// Make sure we don't already have this dimensions registered
	Dimension id = _dimension->id;
	auto it = dimensions.find(id);
	if (it != dimensions.end()) {
		Logger::Warning << "Tried to register dimension " << int(id) << ", but it already exists! Ignoring...\n";
		return;
	}

	// Set up our world
#ifdef BUILD_SERVER
	auto newWorld = std::make_unique<WorldManager>(server);
#else
	auto newWorld = std::make_unique<WorldManager>();
#endif
	newWorld->entityManager.nextEntityId = &sharedEntityId;
	newWorld->dimension = std::move(_dimension);
	newWorld->regionManager = std::make_unique<RegionManager>();
	dimensions[id] = std::move(newWorld);

	Logger::Info << "Initialized dimension " << int(dimensions[id]->dimension->id) << "\n";
}

void Runtime::RunFunctionOnAllDimensions(const std::function<void(WorldManager*)>& _dim) {
	for (auto& it : dimensions) {
		_dim(it.second.get());
	}
}

WorldManager* Runtime::GetWorldForDimension(Dimension _dim) {
	auto it = dimensions.find(_dim);
	if (it == dimensions.end()) {
		return nullptr;
	}
	return dimensions[_dim].get();
}

void Runtime::SetViewDistance(int _renderDistance) {
	for (auto& it : dimensions) {
		it.second->SetViewRadius(_renderDistance);
	}
	Logger::Info << "Render distance set to " << _renderDistance << " chunks!\n";
}

void Runtime::SetSeed(int64_t _seed) {
	for (auto& it : dimensions) {
		it.second->InitWorldSeed(_seed);
	}
}

void Runtime::CloseCurrentWorld() {
	if (currentLevelPath == "")
		return;

	// Shutdown all our worlds
	for (auto& it : dimensions) {
		it.second->Shutdown();
	}

	// Release the save
	saveManager.Release();
}

void Runtime::Tick(std::vector<std::shared_ptr<PlayerSession>>& _players, bool _advanceSimulation,
                   TickProfiler* profiler) {
	// Build our sorted player sets
	std::unordered_map<Dimension, std::vector<std::shared_ptr<PlayerSession>>> activePlayers;
	for (auto& it : dimensions) {
		activePlayers.try_emplace(it.first);
	}
	for (auto& player : _players) {
		activePlayers[player->dimension].push_back(player);
	}

	// Tick each world
	for (auto& it : dimensions) {
		auto dimension = it.first;
		auto& world = it.second;

		auto& thisPlayers = activePlayers[dimension];
		if (thisPlayers.size() <= 0)
			continue;

		world->Tick(thisPlayers, _advanceSimulation, profiler);
		world->Update(thisPlayers);
	}
}

void Runtime::Init(std::string _levelPath, std::string _seedOverride, int _renderDistance) {
	// Override our view distance
	currentLevelPath = _levelPath;
	SetViewDistance(_renderDistance);

	// Setup our save
	bool newSave = false;
	auto initResult = saveManager.Initialize(_levelPath);
	if (initResult != LevelInitFailureReason::SUCCESS) {
		auto tryNewSave = [&]() -> void {
			Logger::Warning << "**** FAILED TO LOAD WORLD DATA! Attempting to create new world... \n";
			newSave = true;
			if (!saveManager.CreateNewWorld({ .randomSeed = (_seedOverride != "")
			                                                    ? saveManager.SeedFromString(_seedOverride)
			                                                    : Java::Random().NextLong() })) {
				Logger::Error << "**** FAILED TO CREATE NEW WORLD! \n";
				exit(1);
			}
			Logger::Info << "New world created successfully. \n";
		};

		if (initResult == LevelInitFailureReason::ALPHA_FORMATTED) {
			Logger::Info << "Alpha Formatted world detected! Attempting to convert...\n";
			saveManager.Release();
#ifdef BUILD_SERVER
			auto conversionResult = Utilities::convertAlphaLevel(_levelPath, server);
#else
			auto conversionResult = Utilities::convertAlphaLevel(_levelPath);
#endif

			if (!conversionResult) {
				// We failed to convert
				Logger::Info << "Failed to convert Alpha world! Falling back.\n";
				tryNewSave();
			} else {
				// Re-init our level
				if (saveManager.Initialize(_levelPath) != LevelInitFailureReason::SUCCESS) {
					Logger::Error << "Converted level failed to load!";
					tryNewSave();
				}
			}
		} else {
			tryNewSave();
		}
	}

	// Initialize save data with our world objects
	saveManager.LoadLevelData();
	auto& thisLevelData = saveManager.GetLevelData();
	for (auto& it : dimensions) {
		auto& uPtr = it.second;
		InitializeWorld(uPtr.get(), thisLevelData, _levelPath);
	}

	// If we created a new save then make a new spawn point
	auto overworld = GetWorldForDimension(Dimension::Overworld);
	if (overworld && newSave) {
		overworld->InitSpawn();
	} else {
		overworld->spawnPoint = thisLevelData.spawnPoint;
	}

	// Register the same spawn point for each dimension
	for (auto& it : dimensions) {
		it.second->spawnPoint = overworld->spawnPoint;
	}

	Logger::Info << "Game runtime initialized!\n";
}

void Runtime::InitializeWorld(WorldManager* _world, LevelData& _levelData, std::string _levelPath) {
	// Overworld has a special case for its level folder
	if (_world->dimension->id == Dimension::Overworld) {
		_world->regionManager->Initialize(_levelPath + "/region");
	} else {
		_world->regionManager->Initialize(_levelPath + "/DIM" + std::to_string(int(_world->dimension->id)) + "/region");
	}
	_world->regionManager->world = _world;

	// Setup game state
	_world->InitWorldSeed(_levelData.randomSeed);
	_world->elapsedTicks = _levelData.time;

	// Initialize weather
	_world->weatherSystem.isRaining = _levelData.raining;
	_world->weatherSystem.isThundering = _levelData.thundering;
	_world->weatherSystem.thunderTimer = _levelData.thunderTime;
	_world->weatherSystem.rainTimer = _levelData.rainTime;
}