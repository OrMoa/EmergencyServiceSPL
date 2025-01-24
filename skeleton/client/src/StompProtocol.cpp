
#include "../include/StompProtocol.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <fstream>

using std::string;
using std::vector;
using std::cout;
using std::endl;


StompProtocol::StompProtocol()
    : connectionHandler(nullptr),
      stateMutex(),
      shouldTerminate(false),
      isLoggedIn(false),
      nextReceiptId(0),
      nextSubscriptionId(0),
      currentUsername(""),
      dataMutex(),
      channelToSubId(),
      subIdToChannel(),
      receiptIdToMsg(),
      userChannelEvents() {}


bool StompProtocol::connect(const std::string& host, short port, 
                           const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(stateMutex);
    
    if(isLoggedIn) {
        cout << "Already logged in" << endl;
        return false;
    }
    
    connectionHandler = std::unique_ptr<ConnectionHandler>(new ConnectionHandler(host, port));
    if(!connectionHandler->connect()) {
        cout << "Could not connect to server" << endl;
        connectionHandler.reset();       
        return false;
    }
    
    string frame = createConnectFrame(username, password);
    if(!connectionHandler->sendFrameAscii(frame, '\0')) {
        cout << "Failed to send CONNECT frame" << endl;
        connectionHandler.reset();
        connectionHandler.reset();
        return false;
    }
    
    currentUsername = username;
    cout << "[DEBUG] CONNECT frame sent successfully. Waiting for server response..." << endl;
    return true;
}

void StompProtocol::disconnect() {
    std::lock_guard<std::mutex> lock(stateMutex);
    if(isLoggedIn) {
        string frame = createDisconnectFrame();
        connectionHandler->sendFrameAscii(frame, '\0');
        isLoggedIn = false;
        shouldTerminate = true;
    }
    connectionHandler = nullptr;
}

