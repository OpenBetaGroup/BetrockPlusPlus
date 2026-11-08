/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "betacraft_heartbeat.h"

#ifdef BETACRAFT_HEARTBEAT
#include "curl_runtime.h"
#include "logger.h"
#include "version.h"
#include <chrono>
#include <curl/curl.h>
#include <curl/easy.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

extern std::atomic<bool> shutdownRequested;

namespace {
constexpr const char* kApiHost = "https://api.betacraft.uk/v2";
constexpr int kPingIntervalSeconds = 60;
constexpr size_t kMaxNameLen = 64;
constexpr size_t kMaxDescriptionLen = 256;
constexpr size_t kMaxIconBytes = 64000;

using json = nlohmann::json;

size_t WriteCallback(char* _ptr, size_t _size, size_t _nmemb, void* _userdata) {
	auto* out = static_cast<std::string*>(_userdata);
	out->append(_ptr, _size * _nmemb);
	return _size * _nmemb;
}

std::string Truncate(std::string _value, size_t _max) {
	if (_value.size() > _max)
		_value.resize(_max);
	return _value;
}

std::string Base64Encode(const std::string& _data) {
	static constexpr char kTable[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	out.reserve(((_data.size() + 2) / 3) * 4);
	size_t i = 0;
	while (i + 2 < _data.size()) {
		const unsigned int n = (static_cast<unsigned char>(_data[i]) << 16) |
		                       (static_cast<unsigned char>(_data[i + 1]) << 8) |
		                       static_cast<unsigned char>(_data[i + 2]);
		out.push_back(kTable[(n >> 18) & 63]);
		out.push_back(kTable[(n >> 12) & 63]);
		out.push_back(kTable[(n >> 6) & 63]);
		out.push_back(kTable[n & 63]);
		i += 3;
	}
	if (i < _data.size()) {
		unsigned int n = static_cast<unsigned char>(_data[i]) << 16;
		if (i + 1 < _data.size())
			n |= static_cast<unsigned char>(_data[i + 1]) << 8;
		out.push_back(kTable[(n >> 18) & 63]);
		out.push_back(kTable[(n >> 12) & 63]);
		if (i + 1 < _data.size()) {
			out.push_back(kTable[(n >> 6) & 63]);
			out.push_back('=');
		} else {
			out += "==";
		}
	}
	return out;
}

bool ResponseOk(const std::string& _body, std::string& _message) {
	try {
		const json parsed = json::parse(_body);
		_message = parsed.value("message", "");
		return !parsed.value("error", false);
	} catch (const json::parse_error&) {
		_message = _body;
		return false;
	}
}

} // namespace

void BetacraftHeartbeat::Load(Config& _config, int _serverPort) {
	enabled = _config.GetAsBoolean("betacraft-heartbeat", false);
	if (!enabled)
		return;

	name = Truncate(_config.GetAsString("betacraft-name", "A Betrock++ server"), kMaxNameLen);
	description = Truncate(_config.GetAsString("betacraft-description", ""), kMaxDescriptionLen);
	privateKey = _config.GetAsString("betacraft-private-key", "");
	category = _config.GetAsString("betacraft-category", "beta");
	gameVersion = _config.GetAsString("betacraft-game-version", "b1.7.3");
	protocol = _config.GetAsString("betacraft-protocol", "beta_14");
	v1Version = _config.GetAsString("betacraft-v1-version", "b1.7.3");
	sendPlayers = _config.GetAsBoolean("betacraft-send-players", true);
	iconPath = _config.GetAsString("betacraft-icon", "");
	maxPlayers = _config.GetAsNumber<int>("max-players", 20);
	if (maxPlayers < 0)
		maxPlayers = 20;
	serverPort = _serverPort;
	socket = _config.GetAsString("betacraft-socket", "");
}

void BetacraftHeartbeat::Start() {
	if (!enabled || running.exchange(true))
		return;

	if (socket.empty()) {
		const std::string ip = FetchPublicIp();
		if (ip.empty()) {
			Logger::Error << "Betacraft heartbeat is enabled but betacraft-socket is empty and public IP "
			                 "lookup failed. Set betacraft-socket to host:port.\n";
			running.store(false);
			enabled = false;
			return;
		}
		socket = ip + ":" + std::to_string(serverPort);
		Logger::Info << "Betacraft heartbeat socket auto-detected as " << socket << "\n";
	}

	if (privateKey.empty()) {
		Logger::Warning << "Betacraft heartbeat has no private key. Contact Moresteck on the Betacraft Discord "
		                   "to verify this address, then set betacraft-private-key.\n";
	}

	worker = std::thread([this]() { ThreadMain(); });
}

BetacraftHeartbeat::~BetacraftHeartbeat() {
	Stop();
}

void BetacraftHeartbeat::Stop() {
	if (!running.exchange(false))
		return;
	if (worker.joinable())
		worker.join();
}

void BetacraftHeartbeat::UpdateSnapshot(const BetacraftHeartbeatSnapshot& _snapshot) {
	std::lock_guard lock(snapshotMutex);
	snapshot = _snapshot;
}

std::string BetacraftHeartbeat::FetchPublicIp() {
	CURL* curl = curl_easy_init();
	if (!curl)
		return {};
	std::string response;
	curl_easy_setopt(curl, CURLOPT_URL, "http://checkip.amazonaws.com");
	curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Betrock++");
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	const CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	CurlRestoreStopSignals();
	if (res != CURLE_OK)
		return {};
	while (!response.empty() && (response.back() == '\n' || response.back() == '\r' || response.back() == ' '))
		response.pop_back();
	return response;
}

bool BetacraftHeartbeat::PostJson(const std::string& _url, const std::string& _json, std::string& _response) {
	CURL* curl = curl_easy_init();
	if (!curl) {
		Logger::Error << "Failed to initialize libcurl for Betacraft heartbeat.\n";
		return false;
	}

	curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");

	curl_easy_setopt(curl, CURLOPT_URL, _url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
	curl_easy_setopt(curl, CURLOPT_POST, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, _json.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(_json.size()));
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Betrock++");
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &_response);

	const CURLcode res = curl_easy_perform(curl);
	long httpCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	CurlRestoreStopSignals();

	if (res != CURLE_OK) {
		Logger::Warning << "Betacraft heartbeat request failed: " << curl_easy_strerror(res) << "\n";
		return false;
	}
	if (httpCode >= 400) {
		Logger::Warning << "Betacraft heartbeat HTTP " << httpCode << ": " << _response << "\n";
		return false;
	}
	return true;
}

std::string BetacraftHeartbeat::BuildUpdatePayload(const BetacraftHeartbeatSnapshot& _snapshot) const {
	json payload;
	payload["game_version"] = gameVersion;
	payload["protocol"] = protocol;
	payload["name"] = name;
	payload["description"] = description;
	payload["private_key"] = privateKey;
	payload["socket"] = socket;
	payload["category"] = category;
	payload["v1_version"] = v1Version;
	payload["send_players"] = sendPlayers;
	payload["max_players"] = _snapshot.maxPlayers;
	payload["online_players"] = _snapshot.onlinePlayers;
	payload["online_mode"] = _snapshot.onlineMode;
	payload["software"] = { { "name", std::string(PROJECT_NAME) },
		                    { "version", std::string(PROJECT_VERSION_FULL_STRING) } };

	json players = json::array();
	if (sendPlayers) {
		for (const auto& username : _snapshot.playerNames)
			players.push_back({ { "username", username } });
	}
	payload["players"] = std::move(players);
	return payload.dump();
}

bool BetacraftHeartbeat::SendUpdate(const BetacraftHeartbeatSnapshot& _snapshot) {
	std::string response;
	const std::string url = std::string(kApiHost) + "/server_update";
	if (!PostJson(url, BuildUpdatePayload(_snapshot), response))
		return false;

	std::string message;
	if (!ResponseOk(response, message)) {
		Logger::Warning << "Betacraft heartbeat rejected: " << (message.empty() ? response : message) << "\n";
		return false;
	}
	return true;
}

bool BetacraftHeartbeat::SendIcon() {
	if (iconPath.empty())
		return true;

	std::ifstream file(iconPath, std::ios::binary);
	if (!file) {
		Logger::Warning << "Betacraft server icon not found: " << iconPath << "\n";
		return false;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	const std::string bytes = buffer.str();
	if (bytes.size() > kMaxIconBytes) {
		Logger::Error << "Betacraft server icon is too large (64 KiB max, recommended 128x128 PNG).\n";
		return false;
	}

	json payload;
	payload["socket"] = socket;
	payload["private_key"] = privateKey;
	payload["icon"] = Base64Encode(bytes);

	std::string response;
	const std::string url = std::string(kApiHost) + "/server_update_icon";
	if (!PostJson(url, payload.dump(), response))
		return false;

	std::string message;
	if (!ResponseOk(response, message)) {
		Logger::Warning << "Betacraft icon update failed: " << (message.empty() ? response : message) << "\n";
		return false;
	}
	Logger::Info << "Betacraft server icon updated.\n";
	return true;
}

void BetacraftHeartbeat::ThreadMain() {
	int failsInARow = -1;
	while (running.load() && !shutdownRequested.load()) {
		BetacraftHeartbeatSnapshot local;
		{
			std::lock_guard lock(snapshotMutex);
			local = snapshot;
			if (local.maxPlayers <= 0)
				local.maxPlayers = maxPlayers;
		}

		if (SendUpdate(local)) {
			if (failsInARow != 0)
				Logger::Info << "Betacraft server list ping succeeded (" << socket << ").\n";
			failsInARow = 0;
			if (!iconSent) {
				iconSent = true;
				SendIcon();
			}
		} else {
			++failsInARow;
			if (failsInARow <= 5)
				Logger::Warning << "Failed to ping the Betacraft server list. Check betacraft-* settings in "
				                   "server.properties.\n";
		}

		for (int i = 0; i < kPingIntervalSeconds * 10 && running.load() && !shutdownRequested.load(); ++i)
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}
#endif
