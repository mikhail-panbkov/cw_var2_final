#include "engine.hpp"
#include "net.hpp"

#include <iostream>

// setsockopt's `optval` parameter uses `const char*` on Windows and `const void*`
// on POSIX. A small wrapper hides the cast site in one place.
namespace {
template <class T>
int setSockOptBool(socket_t s, int level, int name, T value) {
    return setsockopt(s, level, name, reinterpret_cast<const char*>(&value), sizeof(value));
}
}

int main(int argc, char** argv) {
    if (argc > 3) { std::cerr << "Usage: cwdb-server [host] [port]\n"; return 2; }
    std::string host = (argc >= 2) ? argv[1] : "127.0.0.1";
    int port = 5429;
    if (argc >= 3) {
        try {
            int p = std::stoi(argv[2]);
            if (p <= 0 || p > 65535) {
                std::cerr << "invalid port: " << argv[2] << "\n";
                return 2;
            }
            port = p;
        } catch (...) {
            std::cerr << "invalid port: " << argv[2] << "\n";
            return 2;
        }
    }

    netInit();

    socket_t lst = socket(AF_INET, SOCK_STREAM, 0);
    if (lst == invalidSock()) { std::cerr << "socket failed\n"; netShutdown(); return 1; }
    setSockOptBool(lst, SOL_SOCKET, SO_REUSEADDR, 1);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "invalid host: " << host << "\n"; sockClose(lst); netShutdown(); return 2;
    }
    if (bind(lst, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::cerr << "bind failed on " << host << ":" << port << "\n";
        sockClose(lst); netShutdown(); return 1;
    }
    if (listen(lst, 8) != 0) {
        std::cerr << "listen failed\n";
        sockClose(lst); netShutdown(); return 1;
    }

    std::cout << "cwdb-server variant 2 (B+-tree) listening on " << host << ":" << port << "\n";
    std::cout << "Press Ctrl+C to stop.\n";

    Engine engine;

    for (;;) {
        sockaddr_in cli{};
#ifdef _WIN32
        int clen = sizeof(cli);
#else
        socklen_t clen = sizeof(cli);
#endif
        socket_t c = accept(lst, reinterpret_cast<sockaddr*>(&cli), &clen);
        if (c == invalidSock()) continue;

        char ipbuf[64] = {0};
        inet_ntop(AF_INET, &cli.sin_addr, ipbuf, sizeof(ipbuf));
        std::cout << "[server] client connected " << ipbuf << ":" << ntohs(cli.sin_port) << "\n";

        for (;;) {
            std::string cmd;
            if (!recvFrame(c, cmd)) break;
            std::string resp = engine.exec(cmd);
            if (!sendFrame(c, resp)) break;
            if (engine.sessionEnded) {
                engine.sessionEnded = false;
                break;
            }
        }
        sockClose(c);
        std::cout << "[server] client disconnected\n";
    }
}