vector<string> StompProtocol::processInput(const string& input) {
    vector<string> frames;
    vector<string> parts = split(input, ' ');
    if(parts.empty()) return frames;
    
    const string& command = parts[0];
    std::cout << "[DEBUG] Processing command: " << command << std::endl;

    if(command == "login") {
        if(parts.size() < 4) {
            std::cout << "Invalid login command. Usage: login {host:port} {username} {password}" << std::endl;
            return frames;
        }

        std::string hostPort = parts[1];
        std::string username = parts[2];
        std::string password = parts[3];
        
        std::string host;
        short port;
        if(!parseHostPort(hostPort, host, port)) {
            std::cout << "Invalid host:port format" << std::endl;
            return frames;
        }
        
        if(!connect(host, port, username, password)) {
            std::cout << "Could not connect to server" << std::endl;
            return frames;
        }
        return frames;
    }
    
    // All commands below require an active connection
    if(!isConnected()) {
        std::cout << "Not connected to server. Please login first." << std::endl;
        return frames;
    }

    // Handle channel-related commands
    if(command == "join") {
       if(parts.size() < 2) {
        std::cout << "Invalid join command. Usage: join {channel}" << std::endl;
        return frames;
    }

    std::cout << "[DEBUG] About to acquire lock" << std::endl;
    std::lock_guard<std::mutex> lock(dataMutex);
    std::cout << "[DEBUG] Lock acquired" << std::endl;
    
    string channel = parts[1];
    std::cout << "[DEBUG] Processing join command for channel: " << channel << std::endl;

    try {
        if(channelToSubId.count(channel) == 0) {
            std::cout << "[DEBUG] New subscription for channel: " << channel << std::endl;
            int subId = nextSubscriptionId++;
            channelToSubId[channel] = subId;
            subIdToChannel[subId] = channel;
            string receipt = std::to_string(nextReceiptId++);
            receiptIdToMsg[receipt] = "Joined channel " + channel;
            
            std::cout << "[DEBUG] Creating subscribe frame..." << std::endl;
            std::string subscribeFrame = createSubscribeFrame(channel);
            std::cout << "[DEBUG] Created subscribe frame:\n" << subscribeFrame << std::endl;
            frames.push_back(subscribeFrame);
            std::cout << "[DEBUG] Frame added to queue" << std::endl;
        } else {
            std::cout << "[DEBUG] Already subscribed to channel: " << channel << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "[DEBUG] Exception in join handler: " << e.what() << std::endl;
    }
    std::cout << "[DEBUG] Join processing complete" << std::endl;
}

    else if(command == "exit") {
        if(parts.size() < 2) {
            std::cout << "Invalid exit command. Usage: exit {channel}" << std::endl;
            return frames;
        }

        std::lock_guard<std::mutex> lock(dataMutex);
        string channel = parts[1];
        if(channelToSubId.count(channel) > 0) {
            int subId = channelToSubId[channel];
            frames.push_back(createUnsubscribeFrame(subId));
            string receipt = std::to_string(nextReceiptId++);
            receiptIdToMsg[receipt] = "Exited channel " + channel;
            channelToSubId.erase(channel);
            subIdToChannel.erase(subId);
        }
    }
    
    else if(command == "report") {
        if(parts.size() < 2) {
            std::cout << "Invalid report command. Usage: report {json_path}" << std::endl;
            return frames;
        }

        try {
            std::cout << "[DEBUG] Processing report file: " << parts[1] << std::endl;
            names_and_events eventsData = parseEventsFile(parts[1]);

            std::lock_guard<std::mutex> lock(dataMutex);
    
            for(const Event& event : eventsData.events) {
            std::string channel = event.get_channel_name();
            std::cout << "[DEBUG] Creating send frame for channel: " << channel << std::endl;
            saveEventForUser(channel, currentUsername, event);
            std::string frame = createSendFrame(channel, formatEventMessage(event));
            std::cout << "[DEBUG] Created frame:\n" << frame << std::endl;
            frames.push_back(frame);
            }
        }
    catch(const std::exception& e) {
        cout << "Error processing report file: " << e.what() << endl;
        cout << "Make sure the file exists and is in the correct path" << endl;
    }
    }
    
    else if(command == "summary") {
        if(parts.size() < 4) {
            std::cout << "Invalid summary command. Usage: summary {channel} {user} {file}" << std::endl;
            return frames;
        }

        std::lock_guard<std::mutex> lock(dataMutex);
        writeEventSummary(parts[1], parts[2], parts[3]);
    }
    else if(command == "logout") {
        std::cout << "[DEBUG] Creating DISCONNECT frame" << std::endl;
        string frame = createDisconnectFrame();
        frames.push_back(frame);
        std::cout << "[DEBUG] Added DISCONNECT frame to frames vector" << std::endl;
    }
    else {
        std::cout << "Unknown command: " << command << std::endl;
    }

    std::cout << "[DEBUG] Number of frames to send: " << frames.size() << std::endl;
    return frames;
}

void StompProtocol::processResponse(const string& response) {
    vector<string> lines = split(response, '\n');
    if(lines.empty()) return;

    std::cout << "[DEBUG] Processing response. Command: " << lines[0] << std::endl;
    
    if(lines[0] == "CONNECTED") {
        std::lock_guard<std::mutex> lock(stateMutex);
        isLoggedIn = true;
        cout << "Login successful" << endl;
    }
    else if(lines[0] == "ERROR") {
        cout << "Error: " << lines[lines.size()-1] << endl;
        shouldTerminate = true; // Signal threads to stop
        disconnect();
        return;
    }
    else if(lines[0] == "RECEIPT") {
        string receiptId = getHeader("receipt-id", lines);

        std::cout << "[DEBUG] Got RECEIPT with id: " << receiptId << std::endl;

        string msg;
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            if(receiptIdToMsg.count(receiptId) > 0) {
                msg = receiptIdToMsg[receiptId];
                receiptIdToMsg.erase(receiptId);
            }
        }
        
        if(!msg.empty()) {
            if(msg == "disconnect") {
                disconnect();
            }
            cout << msg << endl;
        }
    }
    else if(lines[0] == "MESSAGE") {
        string channel = getHeader("destination", lines);
        string messageBody;
        bool bodyStarted = false;
        
        for(const string& line : lines) {
            if(bodyStarted) {
                messageBody += line + "\n";
            }
            else if(line.empty()) {
                bodyStarted = true;
            }
        }
        
        if(!messageBody.empty()) {
            Event event(messageBody);
            std::lock_guard<std::mutex> lock(dataMutex);
            saveEventForUser(channel, event.getEventOwnerUser(), event);
        }
        else {
        std::cout << "[DEBUG] Message body is empty, skipping event processing." << std::endl;
        }
    }
}

