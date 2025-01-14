#include "../include/StompProtocol.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <ctime>
#include <algorithm>

StompProtocol::StompProtocol()
    : connectionHandler(nullptr),
      connected(false),
      shouldTerminate(false),
      mutex(),
      currentUser(""),
      subscriptionIds(),
      userEvents(),
      nextSubscriptionId(0),
      nextReceiptId(0) {}


StompProtocol::~StompProtocol() {
    if (connectionHandler != nullptr) {
        delete connectionHandler;
    }
}

bool StompProtocol::processKeyboardCommand(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex);
    std::stringstream ss(line);
    std::string command;
    ss >> command;

    if (command == "login") {
        if (connected) {
            std::cout << "The client is already logged in, log out before trying again" << std::endl;
            return false;
        }
        std::string host_port, username, password;
        ss >> host_port >> username >> password;

       if (!validateLoginInput(host_port, username, password)) {
            return false;
        }

         size_t colon_pos = host_port.find(':');
        std::string host = host_port.substr(0, colon_pos);
        short port = 0;
        try {
            port = static_cast<short>(std::stoi(host_port.substr(colon_pos + 1)));
        } catch (...) {
            std::cout << "Invalid port number" << std::endl;
            return false;
        }

        return handleLogin(host, port, username, password);
    }
    /*else if (!connected) {
        std::cout << "Client is not logged in, please login first" << std::endl;
        return false;
    }*/
    else if (command == "join") {
        if(!isConnected){
            std::cout << "Client is not logged in, please login first" << std::endl;
            return false;
        }
        std::string topic;
        ss >> topic;
        return handleJoin(topic);
    }
    else if (command == "exit") {
        if(!isConnected){
            std::cout << "Client is not logged in, please login first" << std::endl;
            return false;
        }

        std::string topic;
        ss >> topic;
        return handleExit(topic);
    }
    else if (command == "report") {
        if(!isConnected){
            std::cout << "Client is not logged in, please login first" << std::endl;
            return false;
        }

        std::string jsonPath;
        ss >> jsonPath;
        return handleReport(jsonPath);
    }
    else if (command == "summary") {
        if(!isConnected){
            std::cout << "Client is not logged in, please login first" << std::endl;
            return false;
        }

        std::string channel, user, file;
        ss >> channel >> user >> file;
        return handleSummary(channel, user, file);
    }
    else if (command == "logout") {
        if(!isConnected){
            std::cout << "Client is not logged in, please login first" << std::endl;
            return false;
        }

        return handleLogout();
    }
    
    return false;
}

bool StompProtocol::processServerMessage(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex);
    
    if (msg.find("CONNECTED") == 0) {
        connected = true;
        std::cout << "Login successful" << std::endl;
    }
    else if (msg.find("RECEIPT") == 0) {
        // Handle receipt - check if it's for logout
        if (msg.find("receipt-id:logout") != std::string::npos) {
            shouldTerminate = true;
            connected = false;
        }
        // Add receipt handling for join/exit if needed
    }
    else if (msg.find("ERROR") == 0) {
        std::cout << "Error received: " << msg << std::endl;
        if (!connected) {
            delete connectionHandler;
            connectionHandler = nullptr;
        }
        return false;
    }
    else if (msg.find("MESSAGE") == 0) {
        Event event(msg);
        saveEvent(event, event.getEventOwnerUser());
    }
    
    return true;
}
bool StompProtocol::validateLoginInput(const std::string& host_port, const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        std::cout << "Username and password cannot be empty" << std::endl;
        return false;
    }

    size_t colon_pos = host_port.find(':');
    if (colon_pos == std::string::npos) {
        std::cout << "Invalid host:port format" << std::endl;
        return false;
    }


    return true;
}

