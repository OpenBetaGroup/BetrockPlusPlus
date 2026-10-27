/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 * Copyright (c) 2026, jomner <github.com/jomner>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "handle_packet.h"
#include "../blocks/server_block_behaviors.h"
#include "../commands/command.h"
#include "../trackers/entity_tracker.h"
#include "addon/addon_impl.h"
#include "blocks.h"
#include "blocks/block_properties.h"
#include "direction_fixer.h"
#include "entities/entity_item.h"
#include "inventory/inventory_interaction.h"
#include "inventory/item_stack.h"
#include "items/item_properties.h"
#include "items/tool_properties.h"
#include "logger.h"
#include "packet_utils.h"
#include "server.h"
#include "tile_entities/tile_entity.h"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <memory>
#include "gamerules.h"

bool IsValidActionInSpawnRadius(PlayerSession& _session, Server& _server, Int3 _pos, WorldManager& _world,
                                int _margin) {
	const int spawnProtection = Gamerule::Get<int32_t>(Gamerule::Id::SpawnProtectionRadius) + _margin;
	if (spawnProtection <= 0)
		return true;
	if (IsOperator(_session, _server))
		return true;

	Int3 normalPos = _pos - _world.GetSpawnPoint(/*Adjust=*/false);

	return (normalPos.x * normalPos.x + normalPos.z * normalPos.z) > (spawnProtection * spawnProtection);
}

