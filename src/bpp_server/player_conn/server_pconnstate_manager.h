/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once

class Server;
// For managing the player's connection state
struct PlayerConnStateManager {
	void HandleConnectionState(PlayerSession& _session, Server& _server);
	void HandleHandshake(PlayerSession& _session, Server& _server);
	void HandleLogin(PlayerSession& _session, Server& _server);
	void HandleVerifyingUsername(PlayerSession& _session, Server& _server);
	void WaitForSpawnChunks(PlayerSession& _session, Server& _server);
	void DisconnectPlayer(PlayerSession& _session, const std::string& _reason, Server& _server, bool doSave = true);

private:
	void FinishLogin(PlayerSession& _session, Server& _server);
};