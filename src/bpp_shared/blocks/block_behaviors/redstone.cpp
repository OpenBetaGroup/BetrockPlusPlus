/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "blocks.h"
#include "blocks/block_behaviors.h"
#include "blocks/block_properties.h"
#include "dimensions.h"
#include "entities/entity_arrow.h"
#include "entities/entity_egg.h"
#include "entities/entity_falling_block.h"
#include "entities/entity_item.h"
#include "entities/entity_player.h"
#include "entities/entity_skeleton.h"
#include "entities/entity_snowball.h"
#include "entities/entity_spider.h"
#include "entities/entity_zombie.h"
#include "enums/items.h"
#include "generator/overworld/tree_gen.h"
#include "helpers/direction_fixer.h"
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

static void PlayNoteblock(WorldManager& _world, Int3 _pos) {
	auto noteblockTe = _world.GetTileEntityAs<TileEntityNoteblock>(_pos);
	if (!noteblockTe)
		return;

	BlockType belowBlock = _world.GetBlockId(_pos.WithOffset(Cardinal::Value::Down));
	switch (Blocks::blockProperties[belowBlock].material.type) {
	case MaterialType::Wood:
		_world.PlayNoteAt(_pos, PacketData::NoteInstrument::BASS, noteblockTe->note);
		break;
	case MaterialType::Sand:
		_world.PlayNoteAt(_pos, PacketData::NoteInstrument::SNARE_DRUM, noteblockTe->note);
		break;
	case MaterialType::Rock:
		_world.PlayNoteAt(_pos, PacketData::NoteInstrument::BASS_DRUM, noteblockTe->note);
		break;
	case MaterialType::Glass:
		_world.PlayNoteAt(_pos, PacketData::NoteInstrument::HI_HAT, noteblockTe->note);
		break;
	default:
		_world.PlayNoteAt(_pos, PacketData::NoteInstrument::HARP, noteblockTe->note);
		break;
	}
}

static void DispenseItemFromDispenser(WorldManager& _world, Int3 _pos, uint8_t _meta) {
	int xOffset = 0;
	int zOffset = 0;

	switch (_meta) {
	case 3:
		zOffset = 1;
		break;
	case 2:
		zOffset = -1;
		break;
	case 5:
		xOffset = 1;
		break;
	default:
		xOffset = -1;
		break;
	}

	auto dispenserTe = _world.GetTileEntityAs<TileEntityDispenser>(_pos);
	if (!dispenserTe)
		return;

	auto randStack = dispenserTe->GetRandomStackInInventory();
	if (!randStack.has_value()) {
		if (_world.onWorldEvent)
			_world.onWorldEvent(PacketData::WorldEvent::DISPENSER_CLICK, _pos, 0, nullptr);
		return;
	}

	Vec3 outputPos;
	outputPos.x = _pos.x + xOffset * 0.6 + 0.5;
	outputPos.y = _pos.y + 0.5;
	outputPos.z = _pos.z + zOffset * 0.6 + 0.5;

	auto randStackPtr = randStack.value();
	if (!randStackPtr)
		return;
	ItemStack newStack = *randStackPtr;
	randStackPtr->DecrementCount(1);
	newStack.count = 1;

	switch (newStack.id) {
	case Items::ARROW: {
		std::shared_ptr<ArrowEntity> entity = std::make_shared<ArrowEntity>(outputPos);
		entity->SetArrowHeading({ double(xOffset), 0.1, double(zOffset) }, 1.1f, 6.0f);
		entity->arrowBelongsToPlayer = true;
		_world.entityManager.AddEntity(entity);
		if (_world.onWorldEvent)
			_world.onWorldEvent(PacketData::WorldEvent::DISPENSER_ARROW_FIRE, _pos, (xOffset + 1) + (zOffset + 1) * 3,
			                    nullptr);
		break;
	}
	case Items::EGG: {
		std::shared_ptr<EggEntity> entity = std::make_shared<EggEntity>(outputPos);
		entity->SetHeading({ double(xOffset), 0.1, double(zOffset) }, 1.1f, 6.0f);
		_world.entityManager.AddEntity(entity);
		if (_world.onWorldEvent)
			_world.onWorldEvent(PacketData::WorldEvent::DISPENSER_ARROW_FIRE, _pos, (xOffset + 1) + (zOffset + 1) * 3,
			                    nullptr);
		break;
	}
	case Items::SNOWBALL: {
		std::shared_ptr<SnowballEntity> entity = std::make_shared<SnowballEntity>(outputPos);
		entity->SetHeading({ double(xOffset), 0.1, double(zOffset) }, 1.1f, 6.0f);
		_world.entityManager.AddEntity(entity);
		if (_world.onWorldEvent)
			_world.onWorldEvent(PacketData::WorldEvent::DISPENSER_ARROW_FIRE, _pos, (xOffset + 1) + (zOffset + 1) * 3,
			                    nullptr);
		break;
	}
	default: {
		outputPos.y -= 0.3;
		std::shared_ptr<ItemEntity> item = std::make_shared<ItemEntity>(outputPos);
		item->itemStack = newStack;

		double speed = _world.rand.NextDouble() * 0.1 + 0.2;
		item->velocity.x = xOffset * speed;
		item->velocity.y = 0.2;
		item->velocity.z = zOffset * speed;
		item->velocity.x += _world.rand.NextGaussian() * 0.007 * 6.0;
		item->velocity.y += _world.rand.NextGaussian() * 0.007 * 6.0;
		item->velocity.z += _world.rand.NextGaussian() * 0.007 * 6.0;

		_world.entityManager.AddEntity(item);

		if (_world.onWorldEvent)
			_world.onWorldEvent(PacketData::WorldEvent::DISPENSER_CLICK_EMPTY, _pos, (xOffset + 1) + (zOffset + 1) * 3,
			                    nullptr);
		break;
	}
	}

	if (_world.onWorldEvent)
		_world.onWorldEvent(PacketData::WorldEvent::DISPENSER_PUFF, _pos, (xOffset + 1) + (zOffset + 1) * 3, nullptr);
}

