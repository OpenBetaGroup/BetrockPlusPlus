/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// The world manager acts like a wrapper around the chunk manager and lighting manager.
// It handles all world-related operations and provides a simple interface for the rest of the code to interact with the world.
#pragma once
#include "BS_thread_pool.hpp"
#include "base_structs.h"
#include "blocks.h"
#include "blocks/block_behaviors.h"
#include "blocks/block_properties_behaviors.h"
#include "chunk.h"
#include "client_pos.h"
#include "dimensions.h"
#include "entities/entity_manager.h"
#include "generator/overworld/biome_gen.h"
#include "helpers/aabb.h"
#include "helpers/explosion.h"
#include "java_math.h"
#include "lighter.h"
#include "managers/tick_scheduler.h"
#include "managers/weather_system.h"
#include "packet_data.h"
#include "tile_entities/tile_entity_manager.h"
#include "world/spawner.h"
#include "world/storage/region_manager.h"
#include "world_access.h"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "gamerules.h"

#ifdef BUILD_SERVER
#include "addon/addon_impl.h"
#endif

struct PendingBlock {
	Block block{ BLOCK_AIR, 0 };
	Int3 blockPos{ 0, 0, 0 };
	Int2 light{ 0, 15 }; // block light, sky light
};

// How many ticks make up a full day/night cycle
constexpr TickTime DAY_LENGTH = 24000;
// The window of time (relative to DAY_LENGTH) during which beds may be used
constexpr TickTime NIGHT_START_TICK = 12541;
constexpr TickTime NIGHT_END_TICK = 23458;

class WorldManager : public WorldAccess {
private:
	Int32_2 lastChunkPos = { 0, 0 };
	std::shared_ptr<Chunk>* lastChunkSlot = nullptr;
	std::unordered_map<Int32_2, std::vector<std::pair<Int3, Block>>> pendingBleedWrites;
	std::mutex genDoneMutex;
	std::deque<std::shared_ptr<Chunk>> genDoneQueue;
	TileEntityManager tileEntityManager;
	EntitySpawner entitySpawner;
	enum class DeferredUpdateType : uint8_t {
		Neighbors,
		Redstone
	};
	struct DeferredUpdate {
		DeferredUpdateType type;
		Int3 position;
		BlockType block;
		BlockType oldBlock;
	};
	std::deque<DeferredUpdate> deferredUpdates;
	bool frozen = false;
	bool replayingDeferred = false;
	static BiomeGenerator biomeGenerator;

public:
#ifdef BUILD_SERVER
	bp_world apiWorld;
#endif
	BS::thread_pool<> pool{ 2 };
	RegionManager* regionManager = nullptr;
	TickTime elapsedTicks = 0;
	int skylightOffset = 0;
	EntityManager entityManager;
	TickScheduler tickScheduler;
	Lighter lightManager;
	// Pos, size, unordered set of destroyed blocks
	std::function<void(Vec3, float, std::unordered_set<Int3>&, Entity*)> onExplosion;
	std::function<void(PendingBlock, Int32_2)> onBlockUpdate;
	std::function<void(PacketData::WorldEvent, Int3, int32_t, PlayerSession*)> onWorldEvent;
	std::function<void(Int3, int8_t, int8_t)> onNotePlay;
	// Lets the server veto an entity (player) modifying a block, e.g. for spawn protection
	std::function<bool(Entity&, Int3)> canModifyBlock;
	bool CanModify(Entity& _entity, Int3 _pos) const {
		return !canModifyBlock || canModifyBlock(_entity, _pos);
	}
	std::unordered_map<Int32_2, std::shared_ptr<Chunk>> chunks;
	Java::Random rand;
	int64_t seed = 0;
	Int3 spawnPoint{ 0, 0, 0 };
	Dimension thisDimension = Dimension::Overworld;
	WeatherSystem weatherSystem;

	WorldManager(bool _pIsHell = false)
#ifdef BUILD_SERVER
	    : apiWorld(this), isHell(_pIsHell)
#else
	    : isHell(_pIsHell)
#endif
	{
		entityManager.world = this;
		tickScheduler.world = this;
		if (isHell)
			thisDimension = Dimension::Nether;
		distHashCounter = uint32_t(rand.NextInt());
	}

	~WorldManager() override {}

