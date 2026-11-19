/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "inventory_tracker.h"

#include "base_types.h"
#include "dimensions.h"
#include "inventory/interactions/furnace.h"
#include "inventory/interactions/player.h"
#include "inventory/inventories.h"
#include "inventory/inventory_interaction.h"
#include "inventory/item_stack.h"
#include "items/item_properties.h"
#include "networking/packets.h"
#include "packet/packet_utils.h"
#include "packet_data.h"
#include "player_conn/player_session_mp.h"
#include "runtime.h"
#include "server.h"
#include "tile_entities/tile_entity_furnace.h"
#include "world.h"
#include "world/storage/save_manager.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

void InventoryTracker::Tick(Server& _server) {
	for (auto& session : _server.GetPlayers()) {
		// Diff window 0 every tick
		{
			auto diffs2 = session->inventoryInteraction.TickDiff();
			if (diffs2.size() <= 5) {
				for (auto difference : diffs2) {
					PacketUtilities::SendSlot(*session, 0, difference.slot, &difference.stack);
				}
			} else {
				// Too many changes, just resend the whole inventory
				PacketUtilities::SendInventory(*session, 0, *session->inventoryInteraction.inventory);
			}
		}

		// Held item updates (i.e. maps)
		{
			auto heldItem = session->inventory.GetHeldItem();
			if (heldItem) {
				if (auto fn = Items::itemBehavior[heldItem->id].whileHeld) {
					fn(heldItem, *session, _server);
				}
			}
		}

		if (!session->activeInteraction)
			continue;

		// Force close windows that reference tile entities that have been deleted
		if (!session->activeInteraction->CanExist(*session->entity)) {
			session->activeInteraction->OnInteractionClosed(*session->entity);
			PacketUtilities::CloseContainer(*session);
			continue;
		}

		// Send each differing slot
		auto diffs = session->activeInteraction->TickDiff();
		if (diffs.size() <= 5) {
			for (auto difference : diffs) {
				PacketUtilities::SendSlot(*session, session->openWindowId, difference.slot, &difference.stack);
			}
		} else {
			// Too many changes, just resend the whole inventory
			PacketUtilities::SendInventory(*session, session->openWindowId, *session->activeInteraction->inventory);
		}

		// Sync container data
		if (auto* furnaceInteraction = dynamic_cast<FurnaceInventoryInteraction*>(session->activeInteraction.get())) {
			auto furnace = furnaceInteraction->tile.lock();
			if (furnace) {
				if ((furnace->dirtyFlags & TileEntityFurnace::FLAG_BURN_TIME) != 0) {
					Packet::ContainerData pkt;
					pkt.windowId = session->openWindowId;
					pkt.containerData.type = PacketData::ContainerDataType::FUEL_REMAINING;
					pkt.containerData.value = furnace->GetBurnTime();
					pkt.Serialize(session->stream);
				}

				if ((furnace->dirtyFlags & TileEntityFurnace::FLAG_MAX_BURN_TIME) != 0) {
					Packet::ContainerData pkt;
					pkt.windowId = session->openWindowId;
					pkt.containerData.type = PacketData::ContainerDataType::FUEL_DURATION;
					pkt.containerData.value = furnace->GetMaxBurnTime();
					pkt.Serialize(session->stream);
				}

				if ((furnace->dirtyFlags & TileEntityFurnace::FLAG_COOK_TIME) != 0) {
					Packet::ContainerData pkt;
					pkt.windowId = session->openWindowId;
					pkt.containerData.type = PacketData::ContainerDataType::SMELTING_PROGRESS;
					pkt.containerData.value = furnace->GetCookTime();
					pkt.Serialize(session->stream);
				}
			}
		}

		if (_server.gameRuntime.GetWorldForDimension(Dimension::Overworld)->elapsedTicks % 40 == 0) {
			// Save periodically
			auto savedNbt = session->SerializeToNbt();
			_server.gameRuntime.saveManager.SavePlayerNbt(
			    std::string(session->username.begin(), session->username.end()), savedNbt);
		}
	}
}