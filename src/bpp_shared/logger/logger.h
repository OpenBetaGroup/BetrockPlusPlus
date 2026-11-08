/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once

#include "loglevel.h"
#include "logstream.h"
#include <fstream>
#include <functional>
#include <mutex>

namespace Logger {
// clang-format off
	extern LogStream Message;
	extern LogStream Info;
	extern LogStream Warning;
	extern LogStream Error;
	extern LogStream Debug;
// clang-format on
void Init();
void Close();
void Flush();

void Log(std::string _message, LogLevel _level = LOG_MESSAGE);
void LogStyled(const std::string& _message);
void SetTerminalSink(std::function<void(const char*, int)> _sink);
}; // namespace Logger
