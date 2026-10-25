/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#define INVALID_SOCKET -1
#include "platform/platform_caps.h"
#if defined(BPP_HAS_POSIX_SOCKETS)
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif
#include "helpers/byteswap_compat.h"
#include "packet_data.h"
#include <atomic>
#include <cstdint>
#include <cstring>
#include <packet_ids.h>
#include <string>
#include <type_traits>
#include <vector>

class NetworkStream {
public:
	// Max time FlushWriteBufferBlocking() will let a single send() block for
	static constexpr int SHUTDOWN_FLUSH_TIMEOUT_MS = 2000;

	NetworkStream(int _clientSocket);
	~NetworkStream();
	NetworkStream(const NetworkStream&) = delete;
	NetworkStream& operator=(const NetworkStream&) = delete;
	NetworkStream(NetworkStream&& _other) noexcept
	    : clientSocket(_other.clientSocket), connected(_other.connected.load()), shortRead(_other.shortRead),
	      readBuffer(std::move(_other.readBuffer)), readPos(_other.readPos),
	      writeBuffer(std::move(_other.writeBuffer)) {
		_other.clientSocket = INVALID_SOCKET;
		_other.readPos = 0;
	}
	bool NewClient();
	// NOTE: This is a limitation of the Vanilla Beta 1.7.3 Client,
	// as it can only process 100 Packets per tick
	static constexpr uint8_t MAX_PACKETS_PER_TICK = 100;
	uint8_t packetsInQueue = 0;
	void IncrementPacketCount(PacketId _id);
	void ResetPacketCount();

	// NOTE: if CheckAndClearShortRead()/IsShortRead() ends up true after this
	// call, the returned value is not meaningful, it was built from a
	// truncated buffer. The caller must Rollback() to the packet's Mark()
	// and retry once more data has arrived (next tick's FillBuffer()).
	template <typename T>
	T Read() {
		static_assert(std::is_trivially_copyable_v<T>,
		              "NetworkStream::Read<T>: use Read<std::string>() or Read<std::string>() for string types");
		T buffer{};
		ReadBytes(reinterpret_cast<uint8_t*>(&buffer), sizeof(T));
		return FromBigEndian(buffer);
	}

	template <typename T = int>
	void Write(const T& _data) {
		if constexpr (std::is_same_v<T, bool>) {
			int8_t boolData = static_cast<int8_t>(_data);
			WriteBytes(reinterpret_cast<const uint8_t*>(&boolData), sizeof(int8_t));
		} else {
			T networkData = ToBigEndian(_data);
			WriteBytes(reinterpret_cast<const uint8_t*>(&networkData), sizeof(T));
		}
	}

	void SetConnected(bool _val) {
		connected = _val;
	}
	bool IsConnected() const {
		return connected;
	}

	// String-8 Read-Write
	std::string ReadString8();
	void WriteString8(const std::string& _str);

	// String-16 Read-Write
	std::string ReadString16();
	void WriteString16(const std::string& _str);

	// Copies up to _len bytes out of the internal read buffer.
	size_t ReadBytes(uint8_t* _buf, size_t _len);

	// Append bytes to the per-session write buffer (no syscall).
	void WriteBytes(const uint8_t* _buf, size_t _len);

	// Handles Entity Metadata Interpreting
	void ReadEntityMetadata(std::vector<PacketData::EntityMetadata::DataEntry>& _metadata);

	// Handles Entity Metadata Conversion
	void WriteEntityMetadata(const std::vector<PacketData::EntityMetadata::DataEntry>& _metadata);

	// Flush the write buffer to the socket (non-blocking).
	// On the server this runs on the write thread, not the tick thread.
	// Returns false if the connection was lost.
	bool FlushWriteBuffer();
	// Blocking flush for use SHUTDOWN ONLY
	void FlushWriteBufferBlocking();

	// Check if there is unconsumed buffer data.
	bool HasData();

	// Drains everything currently available on the socket (non-blocking,
	// looping until EWOULDBLOCK/EAGAIN) into the internal read buffer.
	//
	// Call this exactly once per tick, per client, before anything else
	void DrainToBuffer();

	// Snapshot the read cursor. Call this right before reading a packet's
	// ID, for example before you start parsing a new packet.
	size_t Mark() const {
		return readPos;
	}

	// Rewind the read cursor back to a Mark(). Use this when
	// CheckAndClearShortRead() is true after attempting to parse a packet.
	// it rollbacks the whole packet (including fields already read this
	// attempt) to be reparsed next time once the rest of the bytes are received.
	void Rollback(size_t _mark) {
		readPos = _mark;
		shortRead = false;
	}

	// Append pre-serialised bytes directly to the write buffer.
	// Used for shared-packet broadcast: serialise once, copy to N sessions.
	void WriteRaw(const uint8_t* _data, size_t _len) {
		WriteBytes(_data, _len);
	}

	// Read-only view of the pending write buffer.
	// Valid only until the next Write*/writeRaw/flushWriteBuffer call.
	const std::vector<uint8_t>& GetRawWriteBuffer() const {
		return writeBuffer;
	}

	// Returns true if the *most recent* ReadBytes()-family call came up
	// short (not enough buffered data), AND CLEARS the flag.
	//
	// You probably want to use IsShortRead() instead.
	bool CheckAndClearShortRead() {
		bool val = shortRead;
		shortRead = false;
		return val;
	}

	// Check if the read is short. Useful when doing multiple reads.
	bool IsShortRead() const {
		return shortRead;
	}

private:
	int clientSocket = INVALID_SOCKET;
	// Written by the write thread (flush), read by the tick thread
	std::atomic<bool> connected{ true };
	bool shortRead = false;

	// All un-consumed bytes recevied from the socket.
	std::vector<uint8_t> readBuffer;
	size_t readPos = 0;

	std::vector<uint8_t> writeBuffer;

	static constexpr size_t MAX_READ_BUFFER = 1u << 20; // 1 MiB
	static constexpr size_t MAX_METADATA_ENTRIES = 256;
};