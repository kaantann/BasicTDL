// NodeManager.cpp
#include "NodeManager.h" // Include the corresponding header file
#include <iostream>      // For printing output (e.g., timeouts, list)
#include <vector>        // Used in pruneTimeouts and getNodeList

// Constructor: Initializes the NodeManager with the ID of the node it belongs to.
NodeManager::NodeManager(uint32_t selfNodeId) : m_selfNodeId(selfNodeId)
{
    // Constructor body can be empty if initialization is done in the initializer list.
}

// Updates the position details for a specific node.
// If the node isn't known, it adds it to the list.
void NodeManager::updateNodePosition(const PositionReport& report)
{
    // Ignore reports supposedly from our own node ID.
    if (report.header.sourceNodeId == m_selfNodeId)
    {
        return;
    }

    auto now = std::chrono::steady_clock::now(); // Get the current time.

    // --- Critical Section Start ---
    // Lock the mutex to prevent other threads from accessing m_nodeMap concurrently.
    // The lock_guard automatically unlocks the mutex when it goes out of scope (at the end of this function).
    std::lock_guard<std::mutex> lock(m_mutex);

    // Try to find the node in our map using its ID.
    auto it = m_nodeMap.find(report.header.sourceNodeId);

    if (it == m_nodeMap.end())
    {
        // Node not found in the map. This is the first time we've heard from it
        // (or at least the first time with a PositionReport). Add a new entry.
        NodeInfo newNodeInfo(report.header.sourceNodeId, now); // Create basic info with current time.
        newNodeInfo.updatePosition(report); // Update its position data.
        m_nodeMap[report.header.sourceNodeId] = newNodeInfo; // Add it to the map.
        std::cout << "[NodeMgr] Added new Node ID " << report.header.sourceNodeId << " from PositionReport." << std::endl;
    }
    else
    {
        // Node found! Update its stored position data and its last heard time.
        // 'it->second' refers to the NodeInfo object stored in the map.
        it->second.updatePosition(report);
        it->second.lastHeardTime = now; // Update timestamp since we received a position report.
        // Optional: Log the update.
        // std::cout << "[NodeMgr] Updated position for Node ID: " << report.header.sourceNodeId << std::endl;
    }
    // --- Critical Section End (Mutex automatically unlocked) ---
}

// Updates only the 'lastHeardTime' for a node. Used when any message type is received.
// If the node isn't known, it adds it to the list (without position info initially).
void NodeManager::updateLastHeardTime(uint32_t nodeId)
{
    // Ignore messages supposedly from our own node ID.
    if (nodeId == m_selfNodeId)
    {
        return;
    }

    auto now = std::chrono::steady_clock::now(); // Get the current time.

    // --- Critical Section Start ---
    std::lock_guard<std::mutex> lock(m_mutex);

    // Try to find the node in our map.
    auto it = m_nodeMap.find(nodeId);

    if (it != m_nodeMap.end())
    {
        // Node exists in the map. Just update its lastHeardTime.
        it->second.lastHeardTime = now;
    }
    else
    {
        // Node doesn't exist in our list yet (e.g., we received a Heartbeat first).
        // Create a basic entry for it with the current time. Position will be default.
        NodeInfo newNodeInfo(nodeId, now);
        m_nodeMap[nodeId] = newNodeInfo; // Add it to the map.
        std::cout << "[NodeMgr] Added new Node ID " << nodeId << " from generic message." << std::endl;
    }
    // --- Critical Section End (Mutex automatically unlocked) ---
}

// Removes nodes from the list if they haven't sent any message within the timeout period.
void NodeManager::pruneTimeouts(std::chrono::seconds timeoutDuration)
{
    auto now = std::chrono::steady_clock::now(); // Get the current time.
    std::vector<uint32_t> nodesToRemove; // Create a temporary list to store IDs of nodes to remove.

    // --- Critical Section Start ---
    // We need to lock while iterating through the map.
    std::lock_guard<std::mutex> lock(m_mutex);

    // Iterate through all the nodes currently in our map.
    // 'pair' contains the node ID (pair.first) and the NodeInfo object (pair.second).
    for (const auto& pair : m_nodeMap)
    {
        // Calculate how long it's been since we last heard from this node.
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - pair.second.lastHeardTime);

        // If the elapsed time is greater than the allowed timeout...
        if (elapsed > timeoutDuration)
        {
            nodesToRemove.push_back(pair.first); // ...mark this node's ID for removal.
        }
    }

    // --- Critical Section Still Active (within the same lock scope) ---

    // Now, remove the marked nodes from the actual map.
    // We do this *after* iterating to avoid issues with modifying the map while looping through it.
    for (uint32_t nodeIdToRemove : nodesToRemove)
    {
        m_nodeMap.erase(nodeIdToRemove); // Remove the entry with this ID.
        std::cout << "[NodeMgr] Timed out Node ID: " << nodeIdToRemove << std::endl;
    }
    // --- Critical Section End (Mutex automatically unlocked) ---
}

// Creates and returns a copy of the current node list.
// This is thread-safe because it locks the map during the copy process.
std::vector<NodeInfo> NodeManager::getNodeList()
{
    std::vector<NodeInfo> listCopy; // Create an empty vector to hold the copy.

    // --- Critical Section Start ---
    std::lock_guard<std::mutex> lock(m_mutex);

    listCopy.reserve(m_nodeMap.size()); // Optimize by reserving space in the vector.
    // Copy each NodeInfo object from the map into the vector.
    for (const auto& pair : m_nodeMap)
    {
        listCopy.push_back(pair.second);
    }
    // --- Critical Section End (Mutex automatically unlocked) ---

    return listCopy; // Return the copied list.
}

// Prints the current list of known nodes to the console.
void NodeManager::printNodeList()
{
    // Get a thread-safe copy of the list first.
    std::vector<NodeInfo> currentNodes = getNodeList(); // This method handles its own locking.

    // Don't print anything if the list is empty.
    if (currentNodes.empty())
    {
        // std::cout << "[NodeMgr] Node list is empty." << std::endl; // Optional: uncomment if you want this message
        return;
    }

    // Get the current time to calculate 'time since last heard'.
    auto now = std::chrono::steady_clock::now();

    // Print a header for the list.
    std::cout << "\n===== Known Network Nodes (" << currentNodes.size() << ") =====" << std::endl;

    // Loop through the copied list and print details for each node.
    for (const auto& node : currentNodes)
    {
        auto elapsedSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - node.lastHeardTime).count();
        std::cout << "  Node ID: " << node.nodeId
            << " | Pos (Lat/Lon): ";
        // Check if we have valid position data (simple check based on default 0.0 values)
        if (node.lastPosition.latitude != 0.0 || node.lastPosition.longitude != 0.0)
        {
            std::cout << node.lastPosition.latitude << "/" << node.lastPosition.longitude;
        }
        else
        {
            std::cout << "N/A"; // Print N/A if we haven't received a position report yet.
        }
        std::cout << " | Last Heard: " << elapsedSeconds << "s ago" << std::endl;
    }
    // Print a footer for the list.
    std::cout << "========================================" << std::endl;
}