static bool CanRedstoneComponentStay(WorldManager& _world, Int3 _pos) {
	return _world.IsBlockNormalCube(_pos.Offset(Cardinal::Value::Down));
}

static bool TorchHasAnySupport(WorldManager& _world, Int3 _pos) {
	for (auto face : { Cardinal::Value::East, Cardinal::Value::West, Cardinal::Value::South, Cardinal::Value::North,
	                   Cardinal::Value::Up }) {
		if (CanTorchAttachTo(_world, _pos, face))
			return true;
	}
	return false;
}

static bool PlaceTorchLike(WorldManager& _world, Int3 _pos, Entity& _placer, Cardinal::Value _face,
                           BlockType _blockId) {
	Int3 target = _pos;
	Cardinal::Value attachFace = _face;

	// Snow forces meta 0
	if (_world.GetBlockId(_pos.WithOffset(Cardinal::Opposite(_face))) == BLOCK_SNOW_LAYER) {
		target = _pos.WithOffset(Cardinal::Opposite(_face));
		attachFace = Cardinal::Value::Down;
	}

	// Is there any support nearby?
	if (!TorchHasAnySupport(_world, target))
		return false;

	uint8_t meta = CanTorchAttachTo(_world, target, attachFace) ? GetMetaFromDirection(_blockId, attachFace) : 0;

	return GenericPlace(_world, _pos, _placer, _face, _blockId, meta);
}

static void NotifyAttachedSupportBlock(WorldManager& _world, Int3 _pos, BlockType _blockId, uint8_t _meta) {
	Cardinal::Value dir = GetDirectionFromMeta(_blockId, _meta);
	Int3 support = _pos.WithOffset(Cardinal::Opposite(dir));

	static constexpr Cardinal::Value ALL_DIRS[6] = {
		Cardinal::Value::North, Cardinal::Value::South, Cardinal::Value::East,
		Cardinal::Value::West,  Cardinal::Value::Up,    Cardinal::Value::Down,
	};
	for (auto d : ALL_DIRS) {
		if (d == dir)
			continue;
		Int3 neighborPos = support.WithOffset(d);
		auto block = _world.GetBlockId(neighborPos);
		auto updateFunction = Blocks::blockBehaviors[block].onNeighborBlockChange;
		if (updateFunction)
			updateFunction(_world, neighborPos, _blockId);
	}
}

