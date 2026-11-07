#define ADDON_API_IMPLEMENTATION
#include "../include/addon_api.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool IsDoorOpen(const bp_api* api, bp_world* world, bp_block_pos pos, uint8_t meta) {
	// If this is the top half, get metadata from the bottom half.
	if (meta & 8) {
		pos.y--;
		meta = api->world.getBlock(world, pos).meta;
	}

	return (meta & 4) != 0;
}

static bp_block_pos GetOtherDoorHalf(bp_block_pos pos, uint8_t meta) {
	if (meta & 8)
		pos.y--;
	else
		pos.y++;

	return pos;
}

void OnPlayerJoin(const bp_api* api, const bp_player_join_event* ev) {
	api->player.sendMessage(ev->player, "Welcome to the server! Sent by the test plugin!");
}

#define MAX_KEYS 64

struct Key {
	bp_block_pos doorPos;
};

static struct Key keys[MAX_KEYS];
static int keyCount = 0;

bool MatchesDoor(bp_block_pos storedPos, bp_block_pos targetPos, uint8_t targetMeta) {
	if (bp_block_pos_equals(storedPos, targetPos))
		return true;

	bp_block_pos otherPos = GetOtherDoorHalf(targetPos, targetMeta);
	if (bp_block_pos_equals(otherPos, storedPos))
		return true;

	return false;
}

void OnDoorUse(const bp_api* api, bp_block_use_event* ev) {
	if (IsDoorOpen(api, ev->world, ev->blockPos, ev->block.meta))
		return;

	// Sign item
	if (ev->heldItem.id == 323) {
		if (ev->heldItem.data == 0) {
			keys[keyCount].doorPos = ev->blockPos;
			ev->heldItem.data = ++keyCount;

			api->player.sendMessage(ev->player, "Assigned key to this door");
		} else if (ev->heldItem.data - 1 < keyCount) {
			if (MatchesDoor(keys[ev->heldItem.data - 1].doorPos, ev->blockPos, ev->block.meta))
				return;
			else
				api->player.sendMessage(ev->player, "Wrong key!");
		}
	} else {
		bool locked = false;
		for (int i = 0; i < keyCount; ++i) {
			if (MatchesDoor(keys[i].doorPos, ev->blockPos, ev->block.meta)) {
				locked = true;
				break;
			}
		}
		if (!locked)
			return;

		api->player.sendMessage(ev->player, "This door is locked! Use a key");
	}

	// Prevent the door from opening.
	ev->cancel = true;

	// The normal cancellation resyncs the clicked half,
	// but the other half also needs to be resynced.
	bp_block_pos otherPos = GetOtherDoorHalf(ev->blockPos, ev->block.meta);

	bp_block otherBlock = api->world.getBlock(ev->world, otherPos);

	otherBlock.meta &= ~4; // Closed state

	api->world.sendBlockUpdate(ev->world, otherPos, otherBlock);
}

void OnStairsUse(const bp_api* api, bp_block_use_event* ev) {
	//TODO: Spawn item entity and make player ride it
}

void OnBlockUse(const bp_api* api, bp_block_use_event* ev) {
	// Wooden door
	if (ev->block.id == 64)
		OnDoorUse(api, ev);
	// Wooden-cobblestone stairs
	else if (ev->block.id == 53 || ev->block.id == 67)
		OnStairsUse(api, ev);
}

void OnBlockBreak(const bp_api* api, bp_block_break_event* ev) {
	char* msg;
	asprintf(&msg, "You broke a %d:%d block at %d %d %d using a (%d:%d)x%d", ev->block.id, ev->block.meta,
	         ev->blockPos.x, ev->blockPos.y, ev->blockPos.z, ev->tool.id, ev->tool.data, ev->tool.count);
	api->player.sendMessage(ev->player, msg);
	free(msg);

	ev->cancel = true;
}

