/*
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */
#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include "commands/strategos.h"
#include "server.h"
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <format>
#include <optional>
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

void RequestSample(void* _userData, Server::SampleMode _mode, int _ticks) {
	auto& ctx = CmdCtx(_userData);
	if (!ctx.server->BeginTickSample(_mode, _ticks, ctx.sender)) {
		SendErrorFeedback(ctx, "A tick sample is already pending or active.");
		return;
	}

	SendFeedback(ctx, std::format("§7Tick sample started; report in {} ticks.", _ticks));
}

void Health(const strategos::CmdNode&, void* _userData) {
	RequestSample(_userData, Server::SampleMode::Health, 100);
}

void HealthTicks(const strategos::CmdNode& _node, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto ticks = _node.get_arg<std::string>("ticks");
	int count = 0;

	if (!ticks || !ParsePositiveInt(*ticks, count)) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}

	RequestSample(_userData, Server::SampleMode::Health, count);
}

void Entities(const strategos::CmdNode&, void* _userData) {
	RequestSample(_userData, Server::SampleMode::Entities, 100);
}

void EntitiesTicks(const strategos::CmdNode& _node, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto ticks = _node.get_arg<std::string>("ticks");
	int count = 0;

	if (!ticks || !ParsePositiveInt(*ticks, count)) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}

	RequestSample(_userData, Server::SampleMode::Entities, count);
}

void Freeze(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const bool frozen = ctx.server->ToggleTickFreeze();

	SendFeedback(ctx, frozen ? "§eTick simulation frozen." : "§eTick simulation resumed.");
}

void StepCount(int _count, void* _userData);

void Step(const strategos::CmdNode&, void* _userData) {
	StepCount(1, _userData);
}

void StepTicks(const strategos::CmdNode& _node, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto ticks = _node.get_arg<std::string>("ticks");
	int count = 0;

	if (!ticks || !ParsePositiveInt(*ticks, count)) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}

	StepCount(count, _userData);
}

void StepCount(int _count, void* _userData) {
	auto& ctx = CmdCtx(_userData);

	if (!ctx.server->IsTickFrozen()) {
		SendErrorFeedback(ctx, "Tick stepping requires freeze.");
		return;
	}

	ctx.server->QueueTickSteps(_count);
	SendFeedback(ctx, std::format("§eQueued {} simulation tick(s).", _count));
}

void Rate(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);

	SendFeedback(ctx, std::format("§eTarget tick rate: {:.2f} TPS.", ctx.server->GetTickRate()));
}

void SetRate(const strategos::CmdNode& _node, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	auto token = _node.get_arg<std::string>("tps");

	if (!token || token->empty()) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}

	double rate = 0.0;
	if (!ParseDouble(*token, rate) || rate <= 0.0) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}
	ctx.server->SetTickRate(rate);
	if (ctx.server->GetTickRate() != rate) {
		SendErrorFeedback(ctx, ERROR_REASON_PARAMETERS);
		return;
	}

	SendFeedback(ctx, std::format("§eTarget tick rate set to {:.2f} TPS.", rate));
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
