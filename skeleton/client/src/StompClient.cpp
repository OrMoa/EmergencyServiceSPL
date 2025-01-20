#include "../include/StompProtocol.h"
#include <thread>
#include <iostream>
#include <sstream>
#include <utility>
#include "../include/keyboardInput.h"
#include "../include/SocketReader.h"


int main(int argc, char *argv[]) {
    std::cout << "Starting client..." << std::endl;
    StompProtocol protocol;
    std::cout << "Initialized protocol successfully." << std::endl;


    KeyboardInput keyboardInput(protocol);
    SocketReader socketReader(protocol);
    
    // Start threads
    std::cout << "Starting threads..." << std::endl;
    keyboardInput.start();
    socketReader.start();
    
    // Main loop - keep running until protocol indicates we should stop
    while(!protocol.shouldStop()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Clean shutdown
    std::cout << "Starting shutdown..." << std::endl;
    keyboardInput.stop();
    socketReader.stop();
    
    std::cout << "Client terminated." << std::endl;
    return 0; 
}