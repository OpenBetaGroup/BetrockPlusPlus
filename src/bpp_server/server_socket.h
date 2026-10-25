/*
 * Copyright (c) 2026, Pixel Brush <pixelbrush.dev>
 * Copyright (c) 2026, Aidan <JcbbcEnjoyer>
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
*/
#pragma once
#include "logger.h"
#include "platform/platform_caps.h"
#if defined(BPP_HAS_POSIX_SOCKETS)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#ifdef BPP_HAS_TCP_NODELAY
#include <netinet/tcp.h>
#endif
#include <sys/socket.h>
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

#include <string>

#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16 // "255.255.255.255" + NUL
#endif

namespace ServerSocketManager {
inline void CloseSocket(int _socket) {
#if defined(_WIN32) || defined(_WIN64)
	closesocket(_socket);
	WSACleanup();
#else
	close(_socket);
#endif
}

inline int CreateServerSocket(int _port) {
	int serverSocket = -1;
#if defined(_WIN32) || defined(_WIN64)
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);

	// This allows the server to quickly restart,
	// if not enabled the OS will hold the port until the last packet time expires.
	int reuse = 1;
	setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

#ifdef BPP_HAS_TCP_NODELAY
	int nodelay = 1;
	setsockopt(serverSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
#endif

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(_port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(serverSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
		Logger::Error << "**** FAILED TO BIND SOCKET! ****" << "\n";
		return -1;
	}
	// feels unsafe. did you know it was only 5 in 4.4BSD!? Maybe set to like 128/512.
	listen(serverSocket, BPP_LISTEN_BACKLOG);

#if defined(_WIN32) || defined(_WIN64)
	u_long mode = 1;
	ioctlsocket(serverSocket, FIONBIO, &mode);
#else
	fcntl(serverSocket, F_SETFL, O_NONBLOCK);
#endif

	return serverSocket;
}

inline int CreateClientSocket(int _socket = -1, std::string* _outIp = nullptr) {
	sockaddr_in clientAddr{};
#if defined(_WIN32) || defined(_WIN64)
	int addrLen = sizeof(clientAddr);
	SOCKET rawSocket = accept(_socket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
	if (rawSocket == INVALID_SOCKET)
		return -1;
	u_long clientMode = 1;
	ioctlsocket(rawSocket, FIONBIO, &clientMode);
	DWORD recvTimeout = 45;
	setsockopt(rawSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&recvTimeout), sizeof(recvTimeout));
	int nodelay = 1;
	setsockopt(rawSocket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));
	int clientSocket = static_cast<int>(rawSocket);
#else
	socklen_t addrLen = sizeof(clientAddr);
	int clientSocket = accept(_socket, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
	if (clientSocket < 0)
		return -1;
	fcntl(clientSocket, F_SETFL, O_NONBLOCK);
#ifdef BPP_HAS_SO_RCVTIMEO
	struct timeval recvTimeout{ 0, 45000 };
	setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&recvTimeout), sizeof(recvTimeout));
#endif
#ifdef BPP_HAS_TCP_NODELAY
	int nodelay = 1;
	setsockopt(clientSocket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
#endif
#endif

	if (_outIp) {
#if defined(PS2)
		// lwIP on the PS2 only offers the legacy inet_ntoa
		if (const char* ip = inet_ntoa(clientAddr.sin_addr))
			*_outIp = ip;
#else
		char ipBuffer[INET_ADDRSTRLEN] = {};
		if (inet_ntop(AF_INET, &clientAddr.sin_addr, ipBuffer, sizeof(ipBuffer)))
			*_outIp = ipBuffer;
#endif
	}

	return clientSocket;
}
} // namespace ServerSocketManager