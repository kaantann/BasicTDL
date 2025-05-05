// main.cpp
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Only include message and manager headers now
#include "NetworkManager.h"
#include "NodeManager.h"
#include "TdlMessages.h"

// Remove Winsock includes and pragma comment if NetworkManager handles it
// #include <winsock2.h>
// #include <ws2tcpip.h>
// #pragma comment(lib, "Ws2_32.lib")

#define TDL_PORT 30000
// #define BROADCAST_ADDRESS "255.255.255.255" // Now passed to NetworkManager
const char* BROADCAST_ADDRESS_STR = "255.255.255.255"; // Use a const char*

#define SEND_INTERVAL_SECONDS 5
#define NODE_TIMEOUT_SECONDS (SEND_INTERVAL_SECONDS * 3)

std::atomic<bool> g_shutdown_flag(false); // Keep global shutdown flag for threads

// --- Receiver Thread Function ---
void receiverThreadFunc(NetworkManager& netMgr, NodeManager& nodeManager)
{
	std::cout << "[Receiver] Thread started." << std::endl;
	// No socket setup needed here

	while (!g_shutdown_flag)
	{
		// Use NetworkManager::receive()
		auto receivedOpt = netMgr.receive();

		if (!receivedOpt)
			continue;

		const ReceivedPacket& packet = receivedOpt.value();

		if (packet.data.size() < sizeof(MessageHeader))
		{
			std::cerr << "[Receiver] Warning: Received packet too small (" << packet.data.size() << " bytes). Discarding." << std::endl;
			continue;
		}

		// --- Message Parsing ---
		// 1. Get header pointer (use packet.data.data())
		const MessageHeader* header = reinterpret_cast<const MessageHeader*>(packet.data.data());

		// Get sender IP for logging
		char senderIp[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &packet.senderAddress.sin_addr, senderIp, INET_ADDRSTRLEN);

		// Ignore self - USE NodeManager's self ID
		if (header->sourceNodeId == nodeManager.getSelfNodeId())
		{ // Need to add getSelfNodeId() to NodeManager
			continue;
		}

		// 2. Update last heard time for ANY valid message from another node
		nodeManager.updateLastHeardTime(header->sourceNodeId);

		// 3. Switch based on message type
		switch (header->messageType)
		{
			case POSITION_REPORT_TYPE:
			{
				if (packet.data.size() == sizeof(PositionReport))
				{
					PositionReport receivedReport;
					// Copy from packet.data into the struct
					memcpy(&receivedReport, packet.data.data(), sizeof(PositionReport));
					nodeManager.updateNodePosition(receivedReport);
					// std::cout << "[Receiver] Processed PositionReport from Node " << header->sourceNodeId << std::endl;
				}
				else
				{ /* Size mismatch warning */
				}
				break;
			}
			case HEARTBEAT_TYPE:
			{
				if (packet.data.size() == sizeof(HeartbeatMessage))
				{
					// std::cout << "[Receiver] Processed Heartbeat from Node " << header->sourceNodeId << std::endl;
				}
				else
				{ /* Size mismatch warning */
				}
				break;
			}
			case TEXT_MESSAGE_TYPE:
			{
				if (packet.data.size() == sizeof(TextMessage))
				{
					TextMessage receivedMsg;
					memcpy(&receivedMsg, packet.data.data(), sizeof(TextMessage));
					receivedMsg.text[MAX_TEXT_MSG_LENGTH - 1] = '\0'; // Ensure null termination

					std::cout << "\n--- Text Message Received ---" << std::endl;
					std::cout << "  From Node: " << receivedMsg.header.sourceNodeId << " [" << senderIp << "]" << std::endl;
					std::cout << "  Message:   " << receivedMsg.text << std::endl;
					std::cout << "-----------------------------" << std::endl;
				}
				else
				{ /* Size mismatch warning */
				}
				break;
			}
			default:
				break;
		}

	}

	std::cout << "[Receiver] Shutdown signal received. Thread finished." << std::endl;
}

