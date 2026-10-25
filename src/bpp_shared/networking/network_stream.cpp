/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/

#include "network_stream.h"
#include "logger.h"
#include "numeric_structs.h"
#include "packet_data.h"
#include "packet_labels.h"
#include "ucs2.h"
#include <string>
#include <vector>

NetworkStream::NetworkStream(int _pClientSocket) {
	clientSocket = _pClientSocket;
}

NetworkStream::~NetworkStream() {
	if (clientSocket != INVALID_SOCKET) {
#if defined(_WIN32) || defined(_WIN64)
		shutdown(clientSocket, SD_BOTH);
		closesocket(clientSocket);
		// TODO: Clean-up WSA when the server closes
		// WSACleanup();
#else
		shutdown(clientSocket, SHUT_RDWR);
		close(clientSocket);
#endif
		clientSocket = INVALID_SOCKET;
	}
}

void NetworkStream::FlushWriteBufferBlocking() {
	if (writeBuffer.empty() || clientSocket == INVALID_SOCKET)
		return;

	// Switch to blocking mode
#if defined(_WIN32) || defined(_WIN64)
	u_long mode = 0;
	ioctlsocket(clientSocket, FIONBIO, &mode);
#else
	int flags = fcntl(clientSocket, F_GETFL, 0);
	fcntl(clientSocket, F_SETFL, flags & ~O_NONBLOCK);
#endif

#ifdef BPP_HAS_SO_SNDTIMEO
#if defined(_WIN32) || defined(_WIN64)
	DWORD sndTimeoutMs = SHUTDOWN_FLUSH_TIMEOUT_MS;
	setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&sndTimeoutMs),
	           sizeof(sndTimeoutMs));
#else
	struct timeval sndTimeout{};
	sndTimeout.tv_sec = SHUTDOWN_FLUSH_TIMEOUT_MS / 1000;
	sndTimeout.tv_usec = (SHUTDOWN_FLUSH_TIMEOUT_MS % 1000) * 1000;
	setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, &sndTimeout, sizeof(sndTimeout));
#endif
#endif

	size_t sent = 0;
	while (sent < writeBuffer.size()) {
		int result = send(clientSocket, reinterpret_cast<const char*>(writeBuffer.data() + sent),
		                  static_cast<int>(writeBuffer.size() - sent), 0);
		if (result <= 0)
			break;
		sent += static_cast<size_t>(result);
	}
	writeBuffer.clear();
	ResetPacketCount();

	// We close here so the client can get the packet data we just sent out before we disconnect
#if defined(_WIN32) || defined(_WIN64)
	shutdown(clientSocket, SD_SEND);
	closesocket(clientSocket);
#else
	shutdown(clientSocket, SHUT_WR);
	close(clientSocket);
#endif
	clientSocket = INVALID_SOCKET;
}

void NetworkStream::IncrementPacketCount(PacketId /*_id*/) {
	packetsInQueue++;
	if (packetsInQueue > MAX_PACKETS_PER_TICK)
		return;
}

void NetworkStream::ResetPacketCount() {
	packetsInQueue = 0;
}

// String-8 Handling
void NetworkStream::WriteString8(const std::string& _str) {
	uint16_t length = static_cast<uint16_t>(_str.size());
	Write(length);
	std::vector<uint8_t> data(_str.begin(), _str.end());
	WriteBytes(data.data(), data.size());
}

std::string NetworkStream::ReadString8() {
	uint16_t len = Read<uint16_t>(); // adjust to uint32_t if your protocol uses 4-byte lengths
	std::string result(len, '\0');
	ReadBytes(reinterpret_cast<uint8_t*>(result.data()), len);
	return result;
}

// String-16 Handling
void NetworkStream::WriteString16(const std::string& _str) {
	std::u16string str16 = ToUCS2(_str);
	uint16_t length = static_cast<uint16_t>(str16.size());
	Write(length);
	std::vector<uint8_t> data(length * 2);
	for (size_t i = 0; i < str16.size(); i++) {
		data[i * 2] = (static_cast<uint8_t>((str16[i] >> 8) & 0xFF));
		data[i * 2 + 1] = (static_cast<uint8_t>(str16[i] & 0xFF));
	}
	WriteBytes(data.data(), data.size());
}

