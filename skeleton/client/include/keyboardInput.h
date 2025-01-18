#pragma once
#include "../include/StompProtocol.h"
#include <thread>
#include <atomic>
#include <string>

class KeyboardInput {
private:
    StompProtocol& protocol;
    std::thread inputThread;
    std::atomic<bool> shouldStop;
    void run();
    
    // Helper function to parse host:port string
    bool parseHostPort(const std::string& hostPort, std::string& host, short& port);

public:
    explicit KeyboardInput(StompProtocol& protocol);
    void start();
    void stop();
    ~KeyboardInput();
};
