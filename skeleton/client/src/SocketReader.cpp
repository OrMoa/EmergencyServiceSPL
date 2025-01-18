#include "../include/SocketReader.h"
#include <iostream>

SocketReader::SocketReader(StompProtocol& protocol)
    : protocol(protocol), shouldStop(false), readerThread() {}


void SocketReader::run() {
    while(!shouldStop && !protocol.shouldStop()) {
        std::string response;
        if(!protocol.receiveFrame(response)) {
            if(!protocol.shouldStop()) {  // Only print if not stopping normally
                std::cout << "Connection error" << std::endl;
                protocol.disconnect();
            }
            break;
        }
        
        if(!response.empty()) {
            protocol.processResponse(response);
            
            // Check if we need to stop after processing response
            if(protocol.shouldStop()) {
                shouldStop = true;
                break;
            }
        }
    }
}

void SocketReader::start() {
    readerThread = std::thread(&SocketReader::run, this);
}

void SocketReader::stop() {
    shouldStop = true;
    if(readerThread.joinable()) {
        readerThread.join();
    }
}

SocketReader::~SocketReader() {
    stop();
}