bool StompProtocol::handleLogin(const std::string& host_port, const std::string& username, const std::string& password) {
    size_t colon_pos = host_port.find(':');
    std::string host = host_port.substr(0, colon_pos);
    short port = std::stoi(host_port.substr(colon_pos + 1));

    connectionHandler = new ConnectionHandler(host, port);
    if (!connectionHandler->connect()) {
        std::cout << "Could not connect to server" << std::endl;
        delete connectionHandler;
        connectionHandler = nullptr;
        return false;
    }

    std::map<std::string, std::string> headers;
    headers["accept-version"] = "1.2";
    headers["host"] = "stomp.cs.bgu.ac.il";
    headers["login"] = username;
    headers["passcode"] = password;

    std::string frame = generateFrame("CONNECT", headers);
    if (!sendFrame(frame)) {
        std::cout << "Failed to send CONNECT frame" << std::endl;
        delete connectionHandler;
        connectionHandler = nullptr;
        return false;
    }

    currentUser = username;
    return processLoginResponse(username);
}

bool StompProtocol::processLoginResponse(const std::string& username) {
    std::string serverResponse;
    if (!connectionHandler->getLine(serverResponse)) {
        std::cout << "Connection lost during login" << std::endl;
        delete connectionHandler;
        connectionHandler = nullptr;
        return false;
    }

    if (serverResponse.find("CONNECTED") == 0) {
        std::cout << "Login successful" << std::endl;
        connected = true;
        currentUser = username;
        return true;
    } else if (serverResponse.find("ERROR") == 0) {
        if (serverResponse.find("Wrong password") != std::string::npos) {
            std::cout << "Wrong password" << std::endl;
        } else if (serverResponse.find("User already logged in") != std::string::npos) {
            std::cout << "User already logged in" << std::endl;
        } else {
            std::cout << "Login failed: " << serverResponse << std::endl;
        }
        return false;
    }

    std::cout << "Unexpected response: " << serverResponse << std::endl;
    return false;
}


bool StompProtocol::handleJoin(std::string topic) {
    int subId = getNextSubscriptionId();
    std::string receiptId = "join-" + std::to_string(getNextReceiptId());

    std::map<std::string, std::string> headers;
    headers["destination"] = "/" + topic;
    headers["id"] = std::to_string(subId);
    headers["receipt"] = receiptId;

    std::string frame = generateFrame("SUBSCRIBE", headers);
    if (!sendFrame(frame)) {
        return false;
    }

    subscriptionIds[topic] = subId;
    std::cout << "Joined channel " << topic << std::endl;
    return true;
}

bool StompProtocol::handleExit(std::string topic) {
    auto it = subscriptionIds.find(topic);
    if (it == subscriptionIds.end()) {
        std::cout << "Not subscribed to channel " << topic << std::endl;
        return false;
    }

    std::string receiptId = "exit-" + std::to_string(getNextReceiptId());
    
    std::map<std::string, std::string> headers;
    headers["id"] = std::to_string(it->second);
    headers["receipt"] = receiptId;

    std::string frame = generateFrame("UNSUBSCRIBE", headers);
    if (!sendFrame(frame)) {
        return false;
    }

    subscriptionIds.erase(it);
    std::cout << "Exited channel " << topic << std::endl;
    return true;
}

bool StompProtocol::handleReport(std::string jsonPath) {
    names_and_events events = parseEventsFile(jsonPath);
    
    for (const Event& event : events.events) {
        std::map<std::string, std::string> headers;
        headers["destination"] = "/" + events.channel_name;

        // Create the event body according to the format specified in the PDF
        std::stringstream body;
        body << "user: " << currentUser << "\n";
        body << "event name: " << event.get_name() << "\n";
        body << "date time: " << event.get_date_time() << "\n";
        body << "description: " << event.get_description() << "\n";
        body << "general information:\n";
        for (const auto& info : event.get_general_information()) {
            body << "  " << info.first << ": " << info.second << "\n";
        }

        std::string frame = generateFrame("SEND", headers, body.str());
        if (!sendFrame(frame)) {
            return false;
        }

        saveEvent(event, currentUser);
    }

    return true;
}

