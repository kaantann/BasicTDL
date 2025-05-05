// NetworkManager.h
#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <vector>
#include <optional>   // To return optional received data
#include <cstdint>

// Forward declaration if needed, or include TdlMessages.h if sizes are needed here
// (Better to keep dependencies minimal in headers)

// Structure to hold received packet details
struct ReceivedPacket
{
	std::vector<uint8_t> data; // Use uint8_t for byte buffer
	sockaddr_in senderAddress;
};


class NetworkManager
{
public:
	NetworkManager(uint16_t port, const char* broadcastAddress, int receiveTimeoutMs = 1000);

	~NetworkManager();

	bool isInitialized() const;

	bool sendBroadcast(const void* data, size_t size);

	// Attempt to receive a packet (blocks up to the configured timeout)
	// Returns the received packet if successful, std::nullopt on timeout or error.
	std::optional<ReceivedPacket> receive();

	// Disable copy and assignment
	NetworkManager(const NetworkManager&) = delete;
	NetworkManager& operator=(const NetworkManager&) = delete;

private:
	bool m_initialized = false;
	SOCKET m_sendSocket = INVALID_SOCKET;
	SOCKET m_recvSocket = INVALID_SOCKET;
	sockaddr_in m_broadcastAddr = {};
	uint16_t m_port = 0;
	WSADATA m_wsaData = {}; // Store WSAData

	static constexpr size_t RECEIVE_BUFFER_SIZE = 2048; // Internal buffer size for recvfrom
};

#endif // NETWORK_MANAGER_H