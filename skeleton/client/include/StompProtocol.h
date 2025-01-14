#pragma once
#include "../include/ConnectionHandler.h"
#include "../include/event.h"
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <vector>

class StompProtocol {
private:
    ConnectionHandler* connectionHandler;
    std::atomic<bool> connected{false};
    std::atomic<bool> shouldTerminate{false};
    std::mutex mutex;
    std::string currentUser;
    std::map<std::string, int> subscriptionIds; // topic -> subscription id
    std::map<std::string, std::vector<Event>> userEvents; // user -> events
    int nextSubscriptionId;
    int nextReceiptId;

    // Helper functions
    std::string generateFrame(const std::string& command, 
                            const std::map<std::string, std::string>& headers,
                            const std::string& body = "");
    int getNextSubscriptionId();
    int getNextReceiptId();
    bool sendFrame(const std::string& frame);
    void saveEvent(const Event& event, const std::string& username);
    std::string convertTimeToString(int timestamp);
    bool processLoginResponse(const std::string& username);
    bool validateLoginInput(const std::string& host_port, const std::string& username, const std::string& password);
    bool handleLogin(const std::string& host_port, const std::string& username, const std::string& password);




public:
    StompProtocol();
    ~StompProtocol();

    StompProtocol(const StompProtocol&) = delete;
    StompProtocol& operator=(const StompProtocol&) = delete;

    // Thread-safe command processing
    bool processKeyboardCommand(const std::string& command);
    bool processServerMessage(const std::string& message);

    // Command handlers - thread safe using mutex
    bool handleLogin(std::string host, short port, std::string username, std::string password);
    bool handleJoin(std::string topic);
    bool handleExit(std::string topic);
    bool handleReport(std::string jsonPath);
    bool handleSummary(std::string channel, std::string user, std::string file);
    bool handleLogout();

    // Getters
    bool isConnected() const { return connected; }
    bool shouldStop() const { return shouldTerminate; }
    ConnectionHandler* getConnectionHandler() { return connectionHandler; }
};