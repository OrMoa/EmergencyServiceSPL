
#include "../include/ConnectionHandler.h"

using boost::asio::ip::tcp;

using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::string;

ConnectionHandler::ConnectionHandler(string host, short port) : host_(host), port_(port), io_service_(),
                                                                socket_(io_service_) {
}

ConnectionHandler::~ConnectionHandler() {
    close();
}

bool ConnectionHandler::connect() {
    std::cout << "Starting connect to "
              << host_ << ":" << port_ << std::endl;
    try {
        tcp::endpoint endpoint(boost::asio::ip::address::from_string(host_), port_); // the server endpoint
        boost::system::error_code error;
        socket_.connect(endpoint, error);
        if (error)
            throw boost::system::system_error(error);
    }
    catch (std::exception &e) {
        std::cerr << "Connection failed (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    std::cout << "[DEBUG] Successfully connected to server" << std::endl;
    return true;
}

bool ConnectionHandler::getBytes(char bytes[], unsigned int bytesToRead) {
    size_t tmp = 0;
    boost::system::error_code error;
    try {
        while (!error && bytesToRead > tmp) {
            tmp += socket_.read_some(boost::asio::buffer(bytes + tmp, bytesToRead - tmp), error);
        }
        if (error)
            throw boost::system::system_error(error);
    } catch (std::exception &e) {
        std::cerr << "recv failed getBytes: (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

bool ConnectionHandler::sendBytes(const char bytes[], int bytesToWrite) {
    int tmp = 0;
    boost::system::error_code error;
    try {
        while (!error && bytesToWrite > tmp) {
            tmp += socket_.write_some(boost::asio::buffer(bytes + tmp, bytesToWrite - tmp), error);
        }
        if (error)
            throw boost::system::system_error(error);
    } catch (std::exception &e) {
        std::cerr << "recv failed in sendBytes: (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    std::cout << "[DEBUG] Successfully sent " << tmp << " bytes" << std::endl;
    return true;
}

bool ConnectionHandler::getLine(std::string &line) {
    return getFrameAscii(line, '\n');
}

bool ConnectionHandler::sendLine(std::string &line) {
    return sendFrameAscii(line, '\n');
}

bool ConnectionHandler::getFrameAscii(std::string &frame, char delimiter) {
    frame.clear();
    char ch;
    // Stop when we encounter the null character.
    // Notice that the null character is not appended to the frame string.
    try {
        do {
            if (!getBytes(&ch, 1)) {
                std::cout << "[DEBUG] Failed to read byte from socket" << std::endl;
                return false;
            }
            if (ch != '\0')
                frame.append(1, ch);
        } while (delimiter != ch && ch != '\0');
        if (frame.length() > 0) {  // Only return true if we actually got something
            std::cout << "[DEBUG] Successfully Read frame: \n" << frame << std::endl;
            return true;
        }
        return false; 
    } catch (std::exception &e) {
        std::cerr << "recv failed2 in getFrameAscii: (Error: " << e.what() << ')' << std::endl;
        return false;
    }
    return true;
}

bool ConnectionHandler::sendFrameAscii(const std::string &frame, char delimiter) {
    std::cout << "[DEBUG] Sending frame:\n" << frame << std::endl;
    bool result = sendBytes(frame.c_str(), frame.length());
    if (!result) {
        std::cout << "[DEBUG] Failed to send frame body" << std::endl;
        return false;
    }
    result = sendBytes(&delimiter, 1);
    if (!result) {
        std::cout << "[DEBUG] Failed to send delimiter" << std::endl;
        return false;
    }
    std::cout << "[DEBUG] Frame sent successfully" << std::endl;
    return true;
}

// Close down the connection properly.
void ConnectionHandler::close() {
    try {
        std::cout << "[DEBUG] Closing connection" << std::endl;
        socket_.close();
    } catch (...) {
        std::cout << "closing failed: connection already closed" << std::endl;
    }
}

bool ConnectionHandler::isConnected() const {
    return socket_.is_open();
}