std::string NetworkStream::ReadString16() {
	uint16_t len = Read<uint16_t>();

	// Read as UTF-16 (2 bytes per char) regardless of platform wchar_t size
	std::vector<uint16_t> buf(len);
	ReadBytes(reinterpret_cast<uint8_t*>(buf.data()), len * sizeof(char16_t));

	std::u16string result(len, u'\0');
	for (uint16_t i = 0; i < len; i++) {
		// byteswap each UTF-16 unit, then widen to wchar_t
		result[i] = static_cast<char16_t>(FromBigEndian(buf[i]));
	}
	return ToUTF8(result);
}

size_t NetworkStream::ReadBytes(uint8_t* _buf, size_t _len) {
	size_t available = readBuffer.size() - readPos;

	if (available < _len) {
		// Not enough data has arrived yet. Copy what we have and flag it.
		if (available > 0)
			std::memcpy(_buf, readBuffer.data() + readPos, available);
		shortRead = true;
		return available;
	}

	std::memcpy(_buf, readBuffer.data() + readPos, _len);
	readPos += _len;
	return _len;
}

void NetworkStream::WriteBytes(const uint8_t* _buf, size_t _len) {
	// Session was kicked or closed, skip
	if (!connected) return;
	writeBuffer.insert(writeBuffer.end(), _buf, _buf + _len);
}

// Caps metadata fan-out so a malicious/broken stream cannot grow forever.
// Data is still read until 0x7F (or short-read / entry cap).
void NetworkStream::ReadEntityMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	uint8_t val = Read<uint8_t>();
	size_t entries = 0;
	while (!IsShortRead() && val != PacketData::EntityMetadata::END) {
		if (++entries > MAX_METADATA_ENTRIES) {
			connected = false;
			break;
		}
		// What type the data has
		PacketData::EntityMetadata::Type type = PacketData::EntityMetadata::Type(val >> 5);
		// Where the data goes for the relevant entity
		uint8_t index = uint8_t(val & 0x1F);
		switch (type) {
		case PacketData::EntityMetadata::Type::BYTE: {
			int8_t num = Read<int8_t>();
			_metadata.push_back(PacketData::EntityMetadata::DataEntry{ .type = type, .index = index, .value = num });
			break;
		}
		case PacketData::EntityMetadata::Type::SHORT: {
			int16_t num = Read<int16_t>();
			_metadata.push_back(PacketData::EntityMetadata::DataEntry{ .type = type, .index = index, .value = num });
			break;
		}
		case PacketData::EntityMetadata::Type::INTEGER: {
			int32_t num = Read<int32_t>();
			_metadata.push_back(PacketData::EntityMetadata::DataEntry{ .type = type, .index = index, .value = num });
			break;
		}
		case PacketData::EntityMetadata::Type::FLOAT: {
			float num = Read<float>();
			_metadata.push_back(PacketData::EntityMetadata::DataEntry{ .type = type, .index = index, .value = num });
			break;
		}
		case PacketData::EntityMetadata::Type::STRING: {
			std::string str = ReadString16();
			_metadata.push_back(PacketData::EntityMetadata::DataEntry{ .type = type, .index = index, .value = str });
			break;
		}
		case PacketData::EntityMetadata::Type::ITEM: {
			ItemStack item;
			item.id = Read<ItemId>();
			// TODO: Check if B1.7.3 actually does
			// this for Entity Metadata too
			if (item.id != Items::Id::INVALID) {
				item.count = Read<ItemAmount>();
				item.data = Read<ItemDamage>();
			}
			_metadata.push_back(PacketData::EntityMetadata::DataEntry{ .type = type, .index = index, .value = item });
			break;
		}
		case PacketData::EntityMetadata::Type::COORDINATES: {
			Int32_3 coordinate(Read<int32_t>(), Read<int32_t>(), Read<int32_t>());
			_metadata.push_back(
			    PacketData::EntityMetadata::DataEntry{ .type = type, .index = index, .value = coordinate });
			break;
		}
		default:
			Logger::Warning << "NetworkStream::ReadEntityMetadata: Unknown metadata type "
			                    << static_cast<int>(type);
			break;
		}
		// Read in the next value
		val = Read<uint8_t>();
	}
}