// --- Sender Thread Function ---
void senderThreadFunc(NetworkManager& netMgr, NodeManager& nodeManager)
{
	uint32_t myNodeId = nodeManager.getSelfNodeId();
	std::cout << "[Sender] Thread started (Node ID: " << myNodeId << ")." << std::endl;

	PositionReport myPosReport;
	myPosReport.header.sourceNodeId = myNodeId;
	HeartbeatMessage myHeartbeat;
	myHeartbeat.header.sourceNodeId = myNodeId;

	// Timing logic remains the same
	auto lastPosSendTime = std::chrono::steady_clock::now();
	const auto posSendInterval = std::chrono::seconds(SEND_INTERVAL_SECONDS);
	bool sentTestTextMessage = false;

	while (!g_shutdown_flag)
	{
		auto now = std::chrono::steady_clock::now();

		// --- Send Position Report ---
		if (now - lastPosSendTime >= posSendInterval)
		{
			// ... update report fields ...
			myPosReport.latitude = 50.0 + (myNodeId * 0.01) + ((/*time calc*/ 0 % 100) * 0.001);
			myPosReport.longitude = -1.0 + (myNodeId * 0.01) + ((/*time calc*/ 0 % 100) * 0.001);
			myPosReport.altitude = 100.0 + myNodeId;

			// Use NetworkManager::sendBroadcast
			if (netMgr.sendBroadcast(&myPosReport, sizeof(myPosReport)))
			{
				// std::cout << "[Sender] Sent PositionReport." << std::endl;
				lastPosSendTime = now;
			}
			else
			{ /* Handle send error if needed */
			}
		}

		// --- Send Heartbeat ---
		// ... check timer ...
		{
			if (netMgr.sendBroadcast(&myHeartbeat, sizeof(myHeartbeat)))
			{
				// lastHtbSendTime = now;
			}
			else
			{
				/* Handle send error */
			}
		}

		// --- Send Test Text Message ---
		if (!sentTestTextMessage)
		{
			TextMessage testMsg;
			testMsg.header.sourceNodeId = myNodeId;
			std::string msgContent = "Hello from Node " + std::to_string(myNodeId) + " via NetMgr!";
			strncpy_s(testMsg.text, MAX_TEXT_MSG_LENGTH, msgContent.c_str(), _TRUNCATE);

			if (netMgr.sendBroadcast(&testMsg, sizeof(testMsg)))
			{
				std::cout << "[Sender] Sent Test TextMessage." << std::endl;
				sentTestTextMessage = true;
			}
			else
			{ /* Handle send error */
			}
		}

		// --- Perform Periodic Tasks (Pruning, Printing List) ---
		// ... check timers ...
		nodeManager.pruneTimeouts(std::chrono::seconds(NODE_TIMEOUT_SECONDS));
		nodeManager.printNodeList();
		// ... update lastPruneTime, lastPrintTime ...


		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}

	std::cout << "[Sender] Shutdown signal received. Thread finished." << std::endl;
	// No socket cleanup needed here
}

// --- Main Function ---
int main(int argc, char* argv[])
{
	uint32_t myNodeId = (argc > 1) ? std::stoul(argv[1]) : 1;
	std::cout << "[Main] Starting Simple TDL Node (ID: " << myNodeId << ") using NetworkManager." << std::endl;

	// --- Create Managers ---
	std::unique_ptr<NetworkManager> networkManager = std::make_unique<NetworkManager>(TDL_PORT, BROADCAST_ADDRESS_STR);

	if (!networkManager || !networkManager->isInitialized())
	{
		std::cerr << "[Main] Failed to initialize Network Manager. Exiting." << std::endl;
		return 1; // Exit if network setup failed
	}

	NodeManager nodeManager(myNodeId);
	// Add getSelfNodeId() to NodeManager if receiver needs it

	// --- Create and Launch Threads ---
	std::cout << "[Main] Launching Sender and Receiver threads..." << std::endl;
	// Pass references using std::ref() or raw pointer from unique_ptr.get()
	std::thread rxThread(receiverThreadFunc, std::ref(*networkManager), std::ref(nodeManager));
	std::thread txThread(senderThreadFunc, std::ref(*networkManager), std::ref(nodeManager));

	// --- Wait for user input to shut down ---
	std::cout << "[Main] Threads running. Press Enter to stop..." << std::endl;
	std::cin.get();

	// --- Signal threads to shut down ---
	std::cout << "[Main] Shutdown signal sent. Waiting for threads to join..." << std::endl;
	g_shutdown_flag = true;

	// --- Wait for threads to complete ---
	rxThread.join();
	txThread.join();
	std::cout << "[Main] Threads joined." << std::endl;

	// --- Cleanup ---
	// NetworkManager destructor called automatically when unique_ptr goes out of scope.
	// NodeManager cleaned up as it goes out of scope.
	// No need for explicit WSACleanup here.

	std::cout << "[Main] Exiting." << std::endl;
	return 0;
}