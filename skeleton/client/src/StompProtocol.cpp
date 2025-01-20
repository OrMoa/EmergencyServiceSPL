
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
        return false;
    }
    
    currentUsername = username;
    isLoggedIn = true;
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
    
    if(command == "join" && parts.size() >= 2) {
        std::lock_guard<std::mutex> lock(dataMutex);
        string channel = parts[1];
        if(channelToSubId.count(channel) == 0) {
            int subId = nextSubscriptionId++;
            channelToSubId[channel] = subId;
            subIdToChannel[subId] = channel;
            string receipt = std::to_string(nextReceiptId++);
            receiptIdToMsg[receipt] = "Joined channel " + channel;
            frames.push_back(createSubscribeFrame(channel));
        }
    }
    else if(command == "exit" && parts.size() >= 2) {
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
    else if(command == "report" && parts.size() >= 2) {
        try {
            std::cout << "[DEBUG] Processing report file: " << parts[1] << std::endl;
            names_and_events eventsData = parseEventsFile(parts[1]);
            for(const Event& event : eventsData.events) {
                frames.push_back(createSendFrame(event.get_channel_name(), 
                    formatEventMessage(event)));
            }
        }
        catch(const std::exception& e) {
            cout << "Error processing report file: " << e.what() << endl;
        }
    }
    else if(command == "summary" && parts.size() >= 4) {
        std::lock_guard<std::mutex> lock(dataMutex);
        writeEventSummary(parts[1], parts[2], parts[3]);
    }
    else if(command == "logout") {
        std::cout << "[DEBUG] Creating DISCONNECT frame" << std::endl;
        string frame = createDisconnectFrame();
        frames.push_back(frame);
        std::cout << "[DEBUG] Added DISCONNECT frame to frames vector" << std::endl;
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
        disconnect();
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
    }
}

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

/*string StompProtocol::createSubscribeFrame(const string& channel) {
    std::stringstream frame;
    std::string receiptId = std::to_string(nextReceiptId);
    frame << "SUBSCRIBE\n"
          << "destination:" << channel << "\n"
          << "id:" << channelToSubId[channel] << "\n"
          << "receipt:" << nextReceiptId << "\n\n";
    return frame.str();
}*/

std::string StompProtocol::createSubscribeFrame(const std::string& channel) {
    std::stringstream frame;
    std::string receiptId = std::to_string(nextReceiptId);
    
    // Save the pending message for this receipt
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        receiptIdToMsg[receiptId] = "Joined channel " + channel;
    }
    
    frame << "SUBSCRIBE\n"
          << "destination:/" << channel << "\n"  // Added leading '/' as per examples
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
          << "destination:" << destination << "\n\n"
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
    string key = channel + "_" + user;
    userChannelEvents[key].push_back(event);
}

void StompProtocol::writeEventSummary(const string& channel, const string& user, const string& filename) {
    std::vector<Event> events;
    string key = channel + "_" + user;
    
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        if(userChannelEvents.count(key) > 0) {
            events = userChannelEvents[key];
        }
    }
    
    std::ofstream file(filename);
    if(!file.is_open()) {
        cout << "Error: Could not open file for writing: " << filename << endl;
        return;
    }
    
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
    
    // Write header and stats
    file << "Channel " << channel << endl;
    file << "Stats:" << endl;
    file << "Total: " << totalEvents << endl;
    file << "active: " << activeEvents << endl;
    file << "forces arrival at scene: " << forcesArrived << endl << endl;
    
    // Sort events by time and name
    std::sort(events.begin(), events.end(), 
        [](const Event& a, const Event& b) {
            if(a.get_date_time() != b.get_date_time())
                return a.get_date_time() < b.get_date_time();
            return a.get_name() < b.get_name();
        });
    
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
    
    file.close();
}

string StompProtocol::formatDateTime(int epochTime) const {
    time_t time = static_cast<time_t>(epochTime);
    struct tm* timeinfo = localtime(&time);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%d/%m/%y %H:%M", timeinfo);
    return string(buffer);
}

bool StompProtocol::isConnected() const {
    return connectionHandler != nullptr && connectionHandler->isConnected();
}
