#define ADDON_API_IMPLEMENTATION
#include "../include/addon_api.h"
#include <stdio.h>
#include <stdlib.h>

void GetNote(const bp_api* api, const bp_cmd_action_ctx* ctx) {
	int index;
	if (!api->cmd.getIntegerArg(ctx, "index", &index)) return;

	char* msg;
	asprintf(&msg, "Getting Note #%d!", index);
	api->cmd.sendActionFeedback(ctx, msg);
	free(msg);
}

void SetNote(const bp_api* api, const bp_cmd_action_ctx* ctx) {
	int index;
	if (!api->cmd.getIntegerArg(ctx, "index", &index)) return;
	const char* value;
	if (!api->cmd.getStringArg(ctx, "value", &value)) return;

	char* msg;
	asprintf(&msg, "Setting Note #%d as '%s'!", index, value);
	api->cmd.sendActionFeedback(ctx, msg);
	free(msg);
}

void OnLoad(const bp_api* api, const bp_addon_load* ev) {
	// Create all the nodes we need for our command tree
	bp_cmd_node* note = api->cmd.makeNode("note", BP_CMD_LITERAL);
	bp_cmd_node* get = api->cmd.makeNode("get", BP_CMD_LITERAL);
	bp_cmd_node* get_index = api->cmd.makeNode("index", BP_CMD_INTEGER);
	bp_cmd_node* set = api->cmd.makeNode("set", BP_CMD_LITERAL);
	bp_cmd_node* set_index = api->cmd.makeNode("index", BP_CMD_INTEGER);
	bp_cmd_node* set_value = api->cmd.makeNode("value", BP_CMD_GREEDY_STRING);

	// Wire up our nodes to form the tree
	api->cmd.addChild(note, get);
	api->cmd.addChild(note, set);

	api->cmd.addChild(get, get_index);

	api->cmd.addChild(set, set_index);
	api->cmd.addChild(set_index, set_value);

	// Finish off the tree by putting executors at each end
	api->cmd.setAction(api, get_index, GetNote);
	api->cmd.setAction(api, set_value, SetNote);

	// And finally register our root nodes as commands
	api->cmd.registerCommand(api, get);
	api->cmd.registerCommand(api, set);

	api->log.info(api, "Command test addon loaded!");
}

void OnUnload(const bp_api* api, const bp_addon_unload* ev) {
	api->log.info(api, "Command test addon shutting down...");
}

bp_addon_info bp_addon(const bp_api* api) {
	return (bp_addon_info){
		.id = "command-test",
		.name = "Command Test",
		.version = "1.0",
		.events = {
			.addonLoad = OnLoad,
			.addonUnload = OnUnload,
		},
	};
}