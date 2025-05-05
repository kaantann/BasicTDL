// NetworkManager.cpp
#include "NetworkManager.h"
#include <iostream>
#include <stdexcept> // Could use for exceptions on critical init failure

#pragma comment(lib, "Ws2_32.lib") // Link Winsock library

NetworkManager::NetworkManager(uint16_t port, const char* broadcastAddress, int receiveTimeoutMs) :
	m_port(port)
{

	// 1. Initialize Winsock
	int iResult = WSAStartup(MAKEWORD(2, 2), &m_wsaData);
	if (iResult != 0)
	{
		std::cerr << "[NetMgr] WSAStartup failed: " << iResult << std::endl;
		return; // m_initialized remains false
	}

	// --- Setup Send Socket ---
	m_sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_sendSocket == INVALID_SOCKET)
	{
		std::cerr << "[NetMgr] Send socket creation failed: " << WSAGetLastError() << std::endl;
		WSACleanup();
		return;
	}

	BOOL broadcastOption = TRUE;
	if (setsockopt(m_sendSocket, SOL_SOCKET, SO_BROADCAST, (char*)&broadcastOption, sizeof(broadcastOption)) == SOCKET_ERROR)
	{
		std::cerr << "[NetMgr] setsockopt(SO_BROADCAST) failed: " << WSAGetLastError() << std::endl;
		closesocket(m_sendSocket);
		WSACleanup();
		return;
	}

	// Setup broadcast address structure
	m_broadcastAddr.sin_family = AF_INET;
	m_broadcastAddr.sin_port = htons(m_port);
	if (inet_pton(AF_INET, broadcastAddress, &m_broadcastAddr.sin_addr) != 1)
	{
		std::cerr << "[NetMgr] inet_pton failed for broadcast address: " << WSAGetLastError() << std::endl;
		closesocket(m_sendSocket);
		WSACleanup();
		return;
	}


	// --- Setup Receive Socket ---
	m_recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_recvSocket == INVALID_SOCKET)
	{
		std::cerr << "[NetMgr] Receive socket creation failed: " << WSAGetLastError() << std::endl;
		closesocket(m_sendSocket); // Clean up send socket too
		WSACleanup();
		return;
	}

	BOOL reuseAddr = TRUE;
	setsockopt(m_recvSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr)); // Optional, often helpful

	// Set receive timeout
	DWORD timeout = static_cast<DWORD>(receiveTimeoutMs);
	if (setsockopt(m_recvSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
	{
		std::cerr << "[NetMgr] setsockopt(SO_RCVTIMEO) failed: " << WSAGetLastError() << std::endl;
		// Continue anyway? Or treat as fatal? Let's continue for now.
	}


	// Bind receive socket
	sockaddr_in recvAddr = {};
	recvAddr.sin_family = AF_INET;
	recvAddr.sin_port = htons(m_port);
	recvAddr.sin_addr.s_addr = INADDR_ANY;
	if (bind(m_recvSocket, (SOCKADDR*)&recvAddr, sizeof(recvAddr)) == SOCKET_ERROR)
	{
		std::cerr << "[NetMgr] Bind failed: " << WSAGetLastError() << std::endl;
		closesocket(m_recvSocket);
		closesocket(m_sendSocket);
		WSACleanup();
		return;
	}

	// If all steps succeeded
	m_initialized = true;
	std::cout << "[NetMgr] Network Manager Initialized. Port: " << m_port
		<< ", Broadcast: " << broadcastAddress << std::endl;
}

NetworkManager::~NetworkManager()
{
	std::cout << "[NetMgr] Cleaning up..." << std::endl;
	if (m_sendSocket != INVALID_SOCKET)
	{
		closesocket(m_sendSocket);
	}
	if (m_recvSocket != INVALID_SOCKET)
	{
		closesocket(m_recvSocket);
	}
	if (m_initialized)
	{ // Only call WSACleanup if WSAStartup succeeded
		WSACleanup();
		std::cout << "[NetMgr] Winsock Cleaned up." << std::endl;
	}
}

bool NetworkManager::isInitialized() const
{
	return m_initialized;
}

bool NetworkManager::sendBroadcast(const void* data, size_t size)
{
	if (!m_initialized || m_sendSocket == INVALID_SOCKET)
	{
		return false;
	}

	int bytesSent = sendto(m_sendSocket,
		static_cast<const char*>(data), // Cast data pointer
		static_cast<int>(size),         // Cast size
		0,
		(SOCKADDR*)&m_broadcastAddr,
		sizeof(m_broadcastAddr));

	if (bytesSent == SOCKET_ERROR)
	{
		std::cerr << "[NetMgr] sendto failed: " << WSAGetLastError() << std::endl;
		return false;
	}
	if (bytesSent != static_cast<int>(size))
	{
		std::cerr << "[NetMgr] Warning: sendto sent " << bytesSent << " bytes, but expected " << size << std::endl;
		// Return true anyway, as some data was sent? Or false? Let's return true for now.
	}
	return true;
}

std::optional<ReceivedPacket> NetworkManager::receive()
{
	if (!m_initialized || m_recvSocket == INVALID_SOCKET)
	{
		return std::nullopt;
	}

	// Use a stack buffer or member buffer if preferred over heap allocation?
	// std::vector is convenient but has potential overhead. Let's use vector for now.
	std::vector<uint8_t> buffer(RECEIVE_BUFFER_SIZE);
	sockaddr_in senderAddr = {};
	int senderAddrSize = sizeof(senderAddr);

	int bytesReceived = recvfrom(m_recvSocket,
		reinterpret_cast<char*>(buffer.data()), // Get pointer to vector data
		static_cast<int>(buffer.size()),        // Max size
		0,
		(SOCKADDR*)&senderAddr,
		&senderAddrSize);

	if (bytesReceived == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error == WSAETIMEDOUT)
		{
			// Timeout is expected, return empty optional
			return std::nullopt;
		}
		// Ignore connection reset errors common with UDP
		if (error == WSAECONNRESET)
		{
			std::cerr << "[NetMgr] Warning: recvfrom reported WSAECONNRESET." << std::endl;
			return std::nullopt; // Treat as non-fatal, no data received
		}

		// Other errors
		std::cerr << "[NetMgr] recvfrom failed: " << error << std::endl;
		return std::nullopt;
	}

	if (bytesReceived == 0)
	{
		// Unlikely with UDP, but handle it
		return std::nullopt;
	}

	// Shrink buffer to actual received size
	buffer.resize(bytesReceived);

	// Create and return the packet
	ReceivedPacket packet;
	packet.data = std::move(buffer); // Efficiently move data into the struct
	packet.senderAddress = senderAddr;

	return std::make_optional(std::move(packet)); // Move the packet into optional
}