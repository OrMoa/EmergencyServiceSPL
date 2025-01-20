#include "../include/SocketReader.h"
#include <iostream>

SocketReader::SocketReader(StompProtocol& protocol)
    : protocol(protocol), shouldStop(false), readerThread() {}


void SocketReader::run() {
    std::cout << "[DEBUG] SocketReader::run started." << std::endl;
    while(!shouldStop && !protocol.shouldStop()) {

        if (!protocol.isConnected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::string response;
        std::cout << "[DEBUG] SocketReader: Attempting to receive frame..." << std::endl;

        if(!protocol.receiveFrame(response)) {
            if(!protocol.shouldStop()) {  // Only print if not stopping normally
                std::cout << "Connection error" << std::endl;
                std::cout << "[DEBUG] SocketReader: Connection error in receiveFrame" << std::endl;
                protocol.disconnect();
            }
            break;
        }
        
        if(!response.empty()) {
            std::cout << "[DEBUG] SocketReader: Received response: " << response << std::endl;
            protocol.processResponse(response);
            
            // Check if we need to stop after processing response
            if(protocol.shouldStop()) {
                std::cout << "[DEBUG] shouldStop set to true." << std::endl;
                shouldStop = true;
                break;
            }
        }
    }
    std::cout << "[DEBUG] SocketReader::run ended." << std::endl;
}

void SocketReader::start() {
    readerThread = std::thread(&SocketReader::run, this);
}

void SocketReader::stop() {
    // Set the stop flag
    shouldStop = true;
    std::cout << "[DEBUG] shouldStop set to true in stop (SocketReader)." << std::endl;
    // Force cin to unblock by closing stdin
    pthread_kill(readerThread.native_handle(), SIGINT);
    
    // Wait for the thread to finish
    if (readerThread.joinable()) {
        readerThread.join();
    }
}

SocketReader::~SocketReader() {
    stop();
}