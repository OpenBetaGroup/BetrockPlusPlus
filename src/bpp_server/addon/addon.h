/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once

#include "addon_impl.h"

class Server;

struct Addon {
	bp_addon_info info;
	bp_api api;
	void* dynHandle;
	Server* server;
	//TODO: Currently we assume all command names are unique, but that isn't always the case
	// The ideal way would be to prefix addon commands as a direct fallback
	// So if both addon A and B register /heal
	// These would be on the command tree:
	// - /heal (Wheoever registered it first, this includes built-in names too)
	// - /addona:heal
	// - /addonb:heal
	std::vector<std::string> registeredCommands;
};