#ifndef TDL_MESSAGES_H
#define TDL_MESSAGES_H

#include <cstdint> // For fixed-width integers like uint32_t
#include <chrono>  // Needed for std::chrono::steady_clock
#include <string>  // Used indirectly for text messages

// --- Message Type Constants ---
// An enumeration makes the message types easier to read than just numbers.
enum MessageType : uint32_t
{
	POSITION_REPORT_TYPE = 1, // ID for position updates
	HEARTBEAT_TYPE = 2, // ID for simple "I'm alive" messages
	TEXT_MESSAGE_TYPE = 3  // ID for chat messages
	// Add more types here later if needed...
};

// --- Common Message Header ---
// Every message we send starts with this structure.
// This allows the receiver to know who sent the message and what type it is
// before reading the rest of the data.
struct MessageHeader
{
	MessageType messageType = static_cast<MessageType>(0); // What kind of message is this?
	uint32_t sourceNodeId = 0; // Which node sent this message?
};

// --- Specific Message Structures ---
// These define the actual data content for each message type.

// Position Report: Contains location data.
struct PositionReport
{
	MessageHeader header; // All messages include the header first.
	double latitude = 0.0;
	double longitude = 0.0;
	double altitude = 0.0;

	// Constructor to automatically set the correct message type when created.
	PositionReport() { header.messageType = POSITION_REPORT_TYPE; }
};

// Heartbeat: A very simple message, might just contain the header.
struct HeartbeatMessage
{
	MessageHeader header; // Contains type and source ID.
	// Could add other basic status info here later.

	// Constructor to automatically set the correct message type.
	HeartbeatMessage() { header.messageType = HEARTBEAT_TYPE; }
};

// Text Message: Contains a short text string.
#define MAX_TEXT_MSG_LENGTH 64 // Define maximum length for the text content.
struct TextMessage
{
	MessageHeader header; // Contains type and source ID.
	char text[MAX_TEXT_MSG_LENGTH] = {0}; // Fixed-size buffer for the text. Initialize to zeros.

	// Constructor to automatically set the correct message type.
	TextMessage() { header.messageType = TEXT_MESSAGE_TYPE; }
};


// --- Node Management Structures ---
// Holds information about a known node in the network
struct NodeInfo
{
	uint32_t nodeId;                     // The unique ID of the other node.
	PositionReport lastPosition;         // Store the last known position report received from this node.
	std::chrono::steady_clock::time_point lastHeardTime; // When did we last receive *any* message from this node?

	// Default constructor (needed for use in std::map).
	NodeInfo() : nodeId(0) {}

	// Constructor used by NodeManager when adding a node.
	NodeInfo(uint32_t id, std::chrono::steady_clock::time_point time)
		: nodeId(id), lastHeardTime(time)
	{
		// Initialize lastPosition with default values, indicating we might not
		// have received a specific position report yet.
		lastPosition.header.sourceNodeId = id;
		lastPosition.latitude = 0.0;
		lastPosition.longitude = 0.0;
		lastPosition.altitude = 0.0;
	}

	// Helper method to update only the position data within this NodeInfo.
	void updatePosition(const PositionReport& report)
	{
		lastPosition = report; // Replace the stored position with the new one.
	}
};

#endif // TDL_MESSAGES_H