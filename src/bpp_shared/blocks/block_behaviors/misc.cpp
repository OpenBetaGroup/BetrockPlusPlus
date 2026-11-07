/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "blocks.h"
#include "blocks/block_behaviors.h"
#include "blocks/block_properties.h"
#include "dimensions.h"
#include "entities/entity_falling_block.h"
#include "entities/entity_player.h"
#include "entities/entity_skeleton.h"
#include "entities/entity_spider.h"
#include "entities/entity_tnt.h"
#include "entities/entity_zombie.h"
#include "enums/items.h"
#include "generator/overworld/tree_gen.h"
#include "helpers/direction_fixer.h"
#include "helpers/block_volume.h"
#include "helpers/java/java_math.h"
#include "internal.h"
#include "items/item_properties.h"
#include "logger.h"
#include "numeric_structs.h"
#include "packet_data.h"
#include "managers/rail_manager.h"
#include "managers/redstone_manager.h"
#include "managers/tick_scheduler.h"
#include "tile_entities/tile_entity.h"
#include "world.h"

namespace Blocks {
static void IgniteTnt(WorldManager& _world, Int3 _pos, int _fuse = 80) {
	auto tnt = std::make_shared<TntEntity>(Vec3{ _pos.x + 0.5, _pos.y + 0.5, _pos.z + 0.5 });
	tnt->fuse = _fuse;
	_world.entityManager.AddEntity(tnt);
}

static int GetDirectionFromYaw(float _yaw, int _directionCount) {
	return MathHelper::FloorDouble((_yaw * _directionCount / 360.0f) + 0.5f) & 3;
}

static bool EatCake(WorldManager& _world, Int3 _pos, PlayerSession* _triggeringSession) {
	if (!_triggeringSession || !_triggeringSession->entity)
		return false;
	if (_triggeringSession->entity->health >= _triggeringSession->entity->maxHealth)
		return false;
	auto meta = _world.GetMetadata(_pos);
	if (meta >= 6) {
		_world.SetBlock(_pos, BLOCK_AIR);
		return false;
	}
	_world.SetMeta(_pos, ++meta);
	_triggeringSession->entity->Heal(Items::GetRegenerationAmount(BLOCK_CAKE));
	return false;
}

void RegisterMiscBehaviors() {
	// Vanilla sponge removal notifies every position in a 5x5x5 region. Like coral!
	blockBehaviors[BLOCK_SPONGE].onBlockRemoval = [](WorldManager& _world, Int3 _pos) {
		ForEachBlockInVolume(_pos, 2, [&](Int3 _blockPos) {
			_world.NotifyNeighborsOfUpdate(_blockPos, _world.GetBlockId(_blockPos));
		});
	};

	blockBehaviors[BlockType::BLOCK_COBWEB] = {
		.getCollider = EmptyCollider,
	};

	blockBehaviors[BlockType::BLOCK_SLAB] = {
		.getSelectionBox = SlabAabb,
		.getRayBounds = SlabAabb,
		.getCollider = SlabCollider,
	};

	blockBehaviors[BlockType::BLOCK_STAIRS_WOOD] = {
		.getCollider = StairCollider,
		// ray/selection stay as defaultAABB
	};
	blockBehaviors[BlockType::BLOCK_STAIRS_COBBLESTONE] = {
		.getCollider = StairCollider,
	};

	blockBehaviors[BlockType::BLOCK_SNOW_LAYER] = {
		.getRayBounds = SnowLayerAabb,
		.getCollider = SnowLayerCollider,
		// getSelectionBox stays defaultAABB
	};

	blockBehaviors[BlockType::BLOCK_LADDER] = {
		.getSelectionBox = LadderAabb,
		.getRayBounds = LadderAabb,
		.getCollider = LadderCollider,
	};

	blockBehaviors[BlockType::BLOCK_FENCE] = {
		.getCollider = FenceCollider,
		// ray/selection stay as defaultAABB (full cube)
	};

	blockBehaviors[BlockType::BLOCK_CAKE] = { .getSelectionBox = CakeAabb,
		                                      .getRayBounds = CakeAabb,
		                                      .getCollider = CakeCollider,
		                                      .onBlockActivated = EatCake };

	blockBehaviors[BLOCK_SOULSAND] = {
		.getCollider = SoulSandCollider,
	};

	// Cobweb
	blockBehaviors[BLOCK_COBWEB].onEntityCollidedWithBlock = [](WorldManager& /*_world*/, Int3 /*_pos*/,
	                                                            Entity& _entity) -> void {
		_entity.inWeb = true;
	};

	// Soulsand
	blockBehaviors[BLOCK_SOULSAND].onEntityCollidedWithBlock = [](WorldManager& /*_world*/, Int3 /*_pos*/,
	                                                              Entity& _entity) -> void {
		_entity.velocity.x *= 0.4;
		_entity.velocity.z *= 0.4;
	};

	// Shared overrides
	auto onFurnaceDispenserPlace = [](WorldManager& _world, Int3 _pos, Entity& _placer, Cardinal::Value _face,
	                                  BlockType _blockId, uint8_t /*_meta*/) -> bool {
		int meta[] = { 2, 5, 3, 4 };
		return GenericPlace(_world, _pos, _placer, _face, _blockId, meta[GetDirectionFromYaw(_placer.rotationYaw, 4)]);
	};

	// Fence
	blockBehaviors[BLOCK_FENCE].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                               Cardinal::Value _face, BlockType _blockId, uint8_t _meta) -> bool {
		const bool fenceBelow = (_world.GetBlockId(_pos.WithOffset(Cardinal::Value::Down)) == BLOCK_FENCE);

		if (!fenceBelow && !_world.GetMaterial(_pos.WithOffset(Cardinal::Value::Down)).isSolid)
			return false;

		return GenericPlace(_world, _pos, _placer, _face, _blockId, _meta);
	};