// TODO: Implement this! Ideally we could just pass an entity into here
// and it'd take care of things automatically
void NetworkStream::WriteEntityMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata) {
	for (auto& entry : _metadata) {
		uint8_t val = (static_cast<uint8_t>(entry.type) << 5) | (entry.index & 0x1F);
		Write(val);
		switch (entry.type) {
		case PacketData::EntityMetadata::Type::BYTE:
			Write(std::get<int8_t>(entry.value));
			break;
		case PacketData::EntityMetadata::Type::SHORT:
			Write(std::get<int16_t>(entry.value));
			break;
		case PacketData::EntityMetadata::Type::INTEGER:
			Write(std::get<int32_t>(entry.value));
			break;
		case PacketData::EntityMetadata::Type::FLOAT:
			Write(std::get<float>(entry.value));
			break;
		case PacketData::EntityMetadata::Type::STRING:
			WriteString16(std::get<std::string>(entry.value));
			break;
		case PacketData::EntityMetadata::Type::ITEM:
			Write(std::get<ItemStack>(entry.value).id);
			if (std::get<ItemStack>(entry.value).id != Items::Id::INVALID) {
				Write(std::get<ItemStack>(entry.value).count);
				Write(std::get<ItemStack>(entry.value).data);
			}
			break;
		case PacketData::EntityMetadata::Type::COORDINATES:
			Write(std::get<Int32_3>(entry.value).x);
			Write(std::get<Int32_3>(entry.value).y);
			Write(std::get<Int32_3>(entry.value).z);
			break;
		default:
			Logger::Warning << "NetworkStream::WriteEntityMetadata: Unknown metadata type "
			                    << static_cast<int>(entry.type);
			break;
		}
	}
	Write(PacketData::EntityMetadata::END);
}

bool NetworkStream::FlushWriteBuffer() {
	if (writeBuffer.empty())
		return connected;
	size_t sent = 0;
	while (sent < writeBuffer.size()) {
		int result = send(clientSocket, reinterpret_cast<const char*>(writeBuffer.data() + sent),
		                  static_cast<int>(writeBuffer.size() - sent), 0);
		if (result < 0) {
#if defined(_WIN32) || defined(_WIN64)
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
				break;
#else
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
#endif
			connected = false;
			break;
		}
		if (result == 0) {
			connected = false;
			break;
		}
		sent += static_cast<size_t>(result);
	}
	if (sent > 0) {
		writeBuffer.erase(writeBuffer.begin(),
		                  writeBuffer.begin() + static_cast<std::vector<unsigned char>::difference_type>(sent));
	}
	// TODO: Probably not right
	ResetPacketCount();
	return connected;
}

void NetworkStream::DrainToBuffer() {
	if (clientSocket == INVALID_SOCKET || !connected)
		return;

	// Compact away bytes that were consumed (and NOT rolled back) last
	// Tick, so the buffer doesn't grow unbounded.
	if (readPos > 0) {
		readBuffer.erase(readBuffer.begin(),
		                 readBuffer.begin() + static_cast<std::vector<uint8_t>::difference_type>(readPos));
		readPos = 0;
	}

	if (readBuffer.size() > MAX_READ_BUFFER) {
		connected = false;
		return;
	}

	uint8_t chunk[4096];
	for (;;) {
		int result = recv(clientSocket, reinterpret_cast<char*>(chunk), sizeof(chunk), 0);
		if (result > 0) {
			if (readBuffer.size() + static_cast<size_t>(result) > MAX_READ_BUFFER) {
				connected = false;
				return;
			}
			readBuffer.insert(readBuffer.end(), chunk, chunk + result);
			if (static_cast<size_t>(result) < sizeof(chunk))
				break; // almost certainly drained the socket for now
			continue;  // full chunk -- there could be more queued, keep going
		}
		if (result == 0) {
			connected = false;
			break;
		}
#if defined(_WIN32) || defined(_WIN64)
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK)
			break;
#else
		if (errno == EWOULDBLOCK || errno == EAGAIN)
			break;
#endif
		connected = false;
		break;
	}
}

bool NetworkStream::HasData() {
	return readBuffer.size() > readPos;
}