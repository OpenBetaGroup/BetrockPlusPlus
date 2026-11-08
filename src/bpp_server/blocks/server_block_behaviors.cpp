/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "server_block_behaviors.h"
#include "../../bpp_shared/helpers/direction_fixer.h"
#include "../commands/command.h"
#include "blocks.h"
#include "blocks/block_properties.h"
#include "entities/entity_skeleton.h"
#include "entities/entity_spider.h"
#include "entities/entity_zombie.h"
#include "gamerules.h"
#include "inventory/interactions/chest.h"
#include "inventory/interactions/crafting_table.h"
#include "inventory/interactions/dispenser.h"
#include "inventory/interactions/furnace.h"
#include "inventory/interactions/large_chest.h"
#include "items/tool_properties.h"
#include "pathfinding/pathfinder.h"
#include "tile_entities/tile_entity.h"

namespace ServerBlock {
BlockBehavior blockBehaviors[BLOCK_MAX] = {};
} // namespace ServerBlock

void ServerBlock::Initialize() {
	// Register unique behaviors here
	blockBehaviors[BLOCK_CRAFTING_TABLE].onBlockActivated = [](WorldManager& _world, Int3 _position,
	                                                           PlayerSession& _session, Runtime& _gameRuntime) -> bool {
		Packet::OpenContainer ow;
		ow.windowId = _session.GetNextWindowId();
		ow.slotCount = 9;
		ow.title = "Crafting";
		ow.windowType = PacketData::WindowType::CRAFTING_TABLE;
		ow.Serialize(_session.stream);

		_session.activeInteraction = std::make_unique<CraftingTableInventoryInteraction>(&_session.inventory, _world,
		                                                                                 _gameRuntime, _position);
		_session.activeInteraction->InitSnapshot();
		return false;
	};

	blockBehaviors[BLOCK_DISPENSER].onBlockActivated = [](WorldManager& _world, Int3 _position, PlayerSession& _session,
	                                                      Runtime& /*_gameRuntime*/) -> bool {
		auto trap = _world.GetTileEntityShared<TileEntityDispenser>(_position);
		if (!trap)
			return false;

		Packet::OpenContainer ow;
		ow.windowId = _session.GetNextWindowId();
		ow.slotCount = 9;
		ow.title = "Dispenser";
		ow.windowType = PacketData::WindowType::DISPENSER;
		ow.Serialize(_session.stream);

		_session.activeInteraction = std::make_unique<TrapInventoryInteraction>(&_session.inventory, trap);
		_session.activeInteraction->InitSnapshot();

		PacketUtilities::SendInventory(_session, _session.openWindowId, *_session.activeInteraction->inventory);
		return false;
	};

	auto furnaceActivated = [](WorldManager& _world, Int3 _position, PlayerSession& _session,
	                           Runtime& /*_gameRuntime*/) -> bool {
		auto furnace = _world.GetTileEntityShared<TileEntityFurnace>(_position);
		if (!furnace)
			return false;

		Packet::OpenContainer ow;
		ow.windowId = _session.GetNextWindowId();
		ow.slotCount = 3;
		ow.title = "Furnace";
		ow.windowType = PacketData::WindowType::FURNACE;
		ow.Serialize(_session.stream);

		_session.activeInteraction = std::make_unique<FurnaceInventoryInteraction>(&_session.inventory, furnace);
		_session.activeInteraction->InitSnapshot();

		PacketUtilities::SendInventory(_session, _session.openWindowId, *_session.activeInteraction->inventory);

		Packet::ContainerData cd;
		cd.windowId = _session.openWindowId;
		cd.containerData.type = PacketData::ContainerDataType::FUEL_DURATION;
		cd.containerData.value = furnace->GetMaxBurnTime();
		cd.Serialize(_session.stream);

		return false;
	};

	blockBehaviors[BLOCK_FURNACE].onBlockActivated = furnaceActivated;
	blockBehaviors[BLOCK_FURNACE_LIT].onBlockActivated = furnaceActivated;

	blockBehaviors[BLOCK_CHEST].onBlockActivated = [](WorldManager& _world, Int3 _position, PlayerSession& _session,
	                                                  Runtime& /*_gameRuntime*/) -> bool {
		if (!Blocks::CanOpenChest(_world, _position))
			return false;

		auto chest = _world.GetTileEntityShared<TileEntityChest>(_position);

		// Are we a double chest?
		auto l = _world.GetBlockId({ _position.x - 1, _position.y, _position.z });
		auto r = _world.GetBlockId({ _position.x + 1, _position.y, _position.z });
		auto f = _world.GetBlockId({ _position.x, _position.y, _position.z - 1 });
		auto b = _world.GetBlockId({ _position.x, _position.y, _position.z + 1 });
		bool doubleChest = (l == BLOCK_CHEST || r == BLOCK_CHEST || f == BLOCK_CHEST || b == BLOCK_CHEST);

		if (doubleChest) {
			std::shared_ptr<TileEntityChest> partnerChest = nullptr;
			if (l == BLOCK_CHEST)
				partnerChest = _world.GetTileEntityShared<TileEntityChest>(
				    { _position.x - 1, _position.y, _position.z });
			else if (r == BLOCK_CHEST)
				partnerChest = _world.GetTileEntityShared<TileEntityChest>(
				    { _position.x + 1, _position.y, _position.z });
			else if (f == BLOCK_CHEST)
				partnerChest = _world.GetTileEntityShared<TileEntityChest>(
				    { _position.x, _position.y, _position.z - 1 });
			else
				partnerChest = _world.GetTileEntityShared<TileEntityChest>(
				    { _position.x, _position.y, _position.z + 1 });
			if (!partnerChest)
				return false;

			bool isLeftSide = (r == BLOCK_CHEST || b == BLOCK_CHEST);
			if (!isLeftSide)
				std::swap(chest, partnerChest);

			Packet::OpenContainer ow;
			ow.windowId = _session.GetNextWindowId();
			ow.slotCount = 54;
			ow.title = "Large Chest";
			ow.windowType = PacketData::WindowType::CHEST;
			ow.Serialize(_session.stream);

			_session.activeInteraction = std::make_unique<LargeChestInventoryInteraction>(&_session.inventory, chest,
			                                                                              partnerChest);
			_session.activeInteraction->InitSnapshot();

			PacketUtilities::SendInventory(_session, _session.openWindowId, *_session.activeInteraction->inventory);
			return false;
		}

		// Setup interaction
		_session.activeInteraction = std::make_unique<ChestInventoryInteraction>(&_session.inventory, chest);
		_session.activeInteraction->InitSnapshot();

		// Single chest
		// Open the chest window
		Packet::OpenContainer ow;
		ow.windowId = _session.GetNextWindowId();
		ow.slotCount = 27;
		ow.title = "Chest";
		ow.windowType = PacketData::WindowType::CHEST;
		ow.Serialize(_session.stream);

		// Send inventory
		PacketUtilities::SendInventory(_session, _session.openWindowId, *_session.activeInteraction->inventory);
		return false;
	};

	blockBehaviors[BLOCK_JUKEBOX].onBlockActivated = [](WorldManager& _world, Int3 _position, PlayerSession& _session,
	                                                    Runtime& /*_gameRuntime*/) -> bool {
		auto jukebox = _world.GetTileEntityAs<TileEntityJukebox>(_position);
		if (!jukebox)
			return false;

		// Pop out item if already playing something
		if (jukebox->recordItemId != Items::Id::INVALID) {
			auto ejectedRecord = static_cast<Items::Id>(int16_t(jukebox->recordItemId));
			jukebox->recordItemId = Items::Id::INVALID;

			Blocks::DropItemAt(_world, _position, ejectedRecord, /*count=*/1, 0);

			if (auto& fn = _world.onWorldEvent)
				fn(PacketData::WorldEvent::RECORD_PLAY, _position, 0, nullptr);
			return false;
		}

		ItemStack* heldItem = _session.inventory.GetHeldItem();
		if (!heldItem || !Items::IsRecord(heldItem->id))
			return false;

		jukebox->recordItemId = heldItem->id;
		heldItem->DecrementCount(1);

		if (auto& fn = _world.onWorldEvent)
			fn(PacketData::WorldEvent::RECORD_PLAY, _position, int32_t(int16_t(jukebox->recordItemId)), nullptr);
		return false;
	};
	blockBehaviors[BLOCK_BED].onBlockActivated = [](WorldManager& _world, Int3 _position, PlayerSession& _session,
	                                                Runtime& /*_gameRuntime*/) -> bool {
		if (!_world.InBounds(_position.y))
			return false;
		// Already sleeping? Don't let the client re-trigger the interaction.
		if (_session.entity->isSleeping)
			return false;
		// Intentional game design (trademark)
		if (_world.thisDimension != Dimension::Overworld && Gamerule::Get<bool>(Gamerule::Id::NetherBedsExplode)) {
			// TODO: Probably deduplicate this via blockBehaviors[BLOCK_BED].onBlockDestroyedByPlayer
			Int3 headPos = _position;
			auto meta = _world.GetMetadata(headPos);
			if (!(meta & 0b1000)) {
				auto dir = GetDirectionFromMeta(BLOCK_BED, meta);
				headPos.Offset(dir);
				meta = _world.GetMetadata(headPos);
			}
			auto bedDir = GetDirectionFromMeta(BLOCK_BED, meta);
			Int3 footPos = headPos.WithOffset(Cardinal::Opposite(bedDir));

			Vec3 explosionCenter = {
				(headPos.x + footPos.x) / 2.0 + 0.5,
				headPos.y + 0.5,
				(headPos.z + footPos.z) / 2.0 + 0.5,
			};

			_world.SetBlock(headPos, BLOCK_AIR);
			_world.SetBlock(footPos, BLOCK_AIR);
			_world.DoExplosion(nullptr, explosionCenter, 5.0f, /*doFire=*/true);
			return false;
		}
		// Can only sleep once it's actually dark out.
		if (!_world.IsNight()) {
			SendChat(_session, "You can only sleep at night");
			return false;
		}
		// If not head, but foot, move to headboard
		{
			auto meta = _world.GetMetadata(_position);
			if (!(meta & 0b1000)) {
				auto dir = GetDirectionFromMeta(BLOCK_BED, meta);
				_position.Offset(dir);
			}
		}
		// Is someone already sleeping in this bed?
		for (auto& other : _world.entityManager.entities) {
			auto* otherPlayer = dynamic_cast<EntityMPPlayer*>(other.get());
			if (!otherPlayer || otherPlayer == _session.entity.get())
				continue;
			if (otherPlayer->isSleeping && otherPlayer->bedPosition == _position) {
				SendChat(_session, "This bed is occupied");
				return false;
			}
		}
		_session.entity->TrySleep(_position);
		return false;
	};
}