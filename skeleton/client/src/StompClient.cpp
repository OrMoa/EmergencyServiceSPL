#include "../include/StompProtocol.h"
#include <thread>
#include <iostream>
#include <sstream>
#include "../include/keyboardInput.h"
#include "../include/SocketReader.h"

int main(int argc, char *argv[]) {
    std::cout << "Starting client..." << std::endl;
    StompProtocol protocol;
    std::cout << "Initialized protocol successfully." << std::endl;
    
    KeyboardInput keyboardInput(protocol);
    SocketReader socketReader(protocol);
    
    keyboardInput.start();
    socketReader.start();
    
    keyboardInput.stop();
    socketReader.stop();
        
    return 0;
}