static void SetPressurePlateState(WorldManager& _world, Int3 _pos, BlockType _type) {
	bool powered = _world.GetMetadata(_pos) == 1;
	double horizontalInset = 0.125;
	double veritcalInset = 0.25;

	AABB detectionBox = { _pos.x + horizontalInset,       static_cast<double>(_pos.y), _pos.z + horizontalInset,
		                  (_pos.x + 1) - horizontalInset, _pos.y + veritcalInset,      (_pos.z + 1) - horizontalInset };

	// Collect entities
	std::vector<Entity*> entities;

	// Wooden pressure plates grab everything
	if (_type == BLOCK_PRESSURE_PLATE_WOOD)
		entities = _world.entityManager.GetEntitiesWithinAabb(detectionBox);

	// Stone pressure plates only grab living entities
	if (_type == BLOCK_PRESSURE_PLATE_STONE)
		entities = _world.entityManager.GetLivingEntitiesWithinAabb(detectionBox);

	bool triggered = entities.size() > 0;

	if (triggered && !powered) {
		_world.SetMeta(_pos, 1);
		_world.NotifyNeighborsOfUpdate(_pos, _type);
		_world.NotifyNeighborsOfUpdate(_pos.WithOffset(Cardinal::Value::Down), _type);
	}

	if (!triggered && powered) {
		_world.SetMeta(_pos, 0);
		_world.NotifyNeighborsOfUpdate(_pos, _type);
		_world.NotifyNeighborsOfUpdate(_pos.WithOffset(Cardinal::Value::Down), _type);
	}

	if (triggered) {
		_world.tickScheduler.ScheduleUpdateTick(_pos, _type, 20);
	}
}

