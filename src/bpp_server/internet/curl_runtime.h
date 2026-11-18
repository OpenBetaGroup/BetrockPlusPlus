/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#if defined(ONLINE_MODE_AUTHENTICATION) || defined(BETACRAFT_HEARTBEAT)
#pragma once
#include <curl/curl.h>
#include <memory>
#include <string>
namespace Curl {
bool Init();
void Cleanup();
void RestoreStopSignals();

struct CurlDeleter {
    void operator()(CURL* _curl) const {
        curl_easy_cleanup(_curl);
    }
};

using UniqueCurl = std::unique_ptr<CURL, CurlDeleter>;

inline size_t DefaultWriteCallback(char* _ptr, size_t _size, size_t _nmemb, void* _userdata) {
	auto* out = static_cast<std::string*>(_userdata);
	out->append(_ptr, _size * _nmemb);
	return _size * _nmemb;
}
}
#endif
