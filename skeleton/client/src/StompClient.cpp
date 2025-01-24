#include "../include/StompProtocol.h"
#include <thread>
#include <iostream>
#include <sstream>
#include <utility>
#include "../include/keyboardInput.h"


int main(int argc, char *argv[]) {
    std::cout << "[DEBUG] Starting client..." << std::endl;
    StompProtocol protocol;
    std::cout << "[DEBUG] Protocol initialized" << std::endl;
    
    KeyboardInput keyboardInput(protocol);
   
    keyboardInput.start();
    std::cout << "[DEBUG] KeyboardInput thread started" << std::endl;
    
    while(!protocol.shouldStop()) {
            if(protocol.isConnected()) {
                std::string response;
    
                std::cout << "[DEBUG] Attempting to receive frame..." << std::endl;

                if(protocol.receiveFrame(response)) {
                    if(!response.empty()) {
                        std::cout << "[DEBUG] Received response:\n" << response << std::endl;
                        protocol.processResponse(response);
                    }
                } else if(!protocol.shouldStop()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
           
        std::cout << "[DEBUG] Main loop terminated, cleaning up..." << std::endl;
        keyboardInput.stop();
        std::cout << "[DEBUG] KeyboardInput stopped" << std::endl;

    
    return 0; 
}