void OnBlockPlace(const bp_api* api, bp_block_place_event* ev) {
	char* msg;
	asprintf(&msg, "You placed a %d block at %d %d %d", ev->blockId, ev->blockPos.x, ev->blockPos.y, ev->blockPos.z);
	api->player.sendMessage(ev->player, msg);
	free(msg);

	ev->cancel = true;
}

void OnItemUse(const bp_api* api, bp_item_use_event* ev) {
	char* msg;
	asprintf(&msg, "You used item (%d:%d)x%d", ev->item.id, ev->item.data, ev->item.count);
	api->player.sendMessage(ev->player, msg);
	free(msg);

	if (ev->item.id == 320) {
		api->player.sendMessage(ev->player, "Eat something else!");
		ev->cancel = true;
	}
}

void OnPlayerChat(const bp_api* api, bp_player_chat_event* ev) {
	if (strstr(ev->message, "hate") != NULL) {
		ev->cancel = true;
		api->player.kick(api, ev->player, "You can't use the bad word 'hate'!");
		return;
	}
	if (strstr(ev->message, "players") != NULL) {
		ev->cancel = true;
		(void)api->commands.execute(api, ev->player, "list");
	}
}

void OnEntityDamage(const bp_api* api, bp_entity_damage_event* ev) {
	char* msg;
	asprintf(&msg, "An entity got %d damage!", ev->amount);

	int playerCount = api->server.getPlayerCount(api);
	for (int i = 0; i < playerCount; ++i) {
		bp_player* player = api->server.getPlayerAt(api, i);
		api->player.sendMessage(player, msg);
	}

	free(msg);

	ev->cancel = true;
}

void OnPlayerMove(const bp_api* api, bp_player_move_event* ev) {
	static int counter = 0;

	char* msg;
	asprintf(&msg, "Moved from %.2f %.2f %.2f to %.2f %.2f %.2f", ev->from.x, ev->from.y, ev->from.z, ev->to.x,
	         ev->to.y, ev->to.z);

	api->player.sendMessage(ev->player, msg);

	free(msg);

	if (counter++ > 50) {
		ev->from.y *= 5; // do a funny
		ev->to.y *= 5; // do a funny
		counter = 0;
	}
}

void LoadSave(const bp_api* api) {
	FILE* file = fopen("keys.txt", "r");

	if (file == NULL) {
		api->log.info(api, "Couldn't open addon save file! Ignoring...");
		return;
	}

	keyCount = 0;

	int x, y, z;
	while (fscanf(file, "%d %d %d", &x, &y, &z) == 3) {
		keys[keyCount++].doorPos = (bp_block_pos){ x, y, z };
	}

	fclose(file);
}

void WriteSave(const bp_api* api) {
	FILE* file = fopen("keys.txt", "w");

	if (file == NULL) {
		api->log.error(api, "Failed to create addon save file!");
		return;
	}

	for (int i = 0; i < keyCount; ++i) {
		bp_block_pos pos = keys[i].doorPos;
		fprintf(file, "%d %d %d\n", pos.x, pos.y, pos.z);
	}

	fclose(file);
}

void OnLoad(const bp_api* api, const bp_addon_load* ev) {
	LoadSave(api);
	api->log.info(api, "Test addon loaded!");
}

void OnUnload(const bp_api* api, const bp_addon_unload* ev) {
	api->log.info(api, "Test addon shutting down...");
	WriteSave(api);
}

bp_addon_info bp_addon(const bp_api* api) {
	return (bp_addon_info){
		.id = "test",
		.name = "Test",
		.version = "1.0",
		.events = {
			.playerJoin = OnPlayerJoin,
			.playerChat = OnPlayerChat,
			.playerMove = OnPlayerMove,
			.itemUse = OnItemUse,
			.blockBreak = OnBlockBreak,
			.blockPlace = OnBlockPlace,
			.blockUse = OnBlockUse,
			.entityDamage = OnEntityDamage,
			.addonLoad = OnLoad,
			.addonUnload = OnUnload,
		},
	};
}