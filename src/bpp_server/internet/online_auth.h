/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#ifdef ONLINE_MODE_AUTHENTICATION
#pragma once
#include "../bpp_shared/helpers/java/java_random.h"
#include "BS_thread_pool.hpp"
#include "logger.h"
#include <curl/curl.h>
#include <curl/easy.h>
#include <format>
#include <future>
#include <ios>
#include <sstream>
#include <string>

struct Authentication {
	bool onlineMode = false;
	std::string proxy = "http://betacraft.ee";
	uint16_t proxyPort = 11705;
	const std::string baseUrl = "http://www.minecraft.net/game";
	std::string GenerateAuthHash();
	bool IsRegisteredUsername(std::string _serverId, std::string _username);
	std::future<bool> IsRegisteredUsernameAsync(std::string _serverId, std::string _username);

private:
	BS::thread_pool<> pool{ 2 };
};
#endif