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
};