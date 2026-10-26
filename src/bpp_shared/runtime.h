/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#pragma once
#include "crafting/recipe_manager.h"
#include "items/tool_item_properties.h"
#include "logger.h"
#include "platform/platform_caps.h"
#include "world/storage/region_manager.h"
#include "world/storage/save_manager.h"
#include "world/world.h"

// General game runtime that the client and server can use so that way we don't reuse a bunch of code and have to maintain it in two places.
struct Runtime {
	// Storage
	SaveManager saveManager;
	WorldManager world;
	WorldManager worldHell;
	RegionManager overworldRegionManager;
	RegionManager hellRegionManager; // hehe i call it hell instead of nether cause im quirky
	std::string currentLevelPath = "";

	// Gameplay
	RecipeManager recipeManager;

	// Each world shares an entity id counter
	EntityId sharedEntityId = 2;

	Runtime() : worldHell(true) {
		Blocks::RegisterAll();
		Items::RegisterAll();
		recipeManager.AddVanillaRecipes();
		world.entityManager.nextEntityId = &sharedEntityId;
		worldHell.entityManager.nextEntityId = &sharedEntityId;
		Logger::Info << "New game runtime created!\n";
	}

	void SetViewDistance(int _renderDistance) {
		world.SetViewRadius(_renderDistance);
		worldHell.SetViewRadius(_renderDistance);
		Logger::Info << "Render distance set to " << _renderDistance << " chunks!\n";
	}

	void CloseCurrentWorld() {
		if (currentLevelPath == "")
			return;

		world.Shutdown();
		worldHell.Shutdown();
		saveManager.Release();

		world.regionManager = nullptr;
		worldHell.regionManager = nullptr;
	}

	void Init(std::string _levelPath, std::string _seedOverride = "", int _renderDistance = BPP_DEFAULT_VIEW_DISTANCE) {
		// Override our view distance
		currentLevelPath = _levelPath;
		world.SetViewRadius(_renderDistance);
		worldHell.SetViewRadius(_renderDistance);
		Logger::Info << "Render distance set to " << _renderDistance << " chunks!\n";

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
				auto conversionResult = Utilities::convertAlphaLevel(_levelPath);

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

		// Initialize our region managers
		overworldRegionManager.Initialize(_levelPath + "/region");
		hellRegionManager.Initialize(_levelPath + "/DIM-1/region");

		// Bind our pointers
		overworldRegionManager.world = &world;
		hellRegionManager.world = &worldHell;

		// Initialize save data with our world objects
		saveManager.LoadLevelData();
		auto& thisLevelData = saveManager.GetLevelData();
		world.InitWorldSeed(thisLevelData.randomSeed);
		worldHell.InitWorldSeed(thisLevelData.randomSeed);

		// World time
		world.elapsedTicks = thisLevelData.time;
		worldHell.elapsedTicks = thisLevelData.time;

		// Initialize weather
		world.weatherSystem.isRaining = thisLevelData.raining;
		world.weatherSystem.isThundering = thisLevelData.thundering;
		world.weatherSystem.thunderTimer = thisLevelData.thunderTime;
		world.weatherSystem.rainTimer = thisLevelData.rainTime;

		// Bind the region managers with the world objects
		world.regionManager = &overworldRegionManager;
		worldHell.regionManager = &hellRegionManager;

		// If we created a new save then make a new spawn point
		if (newSave) {
			world.InitSpawn();
		} else {
			world.spawnPoint = saveManager.GetLevelData().spawnPoint;
		}
		worldHell.spawnPoint = world.spawnPoint; // Interestingly the world spawn doesn't have the /= or *= 8 stuff

		Logger::Info << "Game runtime initialized!\n";
	}
};