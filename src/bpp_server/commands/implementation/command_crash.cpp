/*
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "server.h"
#include <csignal>

namespace {
std::string CrashServer(const strategos::CmdNode& /*_cmd*/, void* /*_userData*/) {
	std::raise(SIGSEGV);
	return "hm... this didn't crash";
}
} // namespace

void RegisterCrash(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(
	    strategos::Node::literal("crash").describe("Crashes the server with a seg-fault").executes(CrashServer));
}