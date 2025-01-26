#pragma once

#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <map>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <memory>


class StompProtocol {
private:
    // Connection management
    std::shared_ptr<ConnectionHandler> connectionHandler;
    std::mutex stateMutex;
    
    // STOMP Protocol state
    std::atomic<bool> isLoggedIn{false};
    int nextReceiptId{0};
    int nextSubscriptionId{0};
    std::string currentUsername;
    
    // Thread-safe data structures
    std::mutex dataMutex;
    std::map<std::string, int> channelToSubId;    // channel -> subId
    std::map<int, std::string> subIdToChannel;    // subId -> channel
    std::map<std::string, std::string> receiptIdToMsg;  // receiptId -> pending message
    std::map<std::string, std::vector<Event>> userChannelEvents; // channel_user -> events
    
    // Frame creation methods
    std::string createConnectFrame(const std::string& username, const std::string& password);
    std::string createSubscribeFrame(const std::string& channel);
    std::string createUnsubscribeFrame(int subscriptionId);
    std::string createSendFrame(const std::string& destination, const std::string& body);
    std::string createDisconnectFrame();
    
    // Helper methods
    std::vector<std::string> split(const std::string& s, char delimiter) const;
    std::string getHeader(const std::string& header, const std::vector<std::string>& lines) const;
    void saveEventForUser(const std::string& channel, const std::string& user, const Event& event);
    std::string formatDateTime(int epochTime) const;
    std::string formatEventMessage(const Event& event) const;
    bool parseHostPort(const std::string& hostPort, std::string& host, short& port);
    std::string trim(const std::string& str);



public:
    StompProtocol();
    ~StompProtocol() = default;
    
    // Connection management
    bool connect(const std::string& host, short port, 
                const std::string& username, const std::string& password);
    void disconnect();
    bool isConnected() const;
    //bool shouldStop() const { return shouldTerminate; }
    
    // Main protocol operations
    std::vector<std::string> processInput(const std::string& input);
    void processResponse(const std::string& response);
    bool send(const std::string& frame);
    bool receiveFrame(std::string& frame);
    
    // Event handling
    void writeEventSummary(const std::string& channel, const std::string& user, const std::string& filename);
};