/*void StompProtocol::processKeyboardInput(const std::string& input) {
    std::cout << "[DEBUG] processKeyboardInput: " << input << std::endl;
    
    vector<string> parts = split(input, ' ');
    if(parts.empty()) return;

    const string& command = parts[0];
    if(command == "login" && parts.size() >= 4) {
        std::string hostPort = parts[1];
        std::string username = parts[2];
        std::string password = parts[3];
        
        // Parse host and port
        size_t colonPos = hostPort.find(':');
        if(colonPos == std::string::npos) {
            std::cout << "Invalid host:port format" << std::endl;
            return;
        }
        
        std::string host = hostPort.substr(0, colonPos);
        short port;
        try {
            port = std::stoi(hostPort.substr(colonPos + 1));
        } catch(...) {
            std::cout << "Invalid port number" << std::endl;
            return;
        }
        
        // Try to connect and send CONNECT frame
        if(connect(host, port, username, password)) {
            std::string frame = createConnectFrame(username, password);
            if(!send(frame)) {
                std::cout << "Error sending CONNECT frame" << std::endl;
                disconnect();
            }
        }
        return;
    }
    
    // For all other commands, process normally through processInput
    std::vector<std::string> frames = processInput(input);
    for(const std::string& frame : frames) {
        if(!frame.empty()) {
            std::cout << "[DEBUG] Sending frame:\n" << frame << std::endl;
            if(!send(frame)) {
                std::cout << "Error sending frame" << std::endl;
                disconnect();
                break;
            }
        }
    }
}*/

// Helper methods implementation...
string StompProtocol::createConnectFrame(const string& username, const string& password) {
    std::stringstream frame;
    frame << "CONNECT\n"
          << "accept-version:1.2\n"
          << "host:stomp.cs.bgu.ac.il\n"
          << "login:" << username << "\n"
          << "passcode:" << password << "\n\n";
    return frame.str();
}


std::string StompProtocol::createSubscribeFrame(const std::string& channel) {
    std::stringstream frame;
    std::string receiptId = std::to_string(nextReceiptId);
    receiptIdToMsg[receiptId] = "Joined channel " + channel;

    frame << "SUBSCRIBE\n"
          << "destination:" << channel << "\n"  // Added leading '/' as per examples
          << "id:" << channelToSubId[channel] << "\n"
          << "receipt:" << receiptId << "\n\n";
    
    std::cout << "[DEBUG] Sending SUBSCRIBE frame for channel: " << channel 
              << " with receipt: " << receiptId << std::endl;
              
    return frame.str();
}


string StompProtocol::createUnsubscribeFrame(int subscriptionId) {
    std::stringstream frame;
    frame << "UNSUBSCRIBE\n"
          << "id:" << subscriptionId << "\n"
          << "receipt:" << nextReceiptId << "\n\n";
    return frame.str();
}

string StompProtocol::formatEventMessage(const Event& event) const {
    std::stringstream ss;
    ss << "user: " << currentUsername << "\n"
       << "city: " << event.get_city() << "\n"
       << "event name: " << event.get_name() << "\n"
       << "date time: " << event.get_date_time() << "\n"
       << "general information:\n";
    
    // Add general information fields
    const auto& info = event.get_general_information();
    for(const auto& [key, value] : info) {
        ss << "  " << key << ": " << value << "\n";
    }
    
    ss << "description:\n" << event.get_description() << "\n";
    return ss.str();
}

string StompProtocol::createSendFrame(const string& destination, const string& body) {
    std::stringstream frame;
    frame << "SEND\n"
          << "destination:/" << destination << "\n\n"
          << body;
    return frame.str();
}

string StompProtocol::createDisconnectFrame() {
    std::stringstream frame;
    std::string receiptId = std::to_string(nextReceiptId);
    
    // Save the pending message for this receipt
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        receiptIdToMsg[receiptId] = "disconnect";  // Special message to trigger disconnect
    }
    
    frame << "DISCONNECT\n"
          << "receipt:" << receiptId << "\n\n";
          
    std::cout << "[DEBUG] Sending DISCONNECT frame with receipt: " << receiptId << std::endl;
    return frame.str();
}

bool StompProtocol::send(const string& frame) {
    std::cout << "[DEBUG] StompProtocol::send called with frame:\n" << frame << std::endl;
    
    if (!connectionHandler) {
        std::cout << "[DEBUG] send failed: no connection handler" << std::endl;
        return false;
    }
    
    bool result = connectionHandler->sendFrameAscii(frame, '\0');
    std::cout << "[DEBUG] send result: " << (result ? "success" : "failure") << std::endl;
    return result;
}

bool StompProtocol::receiveFrame(string& frame) {
    return connectionHandler && connectionHandler->getFrameAscii(frame, '\0');
}

// Helper methods implementation
vector<string> StompProtocol::split(const string& s, char delimiter) const {
    vector<string> tokens;
    std::stringstream ss(s);
    string token;
    while(std::getline(ss, token, delimiter)) {
        if(!token.empty()) {
            tokens.push_back(token);
        }
    }
    return tokens;
}

