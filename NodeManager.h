#ifndef NODE_MANAGER_H
#define NODE_MANAGER_H

#include <map>            // To store the list of nodes (ID -> NodeInfo)
#include <vector>         // To return a list of nodes
#include <mutex>          // To protect access to the node list from multiple threads
#include <chrono>         // For time calculations (timeouts)
#include "TdlMessages.h"  // Needs definitions of NodeInfo and PositionReport

class NodeManager
{
public:
	// Constructor: Takes the ID of the node this manager belongs to (to ignore self).
	NodeManager(uint32_t selfNodeId);

	// Updates the position information for a node based on a received PositionReport.
	// Adds the node if it's not already known.
	void updateNodePosition(const PositionReport& report);

	// Updates only the 'lastHeardTime' for a node when any message is received.
	// Adds the node (without position info) if it's not already known.
	void updateLastHeardTime(uint32_t nodeId);

	// Checks the list and removes any nodes that haven't sent a message
	// within the specified timeout duration.
	void pruneTimeouts(std::chrono::seconds timeoutDuration);

	// Returns a copy of the current list of known nodes (safe for other threads to use).
	std::vector<NodeInfo> getNodeList();

	// Prints the current list of known nodes and their status to the console.
	void printNodeList();

	// Gets the ID of the node that owns this manager instance.
	uint32_t getSelfNodeId() const { return m_selfNodeId; }

private:
	uint32_t m_selfNodeId;                     // Store the ID of this node itself.
	std::map<uint32_t, NodeInfo> m_nodeMap;    // The main data structure: maps a node's ID to its NodeInfo.
	std::mutex m_mutex;                        // Mutex to ensure only one thread modifies the m_nodeMap at a time.
};

#endif // NODE_MANAGER_H