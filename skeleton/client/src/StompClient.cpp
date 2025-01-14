#include "../include/StompProtocol.h"
#include <thread>
#include <iostream>
#include <sstream>

void readFromKeyboard(StompProtocol& protocol) {
    const short bufsize = 1024;
    char buf[bufsize];
    while (!protocol.shouldStop()) {
        std::cin.getline(buf, bufsize);
        std::string line(buf);
        
        if (!protocol.processKeyboardCommand(line)) {
            std::cout << "Error processing command" << std::endl;
            if (line == "logout") {
                break;
            }
        }
    }
}

void readFromSocket(StompProtocol& protocol) {
    while (!protocol.shouldStop()) {
        if (!protocol.isConnected() || protocol.getConnectionHandler() == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::string answer;
        if (!protocol.getConnectionHandler()->getLine(answer)) {
            std::cout << "Connection closed" << std::endl;
            break;
        }

        if (!protocol.processServerMessage(answer)) {
            break;
        }
    }
}

int main(int argc, char *argv[]) {
    // Create the protocol instance
    StompProtocol protocol;
    
    // Create threads for keyboard and socket reading
    std::thread keyboardThread(readFromKeyboard, std::ref(protocol));
    std::thread socketThread(readFromSocket, std::ref(protocol));
    
    // Wait for both threads to finish
    keyboardThread.join();
    socketThread.join();
    
    return 0;
}