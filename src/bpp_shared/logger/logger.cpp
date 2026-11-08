/*
 * Copyright (c) 2025-2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Anya Rihtarshich <vesui@proton.me>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#include "logger.h"
#include "strings/ucs2.h"
#include "style.h"
#include <filesystem>

// Reference for Escape Codes
// https://gist.github.com/fnky/458719343aabd01cfb17a3a4f7296797

namespace Logger {

// For targets with small terminals
#if defined(__3DS__)
#define LOGGER_SHORT_TIME
#endif

#ifndef NDEBUG
LogLevel logLevelText = LOG_ALL;
#else
LogLevel logLevelText = LOG_ALL;
#endif

LogLevel logLevelTerminal = LOG_ALL;

// Streams
LogStream Message(LOG_MESSAGE);
LogStream Info(LOG_INFO);
LogStream Warning(LOG_WARNING);
LogStream Error(LOG_ERROR);
LogStream Debug(LOG_DEBUG);

namespace fs = std::filesystem;

std::ofstream logFile;
std::mutex terminalMutex;
std::function<void(const char*, int)> terminalSink;

std::string GetCurrentTimeString(bool _fileFormat = false) {
	auto now = std::chrono::system_clock::now();
	auto inTimeT = std::chrono::system_clock::to_time_t(now);

	std::stringstream ss;
	if (_fileFormat)
		ss << std::put_time(std::localtime(&inTimeT), "%Y-%m-%d-%H-%M-%S");
	else
#ifdef LOGGER_SHORT_TIME
		ss << std::put_time(std::localtime(&inTimeT), "%H:%M:%S");
#else
		ss << std::put_time(std::localtime(&inTimeT), "%Y-%m-%d %H:%M:%S");
#endif
	return ss.str();
}

void Init() {
	if (logLevelText == LOG_NONE)
		return;
	fs::path logDir = "logs";

	if (!fs::exists(logDir)) {
		fs::create_directories(logDir);
	}

	fs::path logFilePath = logDir / (GetCurrentTimeString(true) + ".log");

	logFile.open(logFilePath, std::ios::out | std::ios::app);

	// TODO: Make log file optional?
	// Maybe for systems without a disk,
	// or only limited/slow disk I/O?
	if (!logFile.is_open()) {
		throw std::runtime_error("Failed to open log file");
	}
}

// Manual flush.
// Best to run this at regular intervals,
// or for important messages, like warnings or errors
void Flush() {
	if (logFile.is_open())
		logFile.flush();
}

void Close() {
	Flush();
	if (!logFile.is_open())
		return;
	logFile.close();
}

void SetTerminalSink(std::function<void(const char*, int)> _sink) {
	std::lock_guard lock(terminalMutex);
	terminalSink = std::move(_sink);
}

void WriteTerminal(std::string _message, bool _toStderr = false) {
	std::lock_guard lock(terminalMutex);
	if (terminalSink) {
		terminalSink(_message.data(), static_cast<int>(_message.size()));
	} else if (_toStderr) {
		std::cerr << _message;
	} else {
		std::cout << _message;
	}
}

// Log a message without a header
void WriteMessage(std::string _message) {
	std::string time = GetCurrentTimeString();
	if (logLevelTerminal & LOG_MESSAGE) {
		WriteTerminal(time + " " + _message + "\n");
	}
	if (logLevelText & LOG_MESSAGE) {
		logFile << time << " " << _message << "\n";
	}
}

// Log a message with formatting codes
void WriteStyledMessage(const std::string& _message) {
	std::string time = GetCurrentTimeString();
	if (logLevelTerminal & LOG_MESSAGE) {
		WriteTerminal(time + " " + HandleFormattingCodes(_message) + "\n");
	}
	if (logLevelText & LOG_MESSAGE) {
		logFile << time << " " << StripFormatting(_message) << "\n";
	}
}

// Log a message with an INFO header
void WriteInfo(std::string _message) {
	std::string time = GetCurrentTimeString();
	std::string header = "[INFO]";
	if (logLevelTerminal & LOG_INFO) {
		WriteTerminal(time + " \x1b[1;30;107m" + header + "\x1b[0m " + _message + "\n");
	}
	if (logLevelText & LOG_INFO) {
		logFile << time << " " << header << " " << _message << "\n";
	}
}

// Log a warning
void WriteWarning(std::string _message) {
	std::string time = GetCurrentTimeString();
	std::string header = "[WARNING]";
	if (logLevelTerminal & LOG_WARNING) {
		WriteTerminal(time + " \x1b[1;30;43m" + header + "\x1b[0;33m " + _message + "\x1b[0m\n", true);
	}
	if (logLevelText & LOG_WARNING) {
		logFile << time << " " << header << " " << _message << "\n";
		Flush();
	}
}

// Log an error
void WriteError(std::string _message) {
	std::string time = GetCurrentTimeString();
	std::string header = "[ERROR]";
	if (logLevelTerminal & LOG_ERROR) {
		WriteTerminal(time + " \x1b[1;30;101m" + header + "\x1b[0;31m " + _message + "\x1b[0m\n", true);
	}
	if (logLevelText & LOG_ERROR) {
		logFile << time << " " << header << " " << _message << "\n";
		Flush();
	}
}

// Log Debug Data
void WriteDebug(std::string _message) {
	std::string time = GetCurrentTimeString();
	std::string header = "[DEBUG]";
	if (logLevelTerminal & LOG_DEBUG) {
		WriteTerminal(time + " \x1b[1;30;46m" + header + "\x1b[0m " + _message + "\n", true);
	}
	if (logLevelText & LOG_DEBUG) {
		logFile << time << " " << header << " " << _message << "\n";
	}
}

// Log a message with the passed Level
void Log(std::string _message, LogLevel _level) {
	switch (_level) {
	case LOG_INFO:
		WriteInfo(_message);
		break;
	case LOG_WARNING:
		WriteWarning(_message);
		break;
	case LOG_ERROR:
		WriteError(_message);
		break;
	case LOG_DEBUG:
		WriteDebug(_message);
		break;
	default:
		WriteMessage(_message);
		break;
	}
}

void LogStyled(const std::string& _message) {
	WriteStyledMessage(_message);
}
}; // namespace Logger
