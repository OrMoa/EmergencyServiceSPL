#include <iostream>
#include "../include/keyboardInput.h"
#include <sstream>

KeyboardInput::KeyboardInput(StompProtocol& protocol)
    : protocol(protocol), inputThread(), shouldStop(false) {
        std::cout << "[DEBUG] KeyboardInput initialized." << std::endl;

    }

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
    std::cout << "[DEBUG] KeyboardInput::run started." << std::endl;

    while(!shouldStop && !protocol.shouldStop()) {

        /*std::cout << "[DEBUG] Checking input. shouldStop: " << shouldStop 
                  << ", protocol.shouldStop: " << protocol.shouldStop() 
                  << ", protocol.isConnected: " << protocol.isConnected() << std::endl;*/

        const short bufsize = 1024;
        char buf[bufsize];
        std::cin.getline(buf, bufsize);
        if(shouldStop || protocol.shouldStop()){
            std::cout << "[DEBUG] Stopping KeyboardInput::run." << std::endl;
            break;
        }
        std::string line(buf);
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        if (line.empty()) {
            std::cout << "[DEBUG] Empty line, skipping." << std::endl;
            continue;
        }
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
            std::cout << "[DEBUG] else in keyBoardInput: " << line << std::endl;
            std::vector<std::string> frames = protocol.processInput(line);

            std::cout << "[DEBUG] Got " << frames.size() << " frames to send" << std::endl;

            for(const std::string& frame : frames) {
                if(!frame.empty()) {
                    std::cout << "[DEBUG] Sending frame:\n" << frame << std::endl;
                    if(!protocol.send(frame)) {
                        std::cout << "Error sending frame" << std::endl;
                        protocol.disconnect();
                        shouldStop = true;
                        std::cout << "[DEBUG] shouldStop set to true in keyboardInput." << std::endl;
                        break;
                    }
                    std::cout << "[DEBUG] Frame sent successfully" << std::endl;
                }
            }
        }
    }
}

void KeyboardInput::start() {
    inputThread = std::thread(&KeyboardInput::run, this);
}

void KeyboardInput::stop() {
    // Set the stop flag
    shouldStop = true;
    std::cout << "[DEBUG] shouldStop set to true in stop (keyboardInput)." << std::endl;
    // Force cin to unblock by closing stdin
    pthread_kill(inputThread.native_handle(), SIGINT);
    
    // Wait for the thread to finish
    if (inputThread.joinable()) {
        inputThread.join();
    }
}

KeyboardInput::~KeyboardInput() {
    stop();
}