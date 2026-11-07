/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// This is a pure C API. No C++ allowed!
// For easy binding purposes.

#ifndef ADDON_API_H
#define ADDON_API_H

#include <stdbool.h>
#include <stdint.h>

#define ADDON_API_VERSION 1

typedef struct bp_api bp_api;
typedef struct bp_player bp_player;
typedef struct bp_entity bp_entity;
typedef struct bp_world bp_world;

typedef struct {
	double x;
	double y;
	double z;
} bp_vec3;

typedef struct {
	int16_t id;
	int8_t count;
	int16_t data;
} bp_item_stack;

typedef struct {
	int32_t x;
	int32_t y;
	int32_t z;
} bp_block_pos;

typedef struct {
	int8_t id;
	uint8_t meta;
} bp_block;

struct bp_api {
	uint32_t version;
	void* internal; // Don't touch this.

	struct {
		void (*info)(const bp_api* api, const char* message);
		void (*warning)(const bp_api* api, const char* message);
		void (*error)(const bp_api* api, const char* message);
	} log;

	struct {
		int (*getPlayerCount)(const bp_api* api);
		// Used with getPlayerCount
		bp_player* (*getPlayerAt)(const bp_api* api, int index);
		void (*stop)(const bp_api* api, float timeout);
		double (*getMsptAverage)(const bp_api* api);
		double (*getTpsAverage)(const bp_api* api);
		const char* (*getVersion)();
	} server;

	struct {
		void (*sendMessage)(bp_player* player, const char* message);
		void (*kick)(const bp_api* api, bp_player* player, const char* reason);
		const char* (*getUsername)(bp_player* player);
		bp_entity* (*getEntity)(bp_player* player);
	} player;

	struct {
		bp_vec3 (*getPosition)(bp_entity* entity);
		void (*setPosition)(bp_entity* entity, bp_vec3 position);
		bp_world* (*getWorld)(bp_entity* entity);
	} entity;

	struct {
		void (*setBlock)(bp_world* world, bp_block_pos pos, bp_block block);
		bp_block (*getBlock)(bp_world* world, bp_block_pos pos);
		void (*sendBlockUpdate)(bp_world* world, bp_block_pos pos, bp_block block);
	} world;

	struct {
		void (*setPlayer)(const bp_api* api, bp_player* player, void* _data);
		void* (*getPlayer)(const bp_api* api, bp_player* player);
	} data;

	struct {
		bool (*execute)(const bp_api* api, bp_player* sender, const char* command);
	} commands;
};

typedef struct {
	bp_player* player;
} bp_player_join_event;

typedef struct {
	bp_player* player;
} bp_player_leave_event;

typedef struct {
	bp_player* player;
	const char* message;

	bool cancel;
} bp_player_chat_event;

typedef struct {
	bp_player* player;
	bp_vec3 from;
	bp_vec3 to;

	bool cancel;
} bp_player_move_event;

typedef struct {
	bp_player* player;

	bp_item_stack item;
	bool cancel;
} bp_item_use_event;

typedef struct {
	bp_player* player;
	bp_world* world;

	bp_item_stack heldItem;
	bp_block_pos blockPos;
	bp_block block;
	bool cancel;
} bp_block_use_event;

typedef struct {
	bp_player* player;
	bp_world* world;

	bp_item_stack tool;
	bp_block_pos blockPos;
	bp_block block;
	bool cancel;
} bp_block_break_event;

typedef struct {
	bp_player* player;
	bp_world* world;

	bp_block_pos blockPos;
	int8_t blockId;
	bool cancel;
} bp_block_place_event;

typedef struct {
	bp_entity* entity;

	int amount;
	bool cancel;
} bp_entity_damage_event;

typedef struct {
} bp_server_tick_event;

typedef struct {
	char* crash_log;
	char* crash_log_path;
	char* version_string;
} bp_crash_event;

typedef struct {
} bp_addon_load;

typedef struct {
} bp_addon_unload;

typedef void (*bp_player_join_fn)(const bp_api* api, const bp_player_join_event* event);
typedef void (*bp_player_leave_fn)(const bp_api* api, const bp_player_leave_event* event);
typedef void (*bp_player_chat_fn)(const bp_api* api, bp_player_chat_event* event);
typedef void (*bp_player_move_fn)(const bp_api* api, bp_player_move_event* event);
typedef void (*bp_item_use_fn)(const bp_api* api, bp_item_use_event* event);
typedef void (*bp_block_use_fn)(const bp_api* api, bp_block_use_event* event);
typedef void (*bp_block_break_fn)(const bp_api* api, bp_block_break_event* event);
typedef void (*bp_block_place_fn)(const bp_api* api, bp_block_place_event* event);
typedef void (*bp_entity_damage_fn)(const bp_api* api, bp_entity_damage_event* event);
typedef void (*bp_server_tick_fn)(const bp_api* api, const bp_server_tick_event* event);
typedef void (*bp_crash_event_fn)(const bp_api* api, const bp_crash_event* event);
typedef void (*bp_addon_load_fn)(const bp_api* api, const bp_addon_load* event);
typedef void (*bp_addon_unload_fn)(const bp_api* api, const bp_addon_unload* event);

typedef struct {
	bp_player_join_fn playerJoin;
	bp_player_leave_fn playerLeave;
	bp_player_chat_fn playerChat;
	bp_player_move_fn playerMove;
	bp_item_use_fn itemUse;
	bp_block_break_fn blockBreak;
	bp_block_place_fn blockPlace;
	bp_block_use_fn blockUse;
	bp_entity_damage_fn entityDamage;
	bp_server_tick_fn serverTick;
	bp_crash_event_fn crash;
	bp_addon_load_fn addonLoad;
	bp_addon_unload_fn addonUnload;
} bp_addon_events;

typedef struct {
	const char* id;
	const char* name;
	const char* version;

	bp_addon_events events;
} bp_addon_info;

/* Export this as "bp_addon" from your addon.
 * Note that you should never call any api functions in here!
 * Just return your addon's info, nothing else
 */
typedef bp_addon_info (*bp_addon_fn)(const bp_api* api);

// Standalone functions
bool bp_block_pos_equals(bp_block_pos a, bp_block_pos b);

#ifdef ADDON_API_IMPLEMENTATION
bool bp_block_pos_equals(bp_block_pos a, bp_block_pos b) {
	return a.x == b.x && a.y == b.y && a.z == b.z;
}
#endif

#endif