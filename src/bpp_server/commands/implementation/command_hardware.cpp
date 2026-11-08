/*
 * Copyright (c) 2026, Betrock++ contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only
*/

#include "../../../bpp_shared/platform/hardware_info.h"
#include "../command.h"
#include "../command_manager.h"
#include "../command_registry.h"
#include <format>

namespace {

std::string FormatBytes(uint64_t _bytes) {
	constexpr double MIB = 1024.0 * 1024.0;
	const double mib = static_cast<double>(_bytes) / MIB;
	if (mib >= 1024.0)
		return std::format("{:.2f} GB", mib / 1024.0);
	return std::format("{:.0f} MB", mib);
}

std::string FormatCpu(const HardwareInfo& _info) {
	std::string line = _info.cpuModel.empty() ? "Unknown" : _info.cpuModel;
	std::string details;
	if (_info.threads > 0)
		details += std::format("({})", _info.threads);
	if (_info.cpuMhz > 0.0) {
		if (!details.empty())
			details += " @ ";
		details += _info.cpuMhz >= 1000.0 ? std::format("{:.2f} GHz", _info.cpuMhz / 1000.0)
		                                  : std::format("{:.0f} MHz", _info.cpuMhz);
	}
	if (!details.empty())
		line += std::format(" {}", details);
	return line;
}

std::string ShowHardware(const strategos::CmdNode&, void* _userData) {
	auto& ctx = CmdCtx(_userData);
	const HardwareInfo info = QueryHardwareInfo();
	SendFeedback(ctx, std::format("§6OS: §f{}", info.os.empty() ? "Unknown" : info.os));
	SendFeedback(ctx, std::format("§6Kernel: §f{}", info.kernel.empty() ? "Unknown" : info.kernel));
	SendFeedback(ctx, std::format("§6CPU: §f{}", FormatCpu(info)));
	SendFeedback(ctx,
	             std::format("§6Memory: §f{}", info.memoryTotalBytes ? FormatBytes(info.memoryTotalBytes) : "Unknown"));
	SendFeedback(ctx, std::format("§6Swap: §f{}", info.swapTotalBytes ? FormatBytes(info.swapTotalBytes) : "None"));
	return "";
}

} // namespace

void RegisterHardware(strategos::BrigadierContext& _dispatcher) {
	_dispatcher.add_command(strategos::Node::literal("hardware")
	                            .describe("Shows the hardware the server is running on")
	                            .op()
	                            .executes(ShowHardware));
}