void RegisterRedstoneBehaviors() {
	// Redstone dust
	blockBehaviors[BlockType::BLOCK_REDSTONE] = {
		.getSelectionBox = RedstoneDustAabb,
		.getRayBounds = RedstoneDustAabb,
		.getCollider = EmptyCollider,
		.onNeighborBlockChange = [](WorldManager& _world, Int3 _pos, BlockType _blockId) -> void {
		    if (_blockId != BLOCK_REDSTONE)
			    RedstoneManager::RefreshWireAt(_world, _pos);
		    if (!CanRedstoneComponentStay(_world, _pos)) {
			    BreakAndDropBlock(_world, _pos);
		    }
		},
	};

	blockBehaviors[BlockType::BLOCK_REDSTONE_REPEATER_OFF] = {
		.getSelectionBox = RepeaterAabb,
		.getRayBounds = RepeaterAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BlockType::BLOCK_REDSTONE_REPEATER_ON] = {
		.getSelectionBox = RepeaterAabb,
		.getRayBounds = RepeaterAabb,
		.getCollider = EmptyCollider,
	};

	blockBehaviors[BlockType::BLOCK_BUTTON_STONE] = {
		.getSelectionBox = ButtonAabb,
		.getRayBounds = ButtonAabb,
		.getCollider = EmptyCollider,
		.onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta, Java::Random& /*_random*/) -> void {
		    if (_meta & 0b1000) {
			    _world.SetMeta(_pos, _meta & 0b111);
			    NotifyAttachedSupportBlock(_world, _pos, BLOCK_BUTTON_STONE, _meta);
		    }
		},
		.onNeighborBlockChange = [](WorldManager& _world, Int3 _pos, BlockType /*_blockId*/) -> void {
		    if (!IsSupported(_world, _pos, GetDirectionFromMeta(BLOCK_BUTTON_STONE, _world.GetMetadata(_pos))))
			    BreakAndDropBlock(_world, _pos);
		},
		.onBlockClicked = [](WorldManager& _world, Int3 _pos, PlayerSession* /*_triggeringSession*/) -> void {
		    auto newMeta = _world.GetMetadata(_pos) | 0b1000;
		    _world.SetMeta(_pos, newMeta);
		    NotifyAttachedSupportBlock(_world, _pos, BLOCK_BUTTON_STONE, newMeta);
		    _world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_BUTTON_STONE, 20);
		    //if (_world.onWorldEvent)
		    //    _world.onWorldEvent(PacketData::WorldEvent::DISPENSER_CLICK_EMPTY, _pos, 0, _triggeringSession);
		},
		.onBlockActivated = [](WorldManager& _world, Int3 _pos, PlayerSession* _triggeringSession) -> bool {
		    blockBehaviors[BLOCK_BUTTON_STONE].onBlockClicked(_world, _pos, _triggeringSession);
		    return false;
		},
		.onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& /*_placer*/, Cardinal::Value _face,
		                    BlockType _blockId, uint8_t /*_meta*/) -> bool {
		    // Buttons can only be placed against the sides of blocks
		    if (_face == Cardinal::Value::Up || _face == Cardinal::Value::Down)
			    return false;
		    if (!IsReplaceable(_world, _pos))
			    return false;
		    _world.SetBlock(_pos, _blockId, GetMetaFromDirection(BLOCK_BUTTON_STONE, _face));
		    return true;
		},
	};

	blockBehaviors[BlockType::BLOCK_LEVER] = {
		.getRayBounds = LeverAabb,
		.getCollider = EmptyCollider,
		// getSelectionBox stays defaultAABB
	};

	blockBehaviors[BlockType::BLOCK_PRESSURE_PLATE_STONE] = {
		.getSelectionBox = PressurePlateAabb,
		.getRayBounds = PressurePlateAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BlockType::BLOCK_PRESSURE_PLATE_WOOD] = {
		.getSelectionBox = PressurePlateAabb,
		.getRayBounds = PressurePlateAabb,
		.getCollider = EmptyCollider,
	};

	blockBehaviors[BlockType::BLOCK_TORCH] = {
		.getSelectionBox = TorchAabb,
		.getRayBounds = TorchAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BlockType::BLOCK_REDSTONE_TORCH_OFF] = {
		.getSelectionBox = TorchAabb,
		.getRayBounds = TorchAabb,
		.getCollider = EmptyCollider,
	};
	blockBehaviors[BlockType::BLOCK_REDSTONE_TORCH_ON] = {
		.getSelectionBox = TorchAabb,
		.getRayBounds = TorchAabb,
		.getCollider = EmptyCollider,
	};

	blockBehaviors[BLOCK_REDSTONE_TORCH_ON].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                           Cardinal::Value _face, BlockType _blockId,
	                                                           uint8_t /*_meta*/) -> bool {
		return PlaceTorchLike(_world, _pos, _placer, _face, _blockId);
	};

	blockBehaviors[BLOCK_REDSTONE_TORCH_ON].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		const auto currentFace = GetDirectionFromMeta(BLOCK_REDSTONE_TORCH_ON, _world.GetMetadata(_pos));
		if (CanTorchAttachTo(_world, _pos, currentFace))
			return; // Already valid

		// Matches vanilla order
		static constexpr std::array<Cardinal::Value, 5> CHECK_ORDER = { Cardinal::Value::East, Cardinal::Value::West,
			                                                             Cardinal::Value::South,
			                                                             Cardinal::Value::North, Cardinal::Value::Up };

		// Attach to the first support block we find
		for (Cardinal::Value face : CHECK_ORDER) {
			if (CanTorchAttachTo(_world, _pos, face)) {
				_world.SetMeta(_pos, GetMetaFromDirection(BLOCK_REDSTONE_TORCH_ON, face));
				return;
			}
		}

		BreakAndDropBlock(_world, _pos);
	};

	blockBehaviors[BLOCK_REDSTONE_TORCH_ON].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                                   BlockType /*_blockId*/) -> void {
		auto dir = GetDirectionFromMeta(BLOCK_REDSTONE_TORCH_ON, _world.GetMetadata(_pos));
		if (!CanTorchAttachTo(_world, _pos, dir)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}

		// Torches take two ticks to update
		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_REDSTONE_TORCH_ON, 2);
	};

	blockBehaviors[BLOCK_REDSTONE_TORCH_ON].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                                    Java::Random& /*_random*/) -> void {
		RedstoneManager::PruneTorchUpdates(_world);

		const auto dir = GetDirectionFromMeta(BLOCK_REDSTONE_TORCH_ON, _meta);
		if (!CanTorchAttachTo(_world, _pos, dir)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}

		Int3 supportPos = _pos.WithOffset(Cardinal::Opposite(dir));
		if (RedstoneManager::GetBlockPowerProfile(_world, supportPos).powered) {
			_world.SetBlock(_pos, BLOCK_REDSTONE_TORCH_OFF, _meta);
			RedstoneManager::CheckTorchBurnout(_world, _pos, /*_logUpdate=*/true);
		}
	};

	blockBehaviors[BLOCK_REDSTONE_TORCH_OFF].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		const auto dir = GetDirectionFromMeta(BLOCK_REDSTONE_TORCH_OFF, _world.GetMetadata(_pos));
		if (CanTorchAttachTo(_world, _pos, dir))
			return; // Already valid

		static constexpr std::array<Cardinal::Value, 5> CHECK_ORDER = { Cardinal::Value::East, Cardinal::Value::West,
			                                                             Cardinal::Value::South,
			                                                             Cardinal::Value::North, Cardinal::Value::Up };

		for (Cardinal::Value face : CHECK_ORDER) {
			if (CanTorchAttachTo(_world, _pos, face)) {
				_world.SetMeta(_pos, GetMetaFromDirection(BLOCK_REDSTONE_TORCH_OFF, face));
				return;
			}
		}

		BreakAndDropBlock(_world, _pos);
	};

	blockBehaviors[BLOCK_REDSTONE_TORCH_OFF].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                                    BlockType /*_blockId*/) -> void {
		const auto dir = GetDirectionFromMeta(BLOCK_REDSTONE_TORCH_OFF, _world.GetMetadata(_pos));
		if (!CanTorchAttachTo(_world, _pos, dir)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}

		_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_REDSTONE_TORCH_OFF, 2);
	};

	blockBehaviors[BLOCK_REDSTONE_TORCH_OFF].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                                     Java::Random& /*_random*/) -> void {
		RedstoneManager::PruneTorchUpdates(_world);

		const auto dir = GetDirectionFromMeta(BLOCK_REDSTONE_TORCH_OFF, _meta);
		if (!CanTorchAttachTo(_world, _pos, dir)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}

		Int3 supportPos = _pos.WithOffset(Cardinal::Opposite(dir));
		if (!RedstoneManager::GetBlockPowerProfile(_world, supportPos).powered &&
		    !RedstoneManager::CheckTorchBurnout(_world, _pos, /*_logUpdate=*/false)) {
			_world.SetBlock(_pos, BLOCK_REDSTONE_TORCH_ON, _meta);
		}
	};

	blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                               Cardinal::Value _face, BlockType _blockId,
	                                                               uint8_t /*_meta*/) -> bool {
		const auto dir = Cardinal::FromAngle(_placer.rotationYaw);
		auto meta = GetMetaFromDirection(BLOCK_REDSTONE_REPEATER_OFF, dir);
		if (!CanRedstoneComponentStay(_world, _pos) || !GenericPlace(_world, _pos, _placer, _face, _blockId, meta))
			return false;
		// Turn on if we are being powered!
		if (RedstoneManager::IsRepeaterInputPowered(_world, _pos, meta)) {
			_world.tickScheduler.ScheduleUpdateTick(_pos, _blockId, 1);
		}
		return true;
	};

	blockBehaviors[BLOCK_REDSTONE].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                  Cardinal::Value _face, BlockType _blockId,
	                                                  uint8_t _meta) -> bool {
		BlockType existing = _world.GetBlockId(_pos);
		Int3 sourceBlock = _pos.WithOffset(Cardinal::Opposite(_face));

		// Check if we can replace snow
		Int3 targetPos = _pos;
		if (_world.GetBlockId(sourceBlock) == BLOCK_SNOW_LAYER) {
			targetPos = sourceBlock;
		} else {
			// Check if this block is replaceable
			bool replaceable = existing == BLOCK_AIR || existing == BLOCK_WATER_FLOWING ||
			                   existing == BLOCK_WATER_STILL || existing == BLOCK_LAVA_FLOWING ||
			                   existing == BLOCK_LAVA_STILL || existing == BLOCK_FIRE || existing == BLOCK_SNOW_LAYER;
			if (!replaceable)
				return false;
		}
		if (!CanRedstoneComponentStay(_world, targetPos) || !GenericPlace(_world, _pos, _placer, _face, _blockId, _meta))
			return false;
		return true;
	};

	blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                                       BlockType /*_blockId*/) -> void {
		if (!CanRedstoneComponentStay(_world, _pos)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}
		auto meta = _world.GetMetadata(_pos);
		auto thisBlock = _world.GetBlockId(_pos);
		bool inputPowered = RedstoneManager::IsRepeaterInputPowered(_world, _pos, meta);
		bool isPoweredRepeater = _world.GetBlockId(_pos) ==
		                         BLOCK_REDSTONE_REPEATER_ON; // Powered repeater calls this function too
		int delaySetting = (meta & 12) >> 2;
		int delayTicks = (delaySetting + 1) * 2;

		if (isPoweredRepeater && !inputPowered) {
			_world.tickScheduler.ScheduleUpdateTick(_pos, thisBlock, delayTicks);
		} else if (!isPoweredRepeater && inputPowered) {
			_world.tickScheduler.ScheduleUpdateTick(_pos, thisBlock, delayTicks);
		}
	};

	blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                                        Java::Random& /*_random*/) -> void {
		bool inputPowered = RedstoneManager::IsRepeaterInputPowered(_world, _pos, _meta);
		bool isPoweredRepeater = _world.GetBlockId(_pos) ==
		                         BLOCK_REDSTONE_REPEATER_ON; // Powered repeater calls this function too

		if (isPoweredRepeater && !inputPowered) {
			// Turn off if we are no longer being powered
			_world.SetBlock(_pos, BLOCK_REDSTONE_REPEATER_OFF, _meta);
		} else if (!isPoweredRepeater) {
			// Turn on unconditionally
			_world.SetBlock(_pos, BLOCK_REDSTONE_REPEATER_ON, _meta);
			if (!inputPowered) {
				// If we are no longer being powered request another update
				int delaySetting = (_meta & 12) >> 2;
				int delayTicks = (delaySetting + 1) * 2;
				_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_REDSTONE_REPEATER_ON, delayTicks);
			}
		}
	};

	blockBehaviors[BLOCK_REDSTONE_REPEATER_ON].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                                      BlockType _blockId) -> void {
		blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].onNeighborBlockChange(_world, _pos, _blockId);
	};

	blockBehaviors[BLOCK_REDSTONE_REPEATER_ON].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                                       Java::Random& _random) -> void {
		blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].onTick(_world, _pos, _meta, _random);
	};

	blockBehaviors[BLOCK_REDSTONE_REPEATER_ON].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                              Cardinal::Value _face, BlockType _blockId,
	                                                              uint8_t _meta) -> bool {
		return blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].onBlockPlaced(_world, _pos, _placer, _face, _blockId, _meta);
	};

	blockBehaviors[BLOCK_LEVER].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                               Cardinal::Value _face, BlockType _blockId,
	                                               uint8_t /*_meta*/) -> bool {
		if (_world.GetBlockId(_pos.WithOffset(Cardinal::Opposite(_face))) == BLOCK_SNOW_LAYER)
			return false;

		if (_face == Cardinal::Value::Down || _face == Cardinal::Value::None)
			return false;
		if (!_world.IsBlockNormalCube(_pos.WithOffset(Cardinal::Opposite(_face))))
			return false;

		uint8_t meta = GetMetaFromDirection(BLOCK_LEVER, _face);
		if (_face == Cardinal::Value::Up)
			meta = 5 + _world.rand.NextInt(2);

		return GenericPlace(_world, _pos, _placer, _face, _blockId, meta);
	};

	blockBehaviors[BLOCK_LEVER].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		const auto currentFace = GetDirectionFromMeta(BLOCK_LEVER, _world.GetMetadata(_pos));
		if (CanTorchAttachTo(_world, _pos, currentFace))
			return; // Already valid

		// Matches vanilla order
		static constexpr std::array<Cardinal::Value, 5> CHECK_ORDER = { Cardinal::Value::East, Cardinal::Value::West,
			                                                             Cardinal::Value::South,
			                                                             Cardinal::Value::North, Cardinal::Value::Up };

		// Attach to the first support block we find
		for (Cardinal::Value face : CHECK_ORDER) {
			if (CanTorchAttachTo(_world, _pos, face)) {
				_world.SetMeta(_pos, GetMetaFromDirection(BLOCK_LEVER, face));
				return;
			}
		}

		BreakAndDropBlock(_world, _pos);
	};

	blockBehaviors[BLOCK_LEVER].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                       BlockType /*_blockId*/) -> void {
		auto dir = GetDirectionFromMeta(BLOCK_LEVER, _world.GetMetadata(_pos));
		if (!CanTorchAttachTo(_world, _pos, dir)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}
	};

	blockBehaviors[BLOCK_LEVER].onBlockClicked = [](WorldManager& _world, Int3 _pos,
	                                                PlayerSession* /*_triggeringSession*/) -> void {
		auto newMeta = _world.GetMetadata(_pos) ^ 0b1000;
		_world.SetMeta(_pos, newMeta);
		NotifyAttachedSupportBlock(_world, _pos, BLOCK_LEVER, newMeta);
		//if (_world.onWorldEvent)
		//	_world.onWorldEvent(PacketData::WorldEvent::DISPENSER_CLICK_EMPTY, _pos, 0, _triggeringSession);
	},
	blockBehaviors[BLOCK_LEVER].onBlockActivated = [](WorldManager& _world, Int3 _pos,
	                                                  PlayerSession* _triggeringSession) -> bool {
		blockBehaviors[BLOCK_LEVER].onBlockClicked(_world, _pos, _triggeringSession);
		return false;
	},

	blockBehaviors[BLOCK_TORCH].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                               Cardinal::Value _face, BlockType _blockId,
	                                               uint8_t /*_meta*/) -> bool {
		return PlaceTorchLike(_world, _pos, _placer, _face, _blockId);
	};

	blockBehaviors[BLOCK_TORCH].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		const auto currentFace = GetDirectionFromMeta(BLOCK_TORCH, _world.GetMetadata(_pos));
		if (CanTorchAttachTo(_world, _pos, currentFace))
			return; // Already valid

		// Matches vanilla order
		static constexpr std::array<Cardinal::Value, 5> CHECK_ORDER = { Cardinal::Value::East, Cardinal::Value::West,
			                                                             Cardinal::Value::South,
			                                                             Cardinal::Value::North, Cardinal::Value::Up };

		// Attach to the first support block we find
		for (Cardinal::Value face : CHECK_ORDER) {
			if (CanTorchAttachTo(_world, _pos, face)) {
				_world.SetMeta(_pos, GetMetaFromDirection(BLOCK_TORCH, face));
				return;
			}
		}

		BreakAndDropBlock(_world, _pos);
	};

	blockBehaviors[BLOCK_TORCH].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                       BlockType /*_blockId*/) -> void {
		const auto dir = GetDirectionFromMeta(BLOCK_TORCH, _world.GetMetadata(_pos));
		if (!CanTorchAttachTo(_world, _pos, dir))
			BreakAndDropBlock(_world, _pos);
	};

	// for when the block is interacted with!
	blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].onBlockActivated = [](WorldManager& _world, Int3 _pos,
	                                                                  PlayerSession* /*_triggeringSession*/) -> bool {
		// Increase and loop delay
		auto meta = _world.GetMetadata(_pos);
		int delay = (meta & 12) >> 2;
		delay = (delay + 1) << 2 & 12;
		_world.SetMeta(_pos, delay | (meta & 3));
		return false;
	};
	blockBehaviors[BLOCK_REDSTONE_REPEATER_ON].onBlockActivated = [](WorldManager& _world, Int3 _pos,
	                                                                 PlayerSession* _triggeringSession) -> bool {
		return blockBehaviors[BLOCK_REDSTONE_REPEATER_OFF].onBlockActivated(_world, _pos, _triggeringSession);
	};

	// Wooden Pressure Plate
	blockBehaviors[BLOCK_PRESSURE_PLATE_WOOD].onBlockRemoval = [](WorldManager& _world, Int3 _pos) -> void {
		auto meta = _world.GetMetadata(_pos);
		if (meta > 0) {
			_world.NotifyNeighborsOfUpdate(_pos, BLOCK_PRESSURE_PLATE_WOOD);
			_world.NotifyNeighborsOfUpdate(_pos.WithOffset(Cardinal::Value::Down), BLOCK_PRESSURE_PLATE_WOOD);
		}
	};

	blockBehaviors[BLOCK_PRESSURE_PLATE_WOOD].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                             Cardinal::Value _face, BlockType _blockId,
	                                                             uint8_t _meta) -> bool {
		if (!CanRedstoneComponentStay(_world, _pos) || !GenericPlace(_world, _pos, _placer, _face, _blockId, _meta))
			return false;
		return true;
	};

	blockBehaviors[BLOCK_PRESSURE_PLATE_WOOD].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                                     BlockType /*_blockId*/) -> void {
		if (!CanRedstoneComponentStay(_world, _pos)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}
	};

	blockBehaviors[BLOCK_PRESSURE_PLATE_WOOD].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                                      Java::Random& /*_random*/) -> void {
		if (_meta != 0)
			SetPressurePlateState(_world, _pos, BLOCK_PRESSURE_PLATE_WOOD);
	};

	blockBehaviors[BLOCK_PRESSURE_PLATE_WOOD].onEntityCollidedWithBlock = [](WorldManager& _world, Int3 _pos,
	                                                                         Entity& /*_entity*/) -> void {
		auto meta = _world.GetMetadata(_pos);
		if (meta != 1)
			SetPressurePlateState(_world, _pos, BLOCK_PRESSURE_PLATE_WOOD);
	};

	// Stone Pressure Plate
	blockBehaviors[BLOCK_PRESSURE_PLATE_STONE].onBlockRemoval = [](WorldManager& _world, Int3 _pos) -> void {
		auto meta = _world.GetMetadata(_pos);
		if (meta > 0) {
			_world.NotifyNeighborsOfUpdate(_pos, BLOCK_PRESSURE_PLATE_STONE);
			_world.NotifyNeighborsOfUpdate(_pos.WithOffset(Cardinal::Value::Down), BLOCK_PRESSURE_PLATE_STONE);
		}
	};

	blockBehaviors[BLOCK_PRESSURE_PLATE_STONE].onBlockPlaced = [](WorldManager& _world, Int3 _pos, Entity& _placer,
	                                                              Cardinal::Value _face, BlockType _blockId,
	                                                              uint8_t _meta) -> bool {
		if (!CanRedstoneComponentStay(_world, _pos) || !GenericPlace(_world, _pos, _placer, _face, _blockId, _meta))
			return false;
		return true;
	};

	blockBehaviors[BLOCK_PRESSURE_PLATE_STONE].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                                      BlockType /*_blockId*/) -> void {
		if (!CanRedstoneComponentStay(_world, _pos)) {
			BreakAndDropBlock(_world, _pos);
			return;
		}
	};

	blockBehaviors[BLOCK_PRESSURE_PLATE_STONE].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                                       Java::Random& /*_random*/) -> void {
		if (_meta != 0)
			SetPressurePlateState(_world, _pos, BLOCK_PRESSURE_PLATE_STONE);
	};

	blockBehaviors[BLOCK_PRESSURE_PLATE_STONE].onEntityCollidedWithBlock = [](WorldManager& _world, Int3 _pos,
	                                                                          Entity& /*_entity*/) -> void {
		auto meta = _world.GetMetadata(_pos);
		if (meta != 1)
			SetPressurePlateState(_world, _pos, BLOCK_PRESSURE_PLATE_STONE);
	};

	// Dispensers
	blockBehaviors[BLOCK_DISPENSER].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		auto te = std::make_shared<TileEntityDispenser>(_pos);
		_world.CreateTileEntity(std::move(te));
	};

	blockBehaviors[BLOCK_DISPENSER].onBlockRemoval = [](WorldManager& _world, Int3 _pos) -> void {
		auto* te = _world.GetTileEntityAs<TileEntityDispenser>(_pos);
		if (!te)
			return;

		_world.DropInventory(te->inventory, _pos);
	};

	blockBehaviors[BLOCK_DISPENSER].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                           BlockType _blockId) -> void {
		if (!RedstoneManager::CanProvidePower(_blockId))
			return;

		if (RedstoneManager::IsPositionPowered(_world, _pos) ||
		    RedstoneManager::IsPositionPowered(_world, _pos.WithOffset(Cardinal::Value::Up)))
			_world.tickScheduler.ScheduleUpdateTick(_pos, BLOCK_DISPENSER, 4);
	};

	blockBehaviors[BLOCK_DISPENSER].onTick = [](WorldManager& _world, Int3 _pos, uint8_t _meta,
	                                            Java::Random& /*_random*/) -> void {
		if (RedstoneManager::IsPositionPowered(_world, _pos) ||
		    RedstoneManager::IsPositionPowered(_world, _pos.WithOffset(Cardinal::Value::Up)))
			DispenseItemFromDispenser(_world, _pos, _meta);
	};

	// Noteblocks
	blockBehaviors[BLOCK_NOTEBLOCK].onBlockAdded = [](WorldManager& _world, Int3 _pos) -> void {
		auto te = std::make_shared<TileEntityNoteblock>(_pos);
		_world.CreateTileEntity(std::move(te));
	};

	blockBehaviors[BLOCK_NOTEBLOCK].onBlockRemoval = [](WorldManager& _world, Int3 _pos) -> void {
		auto* te = _world.GetTileEntityAs<TileEntityNoteblock>(_pos);
		if (!te)
			return;
	};

	blockBehaviors[BLOCK_NOTEBLOCK].onTick = [](WorldManager& _world, Int3 _pos, uint8_t /*_meta*/,
	                                            Java::Random& /*_random*/) -> void {
		if (RedstoneManager::IsPositionPowered(_world, _pos) ||
		    RedstoneManager::IsPositionPowered(_world, _pos.WithOffset(Cardinal::Value::Up)))
			PlayNoteblock(_world, _pos);
	};

	blockBehaviors[BLOCK_NOTEBLOCK].onNeighborBlockChange = [](WorldManager& _world, Int3 _pos,
	                                                           BlockType _blockId) -> void {
		if (!RedstoneManager::CanProvidePower(_blockId))
			return;

		if (RedstoneManager::IsPositionPowered(_world, _pos) ||
		    RedstoneManager::IsPositionPowered(_world, _pos.WithOffset(Cardinal::Value::Up)))
			blockBehaviors[BLOCK_NOTEBLOCK].onTick(_world, _pos, 0, _world.rand);
	};

	blockBehaviors[BLOCK_NOTEBLOCK].onBlockClicked = [](WorldManager& _world, Int3 _pos,
	                                                    PlayerSession* /*_triggeringSession*/) -> void {
		auto noteblockTe = _world.GetTileEntityAs<TileEntityNoteblock>(_pos);
		if (!noteblockTe)
			return;
		noteblockTe->note = (noteblockTe->note + 1) % 25;
		PlayNoteblock(_world, _pos);
	};

	blockBehaviors[BLOCK_NOTEBLOCK].onBlockActivated = [](WorldManager& _world, Int3 _pos,
	                                                      PlayerSession* _triggeringSession) -> bool {
		blockBehaviors[BLOCK_NOTEBLOCK].onBlockClicked(_world, _pos, _triggeringSession);
		return false;
	};
}

}; // namespace Blocks