	void Tick(const std::vector<ClientPosition>& _players, bool _advanceSimulation = true,
	          TickProfiler* _profiler = nullptr);
	void Update(const std::vector<ClientPosition>& _players);
	void SaveChunks(const bool _saveIfEntities = false, const bool _deleteEntities = false);
	void Shutdown();
	void SeedChunkLighting(Int32_2 _pos);
	void PerformRandomTicks(const std::vector<ClientPosition>& _players);
	std::vector<AABB> GetCollidingBoundingBoxes(const AABB& _area, Entity* _mover = nullptr);
	void FlushBleedWrites();
	void PropagateChunkLightBorders(Int32_2 _cpos);
	BlockType GetFirstUncoveredBlock(int _wx, int _wz);
	int FindTopSolidBlock(int _wx, int _wz) override;
	void SetMeta(const Int3 _wpos, const uint8_t _metadata = 0, const bool _updateNeighbors = true);
	void SetBlock(const Int3 _wpos, const BlockType _blockType, const uint8_t _metadata = 0,
	              const bool _keepTileEntity = false, const bool _updateNeighbors = true) override;
	void SetBlockRaw(const Int3 _wpos, const BlockType _blockType, const uint8_t _metadata = 0);
	void FillVolume(Int3 _posA, Int3 _posB, BlockType _type = BLOCK_AIR, uint8_t _meta = 0);
	void NotifyRegionChanged(Chunk& _chunk, Int3 _localMin, Int3 _localMax);
	void DrainGenQueue();
	bool IsLiquidInAabb(const AABB& _collider);
	void InitSpawn();
	bool HandleFluidAcceleration(const AABB& _collider, const Material& _material, Entity& _entity);
	bool IsMaterialInAabb(const AABB& _collider, const Material& _material);
	bool IsAabbInFluidLevel(const AABB& _collider, const Material& _material);
	void UpdateLoadRadius(const std::vector<ClientPosition>& _players);
	void PumpPipeline(const std::vector<ClientPosition>& _players);
	void PopulateReady(int _maxPopulates = 16);
	void DrainLoadQueue();
	void DropInventory(Inventory& inventory, Int3 _wpos);
	void UpdateSkylightOffset();
	float GetCelestialAngle();
	int GetBlockLightValue(Int3 _wpos, bool _offsetNonFullBlocks = true);
	Biome GetBiome(Int2 _wpos);
	void EnsureClimate(Chunk& _chunk);
	BlockType GetBlockId(Int3 _wpos) override;
	uint8_t GetMetadata(Int3 _wpos);
	void RemoveTileEntity(Int3 _pos);
	void SetViewRadius(int _viewRadius);
	void NotifyNeighborsOfUpdate(Int3 _globalPos, BlockType _blockId);
	void SetFrozen(bool _frozen) noexcept {
		frozen = _frozen;
	}
	// For creating a fresh tile entity for generation etc
	void CreateTileEntity(std::shared_ptr<TileEntity> _tileEntity);
	// For registering a tile entity that already exists in the world (e.g. loaded from disk)
	void RegisterChunkTileEntities(Chunk* _chunk);
	// Returns the tile entity at world position `pos`, or nullptr if none.
	TileEntity* GetTileEntity(Int3 _pos);

	// Plays the note packet to the client
	void PlayNoteAt(Int3 _pos, int8_t _instrumentState, int8_t _pitchDirection) const {
		if (onNotePlay)
			onNotePlay(_pos, _instrumentState, _pitchDirection);
	}

	// Check if this AABB is spawnable
	bool IsAABBClear(AABB& _collider) {
		auto entities = entityManager.GetEntitiesWithinAabb(_collider);
		for (auto& entity : entities) {
			if (!entity->isDead && entity->preventEntitySpawning)
				return false;
		}
		return true;
	}

	bool CanRainHitSpot(Int3 _pos) {
		if (!weatherSystem.IsRaining())
			return false;
		if (!CanBlockSeeSky(_pos))
			return false;
		if (FindTopSolidBlock(_pos.x, _pos.z) > _pos.y)
			return false;
		return BiomeGenerator::BiomeHasLightning(GetBiome({ _pos.x, _pos.z }));
	}

	bool IsNight() const {
		TickTime relativeTime = elapsedTicks % DAY_LENGTH;
		return relativeTime >= NIGHT_START_TICK && relativeTime < NIGHT_END_TICK;
	}

	bool IsOpenGroundSpot(Int3 _pos) {
		return IsBlockNormalCube(_pos.WithOffset(Cardinal::Value::Down)) && IsAirBlock(_pos) &&
		       IsAirBlock(_pos.WithOffset(Cardinal::Value::Up));
	}

	void DoExplosion(Entity* _exploder, Vec3 _position, float _size, bool _doFire) {
		auto result = Explosion::DoExplosion(*this, _exploder, _position, _size, _doFire);
		if (onExplosion)
			onExplosion(_position, _size, result, _exploder);
	}

