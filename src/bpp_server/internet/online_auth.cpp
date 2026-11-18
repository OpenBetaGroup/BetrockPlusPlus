/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#ifdef ONLINE_MODE_AUTHENTICATION
#include "online_auth.h"
#include "curl_runtime.h"

std::string Authentication::GenerateAuthHash() {
	// Source - https://stackoverflow.com/a/5100745
	// Posted by Kornel Kisielewicz, modified by community. See post 'Timeline' for change history
	// Retrieved 2026-08-13, License - CC BY-SA 4.0
	std::stringstream stream;
	stream << std::hex << Java::Random().NextLong();
	return stream.str();
}

bool Authentication::IsRegisteredUsername(std::string _serverId, std::string _username) {
	if (!onlineMode)
		return true;
	// TODO: Since we also do most of the same stuff for the UUID code,
	// it'd probably be smart to turn this into a generic "get endpoint data function" in curl runtime?
	static Curl::UniqueCurl curl;
    CURL* rawCurl = curl.get();
    // One-time init
	if (!curl) {
		curl.reset(curl_easy_init());
		rawCurl = curl.get();
		curl_easy_setopt(rawCurl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
		curl_easy_setopt(rawCurl, CURLOPT_HTTPGET, 1L);
		curl_easy_setopt(rawCurl, CURLOPT_FORBID_REUSE, 1L);
		curl_easy_setopt(rawCurl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(rawCurl, CURLOPT_MAXREDIRS, 5L);
		curl_easy_setopt(rawCurl, CURLOPT_USERAGENT, "Betrock++");
		// libcurl/OpenSSL otherwise install SIGALRM handlers and can block
		// SIGINT/SIGTERM in a threaded process (musl will then ignore Ctrl+C).
		curl_easy_setopt(rawCurl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(rawCurl, CURLOPT_CONNECTTIMEOUT, 5L);
		curl_easy_setopt(rawCurl, CURLOPT_TIMEOUT, 10L);
		curl_easy_setopt(rawCurl, CURLOPT_PROXY, proxy.c_str());
		curl_easy_setopt(rawCurl, CURLOPT_PROXYPORT, static_cast<long>(proxyPort));

	    curl_easy_setopt(rawCurl, CURLOPT_WRITEFUNCTION, Curl::DefaultWriteCallback);
	}
    // TODO: Don't like this double-grab :p
    if (!rawCurl)
        rawCurl = curl.get();
    // If we don't already have rawCurl here, shit's fucked
    if (!rawCurl)
        return false;

    // Set the URL
	std::string url = std::format("{}/checkserver.jsp?user={}&serverId={}", baseUrl, _username, _serverId);
	curl_easy_setopt(rawCurl, CURLOPT_URL, url.c_str());

    // Get the response
	std::string response;
	curl_easy_setopt(rawCurl, CURLOPT_WRITEDATA, &response);

    // Get the result
	CURLcode res = curl_easy_perform(rawCurl);
	Curl::RestoreStopSignals();

	if (res != CURLE_OK)
		return false;

	return response == "YES";
}

std::future<bool> Authentication::IsRegisteredUsernameAsync(std::string _serverId, std::string _username) {
	return pool.submit_task([this, serverId = std::move(_serverId), username = std::move(_username)]() {
		return IsRegisteredUsername(serverId, username);
	});
}
#endif