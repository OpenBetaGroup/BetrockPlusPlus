/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
#include "uuid.h"
#include <cstddef>
#include <curl/curl.h>
#include <curl/easy.h>
#include <memory>
#include <format>
#include "curl_runtime.h"

std::optional<std::string> Uuid::UsernameToUuid(const std::string& _username) {
	// TODO: Since we also do most of the same stuff for the online auth code,
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
        curl_easy_setopt(rawCurl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(rawCurl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(rawCurl, CURLOPT_TIMEOUT, 10L);
	    curl_easy_setopt(rawCurl, CURLOPT_WRITEFUNCTION, Curl::DefaultWriteCallback);
    }
    // TODO: Don't like this double-grab :p
    if (!rawCurl)
        rawCurl = curl.get();
    // If we don't already have rawCurl here, shit's fucked
    if (!rawCurl)
        return std::nullopt;

    // Set the URL
    std::string url = std::format("https://api.mojang.com/users/profiles/minecraft/{}", _username);
    curl_easy_setopt(rawCurl, CURLOPT_URL, url.c_str());

    // Get the response
	std::string response;
	curl_easy_setopt(rawCurl, CURLOPT_WRITEDATA, &response);

    // Get the result
	CURLcode res = curl_easy_perform(rawCurl);
	Curl::RestoreStopSignals();

	if (res != CURLE_OK)
		return std::nullopt;
    std::cout << response << std::endl;
	return "response";
}
#endif 