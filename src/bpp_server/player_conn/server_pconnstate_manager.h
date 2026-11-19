/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "packet/packet_dispatcher.h"
#include "player_session_mp.h"
#include <string>

class Server;
// For managing the player's connection state
struct PlayerConnStateManager {
	void HandleConnectionState(PlayerSessionMP& _session, Server& _server);
	void HandleHandshake(PlayerSessionMP& _session, Server& _server);
	void HandleLogin(PlayerSessionMP& _session, Server& _server);
	void HandleVerifyingUsername(PlayerSessionMP& _session, Server& _server);
	void WaitForSpawnChunks(PlayerSessionMP& _session, Server& _server);
	void DisconnectPlayer(PlayerSessionMP& _session, const std::string& _reason, Server& _server, bool doSave = true);

private:
	void FinishLogin(PlayerSessionMP& _session, Server& _server);
};