/*
 * Copyright (c) 2026, jwaxy <jwaxy.is-a.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once

#include "addon.h"
#include "addon_api.h"
#include <memory>
#include <vector>

class Server;

class AddonManager {
public:
	AddonManager(Server* _server);
	~AddonManager();

	// Prevent copying by mistake, there should only be one real instance
	AddonManager(const AddonManager&) = delete;
	AddonManager& operator=(const AddonManager&) = delete;

	void Load();

	const std::vector<std::unique_ptr<Addon>>& GetAddons() const noexcept {
		return addons;
	}

	template <typename EventType>
	void Broadcast(void (*bp_addon_events::*_eventMember)(const bp_api*, EventType*), EventType& event) const {
		for (const auto& addon : addons) {
			auto callback = addon->info.events.*_eventMember;
			if (callback) {
				callback(&addon->api, &event);
			}
		}
	}

private:
	Server* server;
	// Only if C++ std had handle maps..., maybe we should use a library for that?
	std::vector<std::unique_ptr<Addon>> addons;
};
