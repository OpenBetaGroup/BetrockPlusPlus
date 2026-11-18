/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#ifdef BETACRAFT_HEARTBEAT
#pragma once
#include "config/config.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct BetacraftHeartbeatSnapshot {
	int onlinePlayers = 0;
	int maxPlayers = 20;
	bool onlineMode = false;
	std::vector<std::string> playerNames;
};

// Pings https://api.betacraft.uk/v2 so the server can appear on the Betacraft list.
class BetacraftHeartbeat {
public:
	void Load(Config& _config, int _serverPort);
	void Start();
	void Stop();
	BetacraftHeartbeat() = default;
	BetacraftHeartbeat(const BetacraftHeartbeat&) = delete;
	BetacraftHeartbeat& operator=(const BetacraftHeartbeat&) = delete;
	~BetacraftHeartbeat();
	void UpdateSnapshot(const BetacraftHeartbeatSnapshot& _snapshot);
	bool Enabled() const {
		return enabled;
	}

private:
	void ThreadMain();
	bool PostJson(const std::string& _url, const std::string& _json, std::string& _response);
	bool SendUpdate(const BetacraftHeartbeatSnapshot& _snapshot);
	bool SendIcon();
	std::string BuildUpdatePayload(const BetacraftHeartbeatSnapshot& _snapshot) const;
	static std::string FetchPublicIp();

	bool enabled = false;
	bool sendPlayers = true;
	bool iconSent = false;
	int maxPlayers = 20;
	std::string name;
	std::string description;
	std::string socket;
	std::string privateKey;
	std::string category;
	std::string gameVersion;
	std::string protocol;
	std::string v1Version;
	std::string iconPath;
	int serverPort = 25565;

	std::mutex snapshotMutex;
	BetacraftHeartbeatSnapshot snapshot;

	std::atomic<bool> running{ false };
	std::thread worker;
};
#endif