	// Slabs
	blockBehaviors[BLOCK_SLAB].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                              Cardinal::Value _face, BlockType _blockId, uint8_t _meta) -> bool {
		auto sourcePos = _pos.WithOffset(Cardinal::Opposite(_face));
		auto sourceBlock = _world.GetBlockId(sourcePos);
		auto sourceMeta = _world.GetMetadata(sourcePos);
		if (sourceBlock == BLOCK_SLAB && _face == Cardinal::Value::Up && sourceMeta == _meta) {
			// Vanilla does this weird but this seems to match its behavior
			auto slabPlacePos = CanPlace(_world, _pos, _placer, _face, BLOCK_SLAB, _meta);
			if (slabPlacePos != Int3{ -1, -1, -1 }) {
				_world.SetBlock(sourcePos, BLOCK_AIR, /*meta=*/0, /*KeepTileEntity=*/true, /*updateNeighbors=*/false);
				bool successfulPlace = GenericPlace(_world, sourcePos, _placer, _face, BLOCK_DOUBLE_SLAB, _meta);
				if (successfulPlace)
					return true;

				_world.SetBlock(sourcePos, BLOCK_SLAB, _meta, /*KeepTileEntity=*/true, /*updateNeighbors=*/false);
			}
		}
		return GenericPlace(_world, _pos, _placer, _face, _blockId, _meta);
	};

	// Mob spawner
	blockBehaviors[BLOCK_MOB_SPAWNER].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		auto spawnerTileEntity = std::make_shared<TileEntityMobSpawner>(_pos);
		_world.CreateTileEntity(std::move(spawnerTileEntity));
	};

	// Snow
	blockBehaviors[BLOCK_SNOW_LAYER].onBlockDestroyedByPlayer = [](WorldManager& _world, Int3 _pos,
	                                                               Entity& /*_destroyer*/) {
		// Snow drops itself when broken with a shovel
		_world.SetBlock(_pos, BLOCK_AIR);
		DropItemAt(_world, _pos, Items::Id::SNOWBALL, /*count=*/1, 0);
		return;
	};
	blockBehaviors[BLOCK_SNOW_LAYER].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                            BlockType /*_blockId*/) -> void {
		const BlockType below = _world.GetBlockId(_pos.WithOffset(Cardinal::Value::Down));
		const bool canStay = (below != BLOCK_AIR) && Blocks::blockProperties[below].isOpaqueCube &&
		                     Blocks::blockProperties[below].material.isSolid;
		if (!canStay)
			_world.SetBlock(_pos, BLOCK_AIR);
	};
	blockBehaviors[BLOCK_SNOW_LAYER].onTick = [](WorldManager& _world, Int3 _pos, uint8_t /*_meta*/,
	                                             Java::Random& /*_random*/) -> void {
		// Melt!
		if (_world.GetBlockLight(_pos) > 11) {
			_world.SetBlock(_pos, BLOCK_AIR);
		}
	};

	// Ice
	blockBehaviors[BLOCK_ICE].onTick = [](WorldManager& _world, Int3 _pos, uint8_t /*_meta*/,
	                                      Java::Random& /*_random*/) -> void {
		// Melt!
		if (_world.GetBlockLight(_pos) > 11 - Blocks::blockProperties[BLOCK_ICE].lightOpacity) {
			_world.SetBlock(_pos, BLOCK_WATER_FLOWING);
		}
	};
	blockBehaviors[BLOCK_ICE].onBlockDestroyedByPlayer = [](WorldManager& _world, Int3 _pos, Entity& _destroyer) {
		// Ice will turn into water if there is something solid under it
		// Or if there is a liquid under it lol
		auto belowMaterial = _world.GetMaterial(_pos.WithOffset(Cardinal::Value::Down));
		if (belowMaterial.isSolid || belowMaterial.isLiquid) {
			_world.SetBlock(_pos, BLOCK_WATER_FLOWING);
			return;
		}
		GenericBreak(_world, _pos, _destroyer);
	};

	// Pumpkin
	auto onPumpkinPlace = [](WorldManager& _world, Int3 _pos, Entity& _placer, Cardinal::Value _face,
	                         BlockType _blockId, uint8_t /*_meta*/) -> bool {
		int meta[] = { 2, 3, 0, 1 };
		auto belowBlockMaterial = _world.GetMaterial({ _pos.x, _pos.y - 1, _pos.z });
		if (!belowBlockMaterial.isOpaque)
			return false;
		return GenericPlace(_world, _pos, _placer, _face, _blockId, meta[GetDirectionFromYaw(_placer.rotationYaw, 4)]);
	};

	blockBehaviors[BLOCK_PUMPKIN].onBlockPlaced = onPumpkinPlace;
	blockBehaviors[BLOCK_PUMPKIN_LIT].onBlockPlaced = onPumpkinPlace;

	// Furnaces
	blockBehaviors[BLOCK_FURNACE].onBlockPlaced = onFurnaceDispenserPlace;
	blockBehaviors[BLOCK_FURNACE_LIT].onBlockPlaced = onFurnaceDispenserPlace;

	blockBehaviors[BLOCK_FURNACE].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		if (_world.GetTileEntity(_pos))
			return;
		auto furnaceTileEntity = std::make_shared<TileEntityFurnace>(_pos);
		_world.CreateTileEntity(std::move(furnaceTileEntity));
	};

	auto dropFurnaceInventory = [](WorldManager& _world, Int3 _pos) -> void {
		auto* te = _world.GetTileEntityAs<TileEntityFurnace>(_pos);
		if (!te)
			return;

		_world.DropInventory(te->inventory, _pos);
	};

	blockBehaviors[BLOCK_FURNACE].onBlockRemoval = dropFurnaceInventory;
	blockBehaviors[BLOCK_FURNACE_LIT].onBlockRemoval = dropFurnaceInventory;

	// Dispenser
	blockBehaviors[BLOCK_DISPENSER].onBlockPlaced = onFurnaceDispenserPlace;

	// Jukebox
	blockBehaviors[BLOCK_JUKEBOX].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		auto jukeboxTileEntity = std::make_shared<TileEntityJukebox>(_pos);
		_world.CreateTileEntity(std::move(jukeboxTileEntity));
	};

	blockBehaviors[BLOCK_JUKEBOX].onBlockRemoval = [](WorldManager& _world, Int3 _pos) -> void {
		auto* te = _world.GetTileEntityAs<TileEntityJukebox>(_pos);
		if (!te || te->recordItemId == Items::Id::INVALID)
			return;

		DropItemAt(_world, _pos, static_cast<Items::Id>(int16_t(te->recordItemId)), /*count=*/1, 0);
	};

	// Stairs
	auto onStairPlace = [](WorldManager& _world, Int3 _pos, Entity& _placer, Cardinal::Value _face, BlockType _blockId,
	                       uint8_t /*_meta*/) -> bool {
		int meta[] = { 2, 1, 3, 0 };
		return GenericPlace(_world, _pos, _placer, _face, _blockId, meta[GetDirectionFromYaw(_placer.rotationYaw, 4)]);
	};

	blockBehaviors[BLOCK_STAIRS_COBBLESTONE].onBlockPlaced = onStairPlace;
	blockBehaviors[BLOCK_STAIRS_WOOD].onBlockPlaced = onStairPlace;

	// Tnt
	// Powered when placed
	blockBehaviors[BLOCK_TNT].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		if (RedstoneManager::IsPositionPowered(_world, _pos)) {
			_world.SetBlock(_pos, BLOCK_AIR);
			IgniteTnt(_world, _pos);
		}
	};
	// Powered by a neighbor
	blockBehaviors[BLOCK_TNT].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos, BlockType _blockId) -> void {
		if (RedstoneManager::CanProvidePower(_blockId) && RedstoneManager::IsPositionPowered(_world, _pos)) {
			_world.SetBlock(_pos, BLOCK_AIR);
			IgniteTnt(_world, _pos);
		}
	};
	// Chain reactions
	blockBehaviors[BLOCK_TNT].onBlockDestroyedByExplosion = [](WorldManager& _world, Int3 _pos) -> void {
		_world.SetBlock(_pos, BLOCK_AIR);
		IgniteTnt(_world, _pos, _world.rand.NextInt(80 / 4) + 80 / 8);
	};

	// Light when punched and the player is holding flint and steel
	blockBehaviors[BLOCK_TNT].onBlockDestroyedByPlayer = [](WorldManager& _world, Int3 _pos,
	                                                        Entity& _destroyer) -> void {
		auto& player = static_cast<PlayerEntity&>(_destroyer);
		auto heldItem = player.GetHeldItem();

		if (heldItem && heldItem->id == Items::Id::FLINT_AND_STEEL) {
			_world.SetBlock(_pos, BLOCK_AIR);
			IgniteTnt(_world, _pos);
			return;
		}

		GenericBreak(_world, _pos, _destroyer);
	};

	// Ladder
	blockBehaviors[BLOCK_LADDER].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                        BlockType /*_blockId*/) -> void {
		blockBehaviors[BLOCK_LADDER].onTick(_world, _pos, _world.GetMetadata(_pos), _world.rand);
	};
	blockBehaviors[BLOCK_LADDER].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                         Java::Random& /*_random*/) -> void {
		// Check to make sure we can still exist here
		if (!IsSupported(_world, _pos, GetDirectionFromMeta(BLOCK_LADDER, _meta)))
			BreakAndDropBlock(_world, _pos);
	};
	blockBehaviors[BLOCK_LADDER].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                Cardinal::Value _face, BlockType _blockId,
	                                                uint8_t /*_meta*/) -> bool {
		Int3 targetPos = _pos;
		const Int3 sourceBlock = _pos.WithOffset(Cardinal::Opposite(_face));
		if (_world.GetBlockId(sourceBlock) == BLOCK_SNOW_LAYER)
			targetPos = sourceBlock;

		static constexpr std::array<Cardinal::Value, 4> CHECK_ORDER = {
			Cardinal::Value::North, Cardinal::Value::South, Cardinal::Value::West, Cardinal::Value::East
		};

		if (Cardinal::IsHorizontal(_face) && IsSupported(_world, _pos, _face)) {
			return GenericPlace(_world, _pos, _placer, _face, _blockId, GetMetaFromDirection(BLOCK_LADDER, _face));
		}

		for (Cardinal::Value dir : CHECK_ORDER) {
			if (IsSupported(_world, _pos, dir)) {
				return GenericPlace(_world, _pos, _placer, _face, _blockId, GetMetaFromDirection(BLOCK_LADDER, dir));
			}
		}
		return false;
	};

	blockBehaviors[BlockType::BLOCK_SIGN_WALL] = {
		.getSelectionBox = WallSignAabb,
		.getRayBounds = WallSignAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BLOCK_SIGN_WALL].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                         Java::Random& /*_random*/) -> void {
		Int3 support = _pos.WithOffset(Cardinal::Opposite(GetDirectionFromMeta(BLOCK_SIGN_WALL, _meta)));
		if (!_world.GetMaterial(support).isSolid) {
			BreakAndDropBlock(_world, _pos);
		}
	};
	blockBehaviors[BLOCK_SIGN_WALL].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                   Cardinal::Value _face, BlockType _blockId, uint8_t _meta) -> bool {
		Int3 support = _pos.WithOffset(Cardinal::Opposite(GetDirectionFromMeta(BLOCK_SIGN_WALL, _meta)));
		if (!_world.GetMaterial(support).isSolid)
			return false;
		return GenericPlace(_world, _pos, _placer, _face, _blockId, _meta);
	};

	blockBehaviors[BLOCK_SIGN_WALL].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                        BlockType /*_blockId*/) -> void {
		blockBehaviors[BLOCK_SIGN_WALL].onTick(_world, _pos, _world.GetMetadata(_pos), _world.rand);
	};

	blockBehaviors[BlockType::BLOCK_SIGN_STANDING] = {
		.getSelectionBox = StandingSignAabb,
		.getRayBounds = StandingSignAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BLOCK_SIGN_STANDING].onTick = [](WorldManager& _world, Int3 _pos, uint8_t /*_meta*/,
	                                         Java::Random& /*_random*/) -> void {
		if (!IsSupported(_world, _pos, Cardinal::Value::Up))
			BreakAndDropBlock(_world, _pos);
	};
	blockBehaviors[BLOCK_SIGN_STANDING].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                   Cardinal::Value _face, BlockType _blockId,
	                                                   uint8_t _meta) -> bool {
		Int3 clicked = _pos.WithOffset(Cardinal::Opposite(_face));
		if (!_world.GetMaterial(clicked).isSolid || _face == Cardinal::Value::Down)
			return false;
		return GenericPlace(_world, _pos, _placer, _face, _blockId, _meta);
	};
	blockBehaviors[BLOCK_SIGN_STANDING].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                        BlockType /*_blockId*/) -> void {
		blockBehaviors[BLOCK_SIGN_STANDING].onTick(_world, _pos, 0, _world.rand);
	};
}
}; // namespace Blocks