bool StompProtocol::handleSummary(std::string channel, std::string user, std::string file) {
    std::ofstream outFile(file);
    if (!outFile.is_open()) {
        std::cout << "Failed to open file: " << file << std::endl;
        return false;
    }

    // Write channel name
    outFile << "Channel " << channel << std::endl;

    // Collect and sort relevant events
    std::vector<Event> relevantEvents;
    int totalEvents = 0, activeEvents = 0, forceEvents = 0;

    auto userEventsIt = userEvents.find(user);
    if (userEventsIt != userEvents.end()) {
        for (const Event& event : userEventsIt->second) {
            if (event.get_channel_name() == channel) {
                relevantEvents.push_back(event);
                totalEvents++;
                
                const auto& info = event.get_general_information();
                if (info.find("active") != info.end() && info.at("active") == "true") {
                    activeEvents++;
                }
                if (info.find("forces_arrival_at_scene") != info.end() && 
                    info.at("forces_arrival_at_scene") == "true") {
                    forceEvents++;
                }
            }
        }
    }

    // Write stats
    outFile << "Stats:" << std::endl;
    outFile << "Total: " << totalEvents << std::endl;
    outFile << "active: " << activeEvents << std::endl;
    outFile << "forces arrival at scene: " << forceEvents << std::endl << std::endl;

    // Sort events by time and name
    std::sort(relevantEvents.begin(), relevantEvents.end(), 
        [](const Event& a, const Event& b) {
            if (a.get_date_time() != b.get_date_time())
                return a.get_date_time() < b.get_date_time();
            return a.get_name() < b.get_name();
    });

    // Write events
    outFile << "Event Reports:" << std::endl;
    int reportNum = 1;
    for (const Event& event : relevantEvents) {
        outFile << "Report_" << reportNum++ << ":" << std::endl;
        outFile << "city: " << event.get_city() << std::endl;
        outFile << "date time: " << convertTimeToString(event.get_date_time()) << std::endl;
        outFile << "event name: " << event.get_name() << std::endl;
        
        // Truncate description if needed
        std::string desc = event.get_description();
        if (desc.length() > 27) {
            desc = desc.substr(0, 27) + "...";
        }
        outFile << "summary: " << desc << std::endl << std::endl;
    }

    outFile.close();
    return true;
}

bool StompProtocol::handleLogout() {
    std::string receiptId = "logout";
    
    std::map<std::string, std::string> headers;
    headers["receipt"] = receiptId;

    std::string frame = generateFrame("DISCONNECT", headers);
    if (!sendFrame(frame)) {
        return false;
    }

    return true;
}

std::string StompProtocol::generateFrame(const std::string& command,
                                       const std::map<std::string, std::string>& headers,
                                       const std::string& body) {
    std::stringstream frame;
    frame << command << "\n";
    
    for (const auto& header : headers) {
        frame << header.first << ":" << header.second << "\n";
    }
    
    frame << "\n";
    if (!body.empty()) {
        frame << body;
    }
    frame << '\0';
    
    return frame.str();
}

bool StompProtocol::sendFrame(const std::string& frame) {
    return connectionHandler->sendFrameAscii(frame, '\0');
}

void StompProtocol::saveEvent(const Event& event, const std::string& username) {
    userEvents[username].push_back(event);
}

std::string StompProtocol::convertTimeToString(int timestamp) {
    time_t time = timestamp;
    tm* ltm = localtime(&time);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << ltm->tm_mday << "/"
       << std::setfill('0') << std::setw(2) << (ltm->tm_mon + 1) << "/"
       << std::setfill('0') << std::setw(2) << (ltm->tm_year % 100) << " "
       << std::setfill('0') << std::setw(2) << ltm->tm_hour << ":"
       << std::setfill('0') << std::setw(2) << ltm->tm_min;
    return ss.str();
}

int StompProtocol::getNextSubscriptionId() {
    return nextSubscriptionId++;
}

int StompProtocol::getNextReceiptId() {
    return nextReceiptId++;
}