namespace HandlePacket {
void KeepAlive(Packet::KeepAlive& /*pkt*/, PlayerSession& _session) {
	Packet::KeepAlive ka;
	ka.Serialize(_session.stream);
}

void ChatMessage(Packet::ChatMessage& _pkt, PlayerSession& _session,
                 std::vector<std::shared_ptr<PlayerSession>>& _players, WorldManager& _world, CommandManager& _cmdMgr,
                 std::function<void(PlayerSession&)> _transferDimension, Server& _server) {
	if (!_session.entity)
		return;

	{
		bp_player_chat_event event{ .player = &_session.apiPlayer, .message = _pkt.message.c_str(), .cancel = false };
		_server.GetAddonManager().Broadcast(&bp_addon_events::playerChat, event);

		if (event.cancel)
			return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (_session.recentChatMessageCount == _session.recentChatMessageTimes.size() &&
	    now - _session.recentChatMessageTimes[_session.nextChatMessageTime] < std::chrono::seconds(1)) {
		_server.DisconnectPlayer("Chat spamming!", _session);
		return;
	}
	_session.recentChatMessageTimes[_session.nextChatMessageTime] = now;
	_session.nextChatMessageTime =
	    (_session.nextChatMessageTime + 1) % _session.recentChatMessageTimes.size();
	if (_session.recentChatMessageCount < _session.recentChatMessageTimes.size())
		++_session.recentChatMessageCount;

	Logger::Message << "<" << _session.username << "> " << _pkt.message << "\n";
	if (!_pkt.message.empty() && _pkt.message.front() == '/') {
		CommandContext ctx;
		ctx.session = &_session;
		ctx.sender = &_session;
		ctx.world = &_world;
		ctx.server = &_server;
		ctx.transferDimension = _transferDimension;
		_cmdMgr.Parse(_pkt.message, ctx);
		return;
	}

	Packet::ChatMessage reply;
	reply.message = "<" + _session.username + "> " + _pkt.message;

	// This includes the current session too
	for (auto& receiver : _players) {
		if (receiver->connState != ConnectionState::Playing)
			continue;
		reply.Serialize(receiver->stream);
	}
}

void PlayerMovement(Packet::PlayerMovement& _pkt, PlayerSession& _session) {
	// On ground flag from client
	if (_session.entity) {
		_session.entity->onGround = _pkt.onGround;
	}
}

static bool HandlePlayerMove(Vec3 _to, PlayerSession& _session, Server& _server) {
	// We are in a vehicle!
	if (_to.y == -999.0 || (_session.entity && !_session.entity->vehicle.expired())) {
		_session.pendingPosition = _to;
		return true;
	}

	if (_session.entity)
		_session.position.pos = _session.entity->position;

	bp_player_move_event event{ .player = &_session.apiPlayer,
		                        .from = { _session.position.pos.x, _session.position.pos.y, _session.position.pos.z },
		                        .to = { _to.x, _to.y, _to.z },
		                        .cancel = false };
	_server.GetAddonManager().Broadcast(&bp_addon_events::playerMove, event);

	const Vec3 finalTo = { event.to.x, event.to.y, event.to.z };

	if (event.cancel || finalTo != _to) {
		Packet::PlayerPositionAndRotation pkt;
		pkt.rotation = _session.rotation;
		pkt.onGround = _session.entity ? _session.entity->onGround : true;
		pkt.position = { finalTo.x, finalTo.y + PLAYER_EYE_HEIGHT, finalTo.z };
		pkt.cameraY = finalTo.y; // This is backwards, thanks notch
		pkt.Serialize(_session.stream);
	}

	if (event.cancel)
		return false;

	_session.position.pos = { event.from.x, event.from.y, event.from.z };
	_session.pendingPosition = finalTo;

	return true;
}

void PlayerPosition(Packet::PlayerPosition& _pkt, PlayerSession& _session, Server& _server) {
	if (!HandlePlayerMove(_pkt.position, _session, _server))
		return;

	_session.pendingPosition = { _pkt.position.x, _pkt.position.y, _pkt.position.z };
	if (_session.entity) {
		_session.entity->onGround = _pkt.onGround;
		_session.entity->HandlePositionChecks();
	}
}

void PlayerRotation(Packet::PlayerRotation& _pkt, PlayerSession& _session) {
	_session.rotation.x = _pkt.yaw;
	_session.rotation.y = _pkt.pitch;
	if (_session.entity) {
		_session.entity->onGround = _pkt.onGround;
	}
}

void PlayerPositionAndRotation(Packet::PlayerPositionAndRotation& _pkt, PlayerSession& _session, Server& _server) {
	_session.rotation.x = _pkt.yaw;
	_session.rotation.y = _pkt.pitch;

	if (!HandlePlayerMove(_pkt.position, _session, _server))
		return;

	_session.pendingPosition = { _pkt.position.x, _pkt.position.y, _pkt.position.z };
	if (_session.entity) {
		_session.entity->onGround = _pkt.onGround;
		_session.entity->HandlePositionChecks();
	}
}

void MineBlock(Packet::MineBlock& _pkt, PlayerSession& _session, WorldManager& _world, Server& _server) {
	Int3 packetPos = { _pkt.position.x, _pkt.position.y, _pkt.position.z };

	if (!_session.entity)
		return;

	auto entityPos = _session.entity->position;

	auto resyncBlock = [&](Int3 _pos) {
		if (!_world.onBlockUpdate)
			return;
		auto* chunk = _world.GetChunkRaw({ _pos.x >> 4, _pos.z >> 4 });
		if (!chunk)
			return;
		Int3 local{ _pos.x & 15, _pos.y, _pos.z & 15 };
		_world.onBlockUpdate(PendingBlock{ .block{ chunk->GetBlock(local), chunk->GetMeta(local) },
		                                   .blockPos{ _pos.x, _pos.y, _pos.z },
		                                   .light{ chunk->GetBlockLight(local), chunk->GetSkyLight(local) } },
		                     chunk->cpos);
	};

	if (_pkt.status != PacketData::MineStatus::DROPPED_ITEM) {
		if (packetPos.Distance({ int(entityPos.x), int(entityPos.y), int(entityPos.z) }) > 6.0)
			return;
		if (!IsValidActionInSpawnRadius(_session, _server, packetPos, _world)) {
			resyncBlock(packetPos);
			return;
		}
	}

	switch (_pkt.status) {
	case PacketData::MineStatus::DIGGING_STARTED: {
		// invoke fire extinguish if matched
		Cardinal::Value clickFace = FaceDirectionToDirection(_pkt.face);
		if (clickFace != Cardinal::Value::None) {
			Int3 adjacentPos = packetPos.WithOffset(clickFace);
			if (_world.GetBlockId(adjacentPos) == BLOCK_FIRE) {
				if (auto fn = Blocks::blockBehaviors[BLOCK_FIRE].onBlockClicked) {
					fn(_world, adjacentPos, &_session);
				}
			}
		}

		// to support clients that implement fire collision
		BlockType blockId = _world.GetBlockId(packetPos);
		if (blockId == BLOCK_FIRE) {
			if (auto fn = Blocks::blockBehaviors[BLOCK_FIRE].onBlockClicked) {
				fn(_world, packetPos, &_session);
			}
			return;
		}

		_session.pendingBlockBreak = { .lastBlock = blockId, .lastBlockPos = packetPos };

		float hardness = Blocks::blockProperties[blockId].hardness;
		if (hardness < 0.0f) {
			resyncBlock(packetPos);
			_session.pendingBlockBreak.reset();
			return;
		}

		if (auto fn = Blocks::blockBehaviors[blockId].onBlockClicked) {
			fn(_world, packetPos, &_session);
		}

		ItemStack* heldItem = _session.inventory.GetHeldItem();

		if (heldItem) {
			auto it = Items::toolBehavior.find(heldItem->id);
			if (it != Items::toolBehavior.end() && it->second.onBlockStartMining)
				it->second.onBlockStartMining(heldItem, blockId);
		}
		return;
	}
	case PacketData::MineStatus::DIGGING_FINISHED: {
		auto newBlockId = _world.GetBlockId(packetPos);

		if (!_session.pendingBlockBreak.has_value() || _session.pendingBlockBreak->lastBlockPos != packetPos) {
			resyncBlock(packetPos);
			_session.pendingBlockBreak.reset();
			return;
		}

		if (_session.pendingBlockBreak->lastBlock != newBlockId) {
			resyncBlock(packetPos);
			_session.pendingBlockBreak.reset();
			return;
		}

		//TODO: Extract common bp_block_use_event and bp_block_break_event code to some helper
		ItemStack* heldItem = _session.inventory.GetHeldItem();

		bp_block_break_event event{ .player = &_session.apiPlayer,
			                        .world = &_world.apiWorld,
			                        .tool = AddonHelper::ToBpStack(heldItem),
			                        .blockPos = { packetPos.x, packetPos.y, packetPos.z },
			                        .block = { .id = newBlockId, .meta = _world.GetMetadata(packetPos) },
			                        .cancel = false };

		const auto oldItem = heldItem ? *heldItem : ItemStack{};

		AddonHelper::FromBpStack(heldItem, event.tool);

		_server.GetAddonManager().Broadcast(&bp_addon_events::blockBreak, event);

		if (heldItem && *heldItem != oldItem) {
			PacketUtilities::SendSlot(_session, 0, _session.inventory.activeHotbarSlot + 36, heldItem);
		}

		// Resync if we missed our break
		if (event.cancel || !_server.TryForceBreak(_session, _world))
			resyncBlock(packetPos);
		return;
	}
	case PacketData::MineStatus::DROPPED_ITEM: {
		ItemStack* heldStack = _session.inventory.GetHeldItem();
		if (!heldStack)
			return;

		ItemStack droppedStack = *heldStack;
		droppedStack.count = 1;

		if (_session.entity->DropItem(droppedStack))
			heldStack->DecrementCount(1);
		return;
	}
	default:
		return;
	}
}

void PlaceBlock(Packet::PlaceBlock& _pkt, PlayerSession& _session, WorldManager& _world, Runtime& _gameRuntime,
                Server& _server) {
	Int3 position = { _pkt.position.x, _pkt.position.y, _pkt.position.z };

	if (!_session.entity)
		return;

	// Block interactions
	auto block = _world.GetBlockId(position);

	auto resyncBlock = [&](Int3 _pos) {
		if (!_world.onBlockUpdate)
			return;
		auto* chunk = _world.GetChunkRaw({ _pos.x >> 4, _pos.z >> 4 });
		if (!chunk)
			return;
		Int3 local{ _pos.x & 15, _pos.y, _pos.z & 15 };
		_world.onBlockUpdate(PendingBlock{ .block{ chunk->GetBlock(local), chunk->GetMeta(local) },
		                                   .blockPos{ _pos.x, _pos.y, _pos.z },
		                                   .light{ chunk->GetBlockLight(local), chunk->GetSkyLight(local) } },
		                     chunk->cpos);
	};

	ItemStack* heldItem = _session.inventory.GetHeldItem();

	{
		bp_block_use_event event{ .player = &_session.apiPlayer,
			                      .world = &_world.apiWorld,
			                      .heldItem = AddonHelper::ToBpStack(heldItem),
			                      .blockPos = { position.x, position.y, position.z },
			                      .block = { block, _world.GetMetadata(position) },
			                      .cancel = false };

		const auto oldItem = heldItem ? *heldItem : ItemStack{};

		_server.GetAddonManager().Broadcast(&bp_addon_events::blockUse, event);

		if (heldItem && *heldItem != oldItem) {
			PacketUtilities::SendSlot(_session, 0, _session.inventory.activeHotbarSlot + 36, heldItem);
		}

		if (event.cancel) {
			resyncBlock(position);
			return;
		}
	}

	// Function returns true if we can place a block after running the function
	if (ServerBlock::blockBehaviors[block].onBlockActivated) {
		if (!ServerBlock::blockBehaviors[block].onBlockActivated(_world, position, _session, _gameRuntime)) {
			return;
		}
	}
	// The server didn't override our block's behavior so check the base behavior
	else if (Blocks::blockBehaviors[block].onBlockActivated) {
		if (!Blocks::blockBehaviors[block].onBlockActivated(_world, position, &_session)) {
			return;
		}
	}

	if (!heldItem) {
		return;
	}

	// Items skip the block placement check below
	if (Items::itemBehavior[heldItem->id].onBlockUse && _pkt.face != PacketData::FaceDirection::INVALID_USE) {
		const Int3 target = position.WithOffset(FaceDirectionToDirection(_pkt.face));
		if (!IsValidActionInSpawnRadius(_session, _server, target, _world, 1)) {
			resyncBlock(position);
			resyncBlock(target);
			PacketUtilities::SendSlot(_session, 0, _session.inventory.activeHotbarSlot + 36, heldItem);
			return;
		}
	}

	// NOTE:
	// Invalid Use packet is sent ANYTIME the client predicts a placement will fail (like placing a block inside of yourself)
	if (_pkt.face == PacketData::FaceDirection::INVALID_USE) {
		bp_item_use_event event{ .player = &_session.apiPlayer,
			                     .item = AddonHelper::ToBpStack(heldItem),
			                     .cancel = false };

		const auto oldItem = heldItem ? *heldItem : ItemStack{};

		_server.GetAddonManager().Broadcast(&bp_addon_events::itemUse, event);

		AddonHelper::FromBpStack(heldItem, event.item);

		if (event.cancel || (heldItem && *heldItem != oldItem)) {
			PacketUtilities::SendSlot(_session, 0, _session.inventory.activeHotbarSlot + 36, heldItem);
		}

		if (event.cancel)
			return;

		// Food check
		if (Items::IsFood(heldItem->id)) {
			if (auto& fn = Items::itemBehavior[heldItem->id].onUse) {
				fn(_session, heldItem, *_session.entity);
			}
			return;
		}

		// Bow check
		if (heldItem->id == Items::Id::BOW) {
			if (auto& fn = Items::itemBehavior[heldItem->id].onUse)
				fn(_session, heldItem, *_session.entity);
			return;
		}

		// Throwable check
		if (heldItem->id == Items::Id::EGG || heldItem->id == Items::Id::SNOWBALL) {
			if (auto& fn = Items::itemBehavior[heldItem->id].onUse)
				fn(_session, heldItem, *_session.entity);
			return;
		}

		if (heldItem->id == Items::Id::FISHING_ROD) {
			if (auto& fn = Items::itemBehavior[heldItem->id].onUse)
				fn(_session, heldItem, *_session.entity);
			return;
		}
		// Special cases
		bool isBucketItem = (heldItem->id == Items::Id::BUCKET || heldItem->id == Items::Id::BUCKET_WATER ||
		                     heldItem->id == Items::Id::BUCKET_LAVA);
		bool isBoatItem = heldItem->id == Items::Id::BOAT;
		bool hasOnBlockUse = static_cast<bool>(Items::itemBehavior[heldItem->id].onBlockUse);

		if ((isBucketItem || isBoatItem) && hasOnBlockUse) {
			Items::itemBehavior[heldItem->id].onBlockUse(_world, heldItem, position, *_session.entity,
			                                             FaceDirectionToDirection(_pkt.face));
		}
		return;
	}

	if (Items::IsBlock(heldItem->id)) {
		Int3 placePosition = position.WithOffset(FaceDirectionToDirection(_pkt.face));

		if (heldItem->id.value < BLOCK_AIR || heldItem->id.value >= Items::Id::SHOVEL_IRON) {
			return;
		}

		if (!IsValidActionInSpawnRadius(_session, _server, placePosition, _world)) {
			resyncBlock(placePosition);
			PacketUtilities::SendInventory(_session, 0, _session.inventory);
			return;
		}

		auto blockId = BlockType(heldItem->id.value);

		bp_block_place_event event{ .player = &_session.apiPlayer,
			                        .world = &_world.apiWorld,
			                        .blockPos = { placePosition.x, placePosition.y, placePosition.z },
			                        .blockId = blockId,
			                        .cancel = false };
		_server.GetAddonManager().Broadcast(&bp_addon_events::blockPlace, event);
		if (!event.cancel) {
			// We can place the block here
			auto function = Blocks::blockBehaviors[blockId].onBlockPlaced;
			if (!function) {
				return;
			}
			auto entityPos = _session.entity->position;
			if (position.Distance({ int(entityPos.x), int(entityPos.y), int(entityPos.z) }) > MAXIMUM_PLACEMENT_REACH)
				return;
			bool result = function(_world, placePosition, *_session.entity, FaceDirectionToDirection(_pkt.face),
			                       blockId, heldItem->data);
			if (result) {
				heldItem->DecrementCount(1);
				return;
			}
		}

		// Result failed so resync
		resyncBlock(position);
		resyncBlock(placePosition);
		PacketUtilities::SendSlot(_session, 0, _session.inventory.activeHotbarSlot + 36, heldItem);
	} else if (Items::IsItem(heldItem->id)) {
		bool isBucketItem = (heldItem->id == Items::Id::BUCKET || heldItem->id == Items::Id::BUCKET_WATER ||
		                     heldItem->id == Items::Id::BUCKET_LAVA);

		if (Items::itemBehavior[heldItem->id].onBlockUse && !isBucketItem) {
			Items::itemBehavior[heldItem->id].onBlockUse(_world, heldItem, position, *_session.entity,
			                                             FaceDirectionToDirection(_pkt.face));
		}
	}
}

void SetHotbarSlot(Packet::SetHotbarSlot& _pkt, PlayerSession& _session) {
	if (_pkt.slot < 0 || _pkt.slot >= 9)
		return;
	// Update previously held item
	auto previousHeldItem = _session.inventory.GetHeldItem();
	if (previousHeldItem)
		if (auto fn = Items::itemBehavior[previousHeldItem->id].onStopHolding) {
			fn(previousHeldItem);
		}
	// Move to new slot
	_session.inventory.activeHotbarSlot = _pkt.slot;
	// Update held item start holding
	// TODO: This doesn't update when an item is given or picked up!!!!
	auto heldItem = _session.inventory.GetHeldItem();
	if (heldItem)
		if (auto fn = Items::itemBehavior[heldItem->id].onStartHolding) {
			fn(heldItem, _session);
		}
}

// Click handler
void ClickSlot(Packet::ClickSlot& _pkt, PlayerSession& _session) {
	if (_session.inventoryLocked)
		return;
	_session.pendingWindowId = _pkt.windowId;
	_session.pendingTransactionId = _pkt.transactionId;

	// Clicked outside the window
	if (_pkt.slotId == -999) {
		InventoryInteraction* interaction = nullptr;
		if (_pkt.windowId == 0)
			interaction = &_session.inventoryInteraction;
		else if (_session.activeInteraction)
			interaction = _session.activeInteraction.get();
		if (!interaction)
			return;

		if (interaction->carried.id != Items::Id::INVALID) {
			if (_pkt.rightClick) {
				// Drop just one item, keep the rest held
				ItemStack dropped = interaction->carried;
				dropped.count = 1;
				interaction->carried.DecrementCount(1);
				if (interaction->carried.count <= 0)
					interaction->carried = ItemStack{ Items::Id::INVALID };
				_session.entity->DropItem(dropped);
			} else {
				// Drop the entire held stack
				_session.entity->DropItem(interaction->carried);
				interaction->carried = ItemStack{ Items::Id::INVALID };
			}
		}
		return;
	}

	// The player's inventory is handled separate
	if (_pkt.windowId == 0) {
		// Make sure what the client thinks and what we have line up
		ItemStack empty{ Items::Id::INVALID };
		auto expected = _session.inventory.GetStackInSlot(_pkt.slotId);
		ItemStack& slotItem = expected ? *expected : empty;

		// Clicked inside the window
		if (slotItem.id != _pkt.item.id || slotItem.data != _pkt.item.data || slotItem.count != _pkt.item.count) {
			Packet::ContainerTransaction ct;
			ct.accepted = false;
			ct.transactionId = _session.pendingTransactionId;
			ct.windowId = _pkt.windowId;
			ct.Serialize(_session.stream);
			_session.inventoryLocked = true;

			// Reset the held cursor
			PacketUtilities::SendSlot(_session, -1, -1, &_session.inventoryInteraction.carried);

			PacketUtilities::SendInventory(_session, _pkt.windowId, _session.inventory);
			return;
		}

		// Everything lined up so go as normal
		if (_pkt.shift) {
			_session.inventoryInteraction.OnShiftClick(_pkt.slotId);
			return;
		}
		if (_pkt.rightClick) {
			_session.inventoryInteraction.OnRightClick(_pkt.slotId);
			return;
		}
		_session.inventoryInteraction.OnLeftClick(_pkt.slotId);
		return;
	}
	ItemStack empty{ Items::Id::INVALID };
	if (!_session.activeInteraction || !_session.activeInteraction->inventory)
		return;
	auto expected = _session.activeInteraction->inventory->GetStackInSlot(_pkt.slotId);
	ItemStack& slotItem = expected ? *expected : empty;
	if (slotItem.id != _pkt.item.id || slotItem.data != _pkt.item.data || slotItem.count != _pkt.item.count) {
		Packet::ContainerTransaction ct;
		ct.accepted = false;
		ct.transactionId = _session.pendingTransactionId;
		ct.windowId = _pkt.windowId;
		ct.Serialize(_session.stream);
		_session.inventoryLocked = true;

		// Reset the held cursor
		PacketUtilities::SendSlot(_session, -1, -1, &_session.activeInteraction->carried);
		PacketUtilities::SendInventory(_session, _pkt.windowId, *_session.activeInteraction->inventory);
		return;
	}

	// Everything lined up so go as normal
	if (_pkt.shift) {
		_session.activeInteraction->OnShiftClick(_pkt.slotId);
		return;
	}
	if (_pkt.rightClick) {
		_session.activeInteraction->OnRightClick(_pkt.slotId);
		return;
	}
	_session.activeInteraction->OnLeftClick(_pkt.slotId);
	return;
}

void CloseContainer(Packet::CloseContainer& _pkt, PlayerSession& _session) {
	if (_pkt.windowId == 0 && _session.entity) {
		// Drop the crafting grid items on inventory close
		for (size_t i = 1; i <= 4; i++) {
			ItemStack& stack = _session.inventory.slots[i];
			_session.entity->DropItem(stack);
			stack = ItemStack{};
		}
		// Drop the cursor
		if (_session.inventoryInteraction.carried.id != Items::INVALID) {
			_session.entity->DropItem(_session.inventoryInteraction.carried);
			_session.inventoryInteraction.carried = ItemStack{};
		}
	}

	// Trigger the close interaction
	if (_session.activeInteraction)
		_session.activeInteraction->OnInteractionClosed(*_session.entity);

	// Get rid of our active interaction and reset the window id
	_session.activeInteraction = nullptr;
	_session.openWindowId = 0;
}

// Client acknowledges a rejected transaction
void ContainerTransaction(Packet::ContainerTransaction& _pkt, PlayerSession& _session) {
	if (_session.inventoryLocked && _pkt.windowId == _session.pendingWindowId &&
	    _pkt.transactionId == _session.pendingTransactionId) {
		_session.inventoryLocked = false;
	}
}

// Other handlers
void InteractWithEntity(Packet::InteractWithEntity& _pkt, PlayerSession& _session, WorldManager& _world) {
	// Check if session entity and source entity match
	if (!_session.entity)
		return;

	if (_pkt.sourceEntityId != _session.entity->id)
		return;

	// Check if target entity exists
	auto entity = _world.entityManager.GetEntityByIdShared(_pkt.targetEntityId);
	auto sourceEntity = _world.entityManager.GetEntityByIdShared(_pkt.sourceEntityId);
	if (!entity || !sourceEntity)
		return;

	if (entity->position.Distance(sourceEntity->position) > 6.0)
		return;

	ItemStack* heldItem = _session.inventory.GetHeldItem();
	if (!heldItem) {
		if (!_pkt.attack) {
			PlayerEntity* playerPtr = dynamic_cast<PlayerEntity*>(sourceEntity.get());
			entity->OnPlayerInteract(playerPtr);
			return;
		}
		ItemStack emptyStack{};
		Items::AttackWithItem(*entity, *sourceEntity, &emptyStack);
		return;
	}

	// Get tool behavior
	auto iter = Items::toolBehavior.find(heldItem->id);
	if (iter == Items::toolBehavior.end()) {
		if (_pkt.attack) {
			Items::AttackWithItem(*entity, *sourceEntity, heldItem);
		} else {
			PlayerEntity* playerPtr = dynamic_cast<PlayerEntity*>(sourceEntity.get());
			entity->OnPlayerInteract(playerPtr);
		}
		return;
	}

	const auto& behavior = iter->second;

	// Attack properly!
	if (_pkt.attack) {
		if (behavior.onEntityAttack)
			behavior.onEntityAttack(*entity, *sourceEntity, heldItem);
		else
			Items::AttackWithItem(*entity, *sourceEntity, heldItem);
	} else {
		if (behavior.onEntityUse)
			behavior.onEntityUse(_world, *entity, heldItem);
		else {
			PlayerEntity* playerPtr = dynamic_cast<PlayerEntity*>(sourceEntity.get());
			entity->OnPlayerInteract(playerPtr);
		}
	}
}

void InteractWithBlock(Packet::InteractWithBlock& /*_pkt*/, PlayerSession& /*_session*/, WorldManager& /*_world*/) {}

void Animation(Packet::Animation& _pkt, PlayerSession& _session, EntityTracker& _entityTracker) {
	// Broadcast what we were sent to players who can see this player
	Packet::Animation anim;
	anim.entityId = _session.entity->id;
	anim.animation = _pkt.animation;
	_entityTracker.SendPacketToViewers(anim, anim.entityId);
}

void PlayerAction(Packet::PlayerAction& _pkt, PlayerSession& _session, EntityTracker& /*_entityTracker*/) {
	auto& entity = _session.entity;
	if (!entity)
		return;
	switch (_pkt.action) {
	case PacketData::PlayerAction::START_SNEAKING:
		entity->UpdateMetadata(entity->flags.isSneaking, true);
		break;
	case PacketData::PlayerAction::STOP_SNEAKING:
		entity->UpdateMetadata(entity->flags.isSneaking, false);
		break;
	case PacketData::PlayerAction::STOP_SLEEPING: {
		Logger::Debug << "STOP_SLEEPING: waking up " << _session.username << " from position "
		                     << entity->position << " (isSleeping=" << entity->isSleeping << ")\n";
		entity->WakeUp();
		break;
	}
	default:
		break;
	}
	// Broadcast what we were sent to players who can see this player
}

void Respawn(Packet::Respawn& /*_pkt*/, PlayerSession& _session, Server& _server) {
	Logger::Info << "Respawned Player " << _session.username << "!\n";

	constexpr Dimension targetDim = Dimension::Overworld;
	const bool changingDim = _session.dimension != targetDim;

	Packet::Respawn pkt;
	pkt.dimension = targetDim;
	pkt.Serialize(_session.stream);

	// Unregister the dead entity from the world it died in
	auto oldWorld = _server.GetWorldForDimension(_session.dimension);
	oldWorld->entityManager.RemoveEntity(_session.entity->id);
	_session.entity->session = nullptr;

	auto newEntity = std::make_shared<EntityMPPlayer>(std::string(_session.username.begin(), _session.username.end()));
	newEntity->session = &_session;
	newEntity->id = _session.entity->id;
	_session.entity = newEntity;

	if (changingDim)
		_server.ResetDimensionState(_session, targetDim);

	auto world = _server.GetWorldForDimension(targetDim);
	Int3 spawn = {};
	bool bedPositionFindFailed = true;
	if (_session.hasBedSpawn) {
		Int3 headPos = _session.spawnPosition.WithOffset(Cardinal::Value::Down);
		auto bedDir = GetDirectionFromMeta(BLOCK_BED, world->GetMetadata(headPos));
		Int3 footPos = headPos.WithOffset(Cardinal::Opposite(bedDir));
		auto sleepPositions = Blocks::GetBedApproachSpots(*world, headPos, footPos);
		if (!sleepPositions.empty()) {
			bedPositionFindFailed = false;
			spawn = sleepPositions[0];
		}
	}
	if (bedPositionFindFailed) {
		if (_session.hasBedSpawn)
			SendChat(_session, "Your home bed was missing or obstructed");
		_session.hasBedSpawn = false;
		spawn = world->GetSpawnPoint(/*Random Adjust=*/true);
	}
	Vec3 spawnLocation = { double(spawn.x) + 0.5, double(spawn.y) + 0.01, double(spawn.z) + 0.5 };

	newEntity->Teleport(spawnLocation);
	world->entityManager.AddEntity(newEntity, newEntity->id); // registers in exactly one world

	_session.position.pos = spawnLocation;
	_session.pendingTeleport = spawnLocation;
	_session.pendingPosition.reset();

	if (changingDim) {
		_session.connState = ConnectionState::WaitingForSpawnChunks;
		return;
	}

	Packet::PlayerPositionAndRotation tpPkt;
	tpPkt.position = { spawnLocation.x, spawnLocation.y + PLAYER_EYE_HEIGHT, spawnLocation.z };
	tpPkt.cameraY = spawnLocation.y;
	tpPkt.yaw = 0;
	tpPkt.pitch = 0;
	tpPkt.onGround = false;
	tpPkt.Serialize(_session.stream);

	Packet::SetTime stPkt;
	stPkt.time = world->elapsedTicks;
	stPkt.Serialize(_session.stream);

	if (Gamerule::Get<bool>(Gamerule::Id::KeepInventory))
		PacketUtilities::SendInventory(_session, 0, _session.inventory);
}

void UpdateSign(Packet::UpdateSign& _pkt, PlayerSession& /*_session*/, WorldManager& _world,
                std::vector<std::shared_ptr<PlayerSession>>& _players) {
	Int3 position = { _pkt.position.x, _pkt.position.y, _pkt.position.z };
	BlockType blockId = _world.GetBlockId(position);
	if (blockId != BLOCK_SIGN_STANDING && blockId != BLOCK_SIGN_WALL)
		return;

	auto tile = std::make_shared<TileEntitySign>(position);

	tile->text1 = _pkt.lines[0];
	tile->text2 = _pkt.lines[1];
	tile->text3 = _pkt.lines[2];
	tile->text4 = _pkt.lines[3];

	_world.CreateTileEntity(tile);

	for (auto& viewer : _players) {
		if (viewer->connState != ConnectionState::Playing || viewer->dimension != _world.thisDimension)
			continue;

		Int2 chunkCoord = Int2{ static_cast<int32_t>(position.x >> 4),
			                    static_cast<int32_t>(position.z >> 4) };
		if (!viewer->sentChunks.contains(chunkCoord))
			continue;

		_pkt.Serialize(viewer->stream);
	}
}

void Disconnect(Packet::Disconnect& _pkt, PlayerSession& _session, Server& _server) {
	_server.DisconnectPlayer(_pkt.reason, _session);
}

} // namespace HandlePacket