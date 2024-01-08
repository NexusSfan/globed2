#pragma once
#include <defs.hpp>
#include <defs/net.hpp>
#include <string>

namespace util::net {
    // Initialize all networking libraries (calls `WSAStartup` on Windows, does nothing on other platforms)
    void initialize();

    // Cleanup networking resources (calls `WSACleanup` on Windows, does nothing on other platforms)
    void cleanup();

    // Returns the last network error code (calls `WSAGetLastError` on Windows, grabs `errno` on other platforms)
    int lastErrorCode();

    // Grabs the error code from `lastErrorCode` and formats to a string
    std::string lastErrorString(bool gai = false);

    // Formats an error code to a string
    std::string lastErrorString(int code, bool gai = false);

    // Throws an exception with the message being the value from `lastErrorString()`
    [[noreturn]] void throwLastError(bool gai = false);

    // Returns the user agent for use in web requests
    std::string webUserAgent();

    // Split an address like 127.0.0.1:4343 into pair("127.0.0.1", 4343)
    std::pair<std::string, unsigned short> splitAddress(const std::string_view address, unsigned short defaultPort = 0);

    // Check if two sockaddr structures are equal
    bool sameSockaddr(const sockaddr_in& s1, const sockaddr_in& s2);

    // getaddrinfo
    std::string getaddrinfo(const std::string_view hostname);
}