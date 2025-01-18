#include <iostream>
#include "../include/keyboardInput.h"
#include <sstream>

KeyboardInput::KeyboardInput(StompProtocol& protocol)
    : protocol(protocol), inputThread(), shouldStop(false) {}

bool KeyboardInput::parseHostPort(const std::string& hostPort, std::string& host, short& port) {
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

void KeyboardInput::run() {
    while(!shouldStop && !protocol.shouldStop()) {
        const short bufsize = 1024;
        char buf[bufsize];
        std::cin.getline(buf, bufsize);
        
        if(shouldStop || protocol.shouldStop()) break;
        
        std::string line(buf);
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        if (line.empty()) 
            continue;

        if(command == "login") {
            if(!protocol.isConnected()) {
                std::string hostPort, username, password;
                if(iss >> hostPort >> username >> password) {
                    std::string host;
                    short port;
                    if(parseHostPort(hostPort, host, port)) {
                        if(!protocol.connect(host, port, username, password)) {
                            std::cout << "Could not connect to server" << std::endl;
                        }
                    } else {
                        std::cout << "Invalid host:port format" << std::endl;
                    }
                } else {
                    std::cout << "Invalid login command format" << std::endl;
                }
            } else {
                std::cout << "Client is already logged in" << std::endl;
            }
        }
        else if(!protocol.isConnected()) {
            std::cout << "Client is not logged in" << std::endl;
        }
        else {
            // Process other commands when connected
            std::vector<std::string> frames = protocol.processInput(line);
            for(const std::string& frame : frames) {
                if(!frame.empty()) {
                    if(!protocol.send(frame)) {
                        std::cout << "Error sending frame" << std::endl;
                        protocol.disconnect();
                        shouldStop = true;
                        break;
                    }
                }
            }
        }
    }
}

void KeyboardInput::start() {
    inputThread = std::thread(&KeyboardInput::run, this);
}

void KeyboardInput::stop() {
    shouldStop = true;
    if(inputThread.joinable()) {
        inputThread.join();
    }
}

KeyboardInput::~KeyboardInput() {
    stop();
}