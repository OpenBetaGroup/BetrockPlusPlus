/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

// Small filesystem helpers for the places where "just call std::filesystem"
// isn't portable. Everything here is a plain forward to std::filesystem on
// desktop; the fallbacks only run if the first attempt fails.

#pragma once

#include <filesystem>
#include <fstream>
#include <system_error>

namespace Platform::FS {

// Copies _from over _to. Falls back to a plain stream copy because
// std::filesystem::copy_file needs OS primitives that some libcs don't have.
inline bool CopyFileOverwrite(const std::filesystem::path& _from, const std::filesystem::path& _to) {
	std::error_code ec;
	if (std::filesystem::copy_file(_from, _to, std::filesystem::copy_options::overwrite_existing, ec) && !ec)
		return true;

	std::ifstream in(_from, std::ios::binary);
	std::ofstream out(_to, std::ios::binary | std::ios::trunc);
	if (!in || !out)
		return false;
	// Inserting an empty streambuf would set failbit on _to
	if (in.peek() != std::ifstream::traits_type::eof())
		out << in.rdbuf();
	out.flush();
	return out.good();
}

// Moves _from onto _to, replacing it if it already exists.
// POSIX does this atomically, but FAT/exFAT (SD cards) refuses to rename onto
// an existing file, so on failure we remove the target and try once more.
inline bool ReplaceFile(const std::filesystem::path& _from, const std::filesystem::path& _to) {
	std::error_code ec;
	std::filesystem::rename(_from, _to, ec);
	if (!ec)
		return true;

	std::error_code removeEc;
	std::filesystem::remove(_to, removeEc);
	ec.clear();
	std::filesystem::rename(_from, _to, ec);
	return !ec;
}

} // namespace Platform::FS
