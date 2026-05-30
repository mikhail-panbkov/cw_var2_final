#pragma once
// Length-prefixed TCP frame IPC (Winsock on Windows, BSD sockets on POSIX).
#include <cstdint>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
   using socket_t = SOCKET;
   inline int sockClose(socket_t s) { return closesocket(s); }
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
   using socket_t = int;
   inline int sockClose(socket_t s) { return ::close(s); }
#endif

constexpr socket_t invalidSock() {
#ifdef _WIN32
    return INVALID_SOCKET;
#else
    return -1;
#endif
}

inline void netInit() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        throw std::runtime_error("WSAStartup failed");
#endif
}

inline void netShutdown() {
#ifdef _WIN32
    WSACleanup();
#endif
}

inline bool recvAll(socket_t s, char* buf, std::size_t n) {
    std::size_t got = 0;
    while (got < n) {
        int r = recv(s, buf + got, static_cast<int>(n - got), 0);
        if (r <= 0) return false;
        got += static_cast<std::size_t>(r);
    }
    return true;
}

inline bool sendAll(socket_t s, const char* buf, std::size_t n) {
    std::size_t sent = 0;
    while (sent < n) {
        int r = send(s, buf + sent, static_cast<int>(n - sent), 0);
        if (r <= 0) return false;
        sent += static_cast<std::size_t>(r);
    }
    return true;
}

inline bool sendFrame(socket_t s, const std::string& payload) {
    auto len = static_cast<std::uint32_t>(payload.size());
    unsigned char hdr[4] = {
        static_cast<unsigned char>((len >> 24) & 0xFF),
        static_cast<unsigned char>((len >> 16) & 0xFF),
        static_cast<unsigned char>((len >> 8) & 0xFF),
        static_cast<unsigned char>(len & 0xFF)
    };
    if (!sendAll(s, reinterpret_cast<const char*>(hdr), 4)) return false;
    if (len == 0) return true;
    return sendAll(s, payload.data(), payload.size());
}

inline bool recvFrame(socket_t s, std::string& out) {
    unsigned char hdr[4];
    if (!recvAll(s, reinterpret_cast<char*>(hdr), 4)) return false;
    auto len = (static_cast<std::uint32_t>(hdr[0]) << 24)
             | (static_cast<std::uint32_t>(hdr[1]) << 16)
             | (static_cast<std::uint32_t>(hdr[2]) << 8)
             |  static_cast<std::uint32_t>(hdr[3]);
    out.assign(len, '\0');
    if (len == 0) return true;
    return recvAll(s, out.data(), len);
}
