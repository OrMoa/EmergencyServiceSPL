#pragma once
#include "../include/StompProtocol.h"
#include <thread>
#include <atomic>

class SocketReader {
private:
    StompProtocol& protocol;
    std::atomic<bool> shouldStop;
    std::thread readerThread;
    void run();

public:
    explicit SocketReader(StompProtocol& protocol);
    void start();
    void stop();
    ~SocketReader();
};