	bool CanBlockSeeSky(Int3 _pos) {
		auto chunk = GetChunkRaw({ _pos.x >> 4, _pos.z >> 4 });
		if (!chunk)
			return false;
		Int3 localPos = { _pos.x & 15, _pos.y, _pos.z & 15 };

		return chunk->CanBlockSeeSky(localPos);
	}

	int GetBlockLightFull(Int3 _wpos) {
		auto chunk = GetChunkRaw({ _wpos.x >> 4, _wpos.z >> 4 });
		if (!chunk)
			return 15;

		Int3 localPos = { _wpos.x & 15, _wpos.y, _wpos.z & 15 };
		int skylight = chunk->GetSkyLight(localPos) - this->skylightOffset;
		int blockLight = chunk->GetBlockLight(localPos);
		if (blockLight > skylight)
			return blockLight;
		else
			return skylight;
	}

	int GetBlockLightRaw(Int3 _wpos) {
		auto chunk = GetChunkRaw({ _wpos.x >> 4, _wpos.z >> 4 });
		if (!chunk)
			return 15;

		Int3 localPos = { _wpos.x & 15, _wpos.y, _wpos.z & 15 };
		int skylight = chunk->GetSkyLight(localPos);
		int blockLight = chunk->GetBlockLight(localPos);
		if (blockLight > skylight)
			return blockLight;
		else
			return skylight;
	}

	bool IsDay() const {
		return skylightOffset < 4;
	}

	int GetViewRadius() const {
		return viewRadius;
	}
	int GetSimulationDistance() const {
		return simulationRadius;
	}
	Dimension GetDimension() const {
		return thisDimension;
	}
	void InitWorldSeed(std::string _pSeed) {
		InitWorldSeed(HashCode(_pSeed));
	}
	void InitWorldSeed(int64_t _pSeed) {
		seed = _pSeed;
		// Update the biome generator with this seed
		if (!isHell)
			biomeGenerator = BiomeGenerator(seed);
	}

	// Returns nullptr if not found or wrong type.
	template <typename T>
	T* GetTileEntityAs(Int3 _pos) {
		return dynamic_cast<T*>(GetTileEntity(_pos));
	}

	template <typename T>
	std::shared_ptr<T> GetTileEntityShared(Int3 _pos) {
		Chunk* chunk = GetChunkRaw({ _pos.x >> 4, _pos.z >> 4 });
		if (!chunk)
			return nullptr;
		for (auto& te : chunk->tileEntities) {
			if (te && te->position.x == _pos.x && te->position.y == _pos.y && te->position.z == _pos.z)
				return std::dynamic_pointer_cast<T>(te);
		}
		return nullptr;
	}

	// Called from pool gen threads
	void PostGenResult(std::shared_ptr<Chunk> _chunk) {
		std::lock_guard lk(genDoneMutex);
		genDoneQueue.push_back(std::move(_chunk));
	}

	std::shared_ptr<Chunk> GetChunk(Int32_2 _pos) {
		return GetChunkShared(_pos);
	}

	bool CanPopulate(Int32_2 _pos) {
		return CanPopulateDirect(_pos);
	}

	void SetBlock(Int3 _wpos, const Block& _block) {
		SetBlock(_wpos, _block.type, _block.data);
	}

	bool IsAirBlock(Int3 _wpos) {
		return GetBlockId(_wpos) == BlockType::BLOCK_AIR;
	}

	bool IsBlockNormalCube(Int3 _wpos) {
		BlockType block = GetBlockId(_wpos);
		if (block == BlockType::BLOCK_AIR)
			return false;

		const auto& props = Blocks::blockProperties[block];
		return props.material.isSolid && props.material.isOpaque && props.isNormalCube;
	}

	Int3 GetSpawnPoint(bool _adjust) {
		if (!_adjust)
			return spawnPoint;
		int sx = this->spawnPoint.x;
		int sz = this->spawnPoint.z;
		const int32_t radius = Gamerule::Get<int32_t>(Gamerule::Id::RespawnRadius);
		if (radius > 0) {
			sx += rand.NextInt(radius*2) - radius;
			sz += rand.NextInt(radius*2) - radius;
		}
		int sy = FindTopSolidBlock(sx, sz);
		return { sx, sy, sz };
	}

	int GetHeightValue(int _wx, int _wz) {
		auto* chunk = GetChunkRaw({ _wx >> 4, _wz >> 4 });
		if (!chunk || chunk->state.load() < ChunkState::Generated)
			return 0;
		return chunk->GetHeightValue({ _wx & 15, _wz & 15 });
	}