string StompProtocol::getHeader(const string& header, const vector<string>& lines) const {
    for(const string& line : lines) {
        size_t pos = line.find(':');
        if(pos != string::npos) {
            if(line.substr(0, pos) == header) {
                return line.substr(pos + 1);
            }
        }
    }
    return "";
}

void StompProtocol::saveEventForUser(const string& channel, const string& user, const Event& event) {
    std::cout << "[DEBUG] Saving event for user: " << user << " in channel: " << channel << std::endl;

    string key = channel + "_" + user;
    std::cout << "[DEBUG] key saved: " << key <<  std::endl;
    userChannelEvents[key].push_back(event);
}

void StompProtocol::writeEventSummary(const string& channel, const string& user, const string& filename) {

    std::cout << "[DEBUG] writeEventSummary called with channel: " << channel 
              << ", user: " << user << ", filename: " << filename << std::endl;


    std::vector<Event> events;
    string key = channel + "_" + user;
    std::cout << "[DEBUG] Generated key: " << key << std::endl;
    
   
    if(userChannelEvents.count(key) > 0) {
        events = userChannelEvents[key];
        std::cout << "[DEBUG] Found " << events.size() << " events for key: " << key << std::endl;
    } else {
        std::cout << "[DEBUG] No events found for key: " << key << std::endl;
        return;
    }

    std::cout << "[DEBUG] Sorting events" << std::endl;
    // Sort events first by time, then by name
    std::sort(events.begin(), events.end(), 
        [](const Event& a, const Event& b) {
            if(a.get_date_time() != b.get_date_time())
                return a.get_date_time() < b.get_date_time();
            return a.get_name() < b.get_name();
        });
    
    std::ofstream file(filename, std::ios::trunc);
    if(!file.is_open()) {
        std::cout << "Error: Could not open file for writing: " << filename << endl;
        return;
    }

    std::cout << "[DEBUG] Opened file for writing: " << filename << std::endl;

    file << "Channel " << channel << endl;

    // Count statistics
    int totalEvents = events.size();
    int activeEvents = 0;
    int forcesArrived = 0;
    
    for(const Event& event : events) {
        const auto& info = event.get_general_information();
        if(info.at("active") == "true") 
            activeEvents++;
        if(info.at("forces_arrival_at_scene") == "true")
            forcesArrived++;
    }

    std::cout << "[DEBUG] Statistics - Total: " << totalEvents 
              << ", Active: " << activeEvents 
              << ", Forces Arrived: " << forcesArrived << std::endl;
    
    // Write header and stats
    file << "Stats:" << endl;
    file << "Total: " << totalEvents << endl;
    file << "active: " << activeEvents << endl;
    file << "forces arrival at scene: " << forcesArrived << endl << endl;
    
    std::cout << "[DEBUG] Writing event reports" << std::endl;
  
    // Write event reports
    file << "Event Reports:" << endl;
    for(size_t i = 0; i < events.size(); i++) {
        const Event& event = events[i];
        file << "Report_" << (i+1) << ":" << endl;
        file << "city: " << event.get_city() << endl;
        file << "date time: " << formatDateTime(event.get_date_time()) << endl;
        file << "event name: " << event.get_name() << endl;
        
        // Truncate description if needed
        string desc = event.get_description();
        if(desc.length() > 27) {
            desc = desc.substr(0, 27) + "...";
        }
        file << "summary: " << desc << endl << endl;
    }
    std::cout << "[DEBUG] Finished writing event reports" << std::endl;

    file.close();
    std::cout << "[DEBUG] File closed successfully" << std::endl;

}

string StompProtocol::formatDateTime(int epochTime) const {
    time_t time = static_cast<time_t>(epochTime);
    struct tm* timeinfo = localtime(&time);
    char dateStr[80];
    strftime(dateStr, sizeof(dateStr), "%d/%m/%y %H:%M", timeinfo);
    return string(dateStr);
}

bool StompProtocol::isConnected() const {
    return connectionHandler != nullptr && connectionHandler->isConnected();
}

bool StompProtocol::parseHostPort(const std::string& hostPort, std::string& host, short& port) {
    size_t colonPos = hostPort.find(':');
    if(colonPos == std::string::npos) return false;
    
    try {
        host = hostPort.substr(0, colonPos);
        port = std::stoi(hostPort.substr(colonPos + 1));
        return true;
    } catch(...) {
        return false;
    }
}