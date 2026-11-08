/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "server.h"
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>
#include <system_error>

namespace {

bool ParsePositiveInt(const std::string& _value, int& _result) {
	if (_value.empty())
		return false;

	const auto parsed = std::from_chars(_value.data(), _value.data() + _value.size(), _result);
	return parsed.ec == std::errc{} && parsed.ptr == _value.data() + _value.size() && _result > 0;
}

bool ParseDouble(const std::string& _value, double& _result) {
	if (_value.empty() || std::isspace(static_cast<unsigned char>(_value.front())))
		return false;

	errno = 0;
	char* end = nullptr;
	const double parsed = std::strtod(_value.c_str(), &end);
	if (errno == ERANGE || end != _value.c_str() + _value.size() || !std::isfinite(parsed))
		return false;

	_result = parsed;
	return true;
}

std::string RequestSample(void* _userData, Server::SampleMode _mode, int _ticks) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.server->BeginTickSample(_mode, _ticks, ctx.sender))
		return "A tick sample is already pending or active.";

	SendFeedback(ctx, std::format("§7Tick sample started; report in {} ticks.", _ticks));
	return {};
}

std::string Health(const strategos::CmdNode&, void* _userData) {
	return RequestSample(_userData, Server::SampleMode::Health, 100);
}

std::string HealthTicks(const strategos::CmdNode& _node, void* _userData) {
	auto ticks = _node.get_arg<std::string>("ticks");
	int count = 0;

	if (!ticks || !ParsePositiveInt(*ticks, count))
		return ERROR_REASON_PARAMETERS;

	return RequestSample(_userData, Server::SampleMode::Health, count);
}

std::string Entities(const strategos::CmdNode&, void* _userData) {
	return RequestSample(_userData, Server::SampleMode::Entities, 100);
}

std::string EntitiesTicks(const strategos::CmdNode& _node, void* _userData) {
	auto ticks = _node.get_arg<std::string>("ticks");
	int count = 0;

	if (!ticks || !ParsePositiveInt(*ticks, count))
		return ERROR_REASON_PARAMETERS;

	return RequestSample(_userData, Server::SampleMode::Entities, count);
}

std::string Freeze(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const bool frozen = ctx.server->ToggleTickFreeze();

	SendFeedback(ctx, frozen ? "§eTick simulation frozen." : "§eTick simulation resumed.");
	return {};
}

std::string StepCount(int _count, void* _userData);

std::string Step(const strategos::CmdNode&, void* _userData) {
	return StepCount(1, _userData);
}

std::string StepTicks(const strategos::CmdNode& _node, void* _userData) {
	auto ticks = _node.get_arg<std::string>("ticks");
	int count = 0;

	if (!ticks || !ParsePositiveInt(*ticks, count))
		return ERROR_REASON_PARAMETERS;

	return StepCount(count, _userData);
}

std::string StepCount(int _count, void* _userData) {
	auto& ctx = CmdCtx(_userData);

	if (!ctx.server->IsTickFrozen())
		return "Tick stepping requires freeze.";

	ctx.server->QueueTickSteps(_count);
	SendFeedback(ctx, std::format("§eQueued {} simulation tick(s).", _count));
	return {};
}

std::string Rate(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);

	SendFeedback(ctx, std::format("§eTarget tick rate: {:.2f} TPS.", ctx.server->GetTickRate()));
	return {};
}

std::string SetRate(const strategos::CmdNode& _node, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto token = _node.get_arg<std::string>("tps");

	if (!token || token->empty())
		return ERROR_REASON_PARAMETERS;

	double rate = 0.0;
	if (!ParseDouble(*token, rate) || rate <= 0.0)
		return ERROR_REASON_PARAMETERS;
	ctx.server->SetTickRate(rate);
	if (ctx.server->GetTickRate() != rate)
		return ERROR_REASON_PARAMETERS;

	SendFeedback(ctx, std::format("§eTarget tick rate set to {:.2f} TPS.", rate));
	return {};
}

} // namespace

void RegisterTick(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("tick")
	                            .describe("Controls simulation ticks and reports tick performance")
	                            .op()
	                            .then(strategos::Node::literal("health").executes(Health).then(
	                                strategos::Node::string("ticks").executes(HealthTicks)))
	                            .then(strategos::Node::literal("entities")
	                                      .executes(Entities)
	                                      .then(strategos::Node::string("ticks").executes(EntitiesTicks)))
	                            .then(strategos::Node::literal("freeze").executes(Freeze))
	                            .then(strategos::Node::literal("step").executes(Step).then(
	                                strategos::Node::string("ticks").executes(StepTicks)))
	                            .then(strategos::Node::literal("rate").executes(Rate).then(
	                                strategos::Node::string("tps").executes(SetRate))));
}