	// Returns the baked temperature/humidity for a world column.
	double GetTemperatureAt(int _wx, int _wz) {
		auto* chunk = GetChunkRaw({ _wx >> 4, _wz >> 4 });
		if (!chunk || chunk->state.load() < ChunkState::Generated)
			return 0.5;
		EnsureClimate(*chunk);
		return double(chunk->GetTemperature({ _wx & 15, _wz & 15 }));
	}

	double GetHumidityAt(int _wx, int _wz) {
		auto* chunk = GetChunkRaw({ _wx >> 4, _wz >> 4 });
		if (!chunk || chunk->state.load() < ChunkState::Generated)
			return 0.5;
		EnsureClimate(*chunk);
		return double(chunk->GetHumidity({ _wx & 15, _wz & 15 }));
	}

	uint8_t GetSkyLight(const Int3 _pos) override {
		if (!InBounds(_pos.y))
			return 0;
		auto* chunk = GetChunkRaw({ _pos.x >> 4, _pos.z >> 4 });
		if (!chunk || chunk->state.load() < ChunkState::Generated)
			return 0;
		return chunk->GetSkyLight({ _pos.x & 15, _pos.y, _pos.z & 15 });
	}

	uint8_t GetBlockLight(const Int3 _pos) {
		if (!InBounds(_pos.y))
			return 0;
		auto* chunk = GetChunkRaw({ _pos.x >> 4, _pos.z >> 4 });
		if (!chunk || chunk->state.load() < ChunkState::Generated)
			return 0;
		return chunk->GetBlockLight({ _pos.x & 15, _pos.y, _pos.z & 15 });
	}

	void InvalidateChunkCache() {
		lastChunkSlot = nullptr;
	}

	Chunk* GetChunkRaw(Int32_2 _pos) {
		auto* slot = FindChunkSlot(_pos);
		return slot ? slot->get() : nullptr;
	}

	bool IsChunkValid(Chunk* _chunk) {
		if (!_chunk)
			return false;
		if (_chunk->state.load() >= ChunkState::Generated)
			return true;
		return false;
	}

	bool AABBinValidChunks(const AABB& _collider) {
		int minCX = MathHelper::FloorDouble(_collider.minX) >> 4;
		int maxCX = MathHelper::FloorDouble(_collider.maxX + 1.0) >> 4;
		int minCZ = MathHelper::FloorDouble(_collider.minZ) >> 4;
		int maxCZ = MathHelper::FloorDouble(_collider.maxZ + 1.0) >> 4;

		for (int cx = minCX; cx <= maxCX; cx++) {
			for (int cz = minCZ; cz <= maxCZ; cz++) {
				if (!IsChunkValid(GetChunkRaw({ cx, cz })))
					return false;
			}
		}
		return true;
	}

	Int32_2 BlockToChunkPos(Int32_2 _blockPos) {
		return { _blockPos.x >> 4, _blockPos.z >> 4 };
	}

	// Returns true when the world-space Y is within valid chunk bounds.
	static constexpr bool InBounds(int _y) {
		return _y >= 0 && _y < CHUNK_HEIGHT;
	}

private:
	// I believe the vanilla default is
	int viewRadius = 9;
	int simulationRadius = 9;
	uint32_t distHashCounter = 0;
	bool isHell = false; // for the nether

	std::shared_ptr<Chunk>* FindChunkSlot(Int32_2 _pos) {
		if (lastChunkSlot && _pos == lastChunkPos)
			return lastChunkSlot;
		auto it = chunks.find(_pos);
		if (it == chunks.end())
			return nullptr;
		lastChunkSlot = &it->second;
		lastChunkPos = _pos;
		return lastChunkSlot;
	}

	std::shared_ptr<Chunk> GetChunkShared(Int32_2 _pos) {
		auto* slot = FindChunkSlot(_pos);
		return slot ? *slot : nullptr;
	}

	// Check if a chunk can be populated
	bool CanPopulateDirect(Int32_2 _pos) {
		auto* chunk = GetChunkRaw(_pos);
		if (!chunk)
			return false;
		if (chunk->isTerrainPopulated)
			return false;
		if (chunk->state.load() < ChunkState::Generated)
			return false;
		auto* a = GetChunkRaw({ _pos.x + 1, _pos.z });
		auto* b = GetChunkRaw({ _pos.x, _pos.z + 1 });
		auto* c = GetChunkRaw({ _pos.x + 1, _pos.z + 1 });
		if (!a || !b || !c)
			return false;
		if (a->state.load() < ChunkState::Generated)
			return false;
		if (b->state.load() < ChunkState::Generated)
			return false;
		if (c->state.load() < ChunkState::Generated)
			return false;
		return true;
	}
};