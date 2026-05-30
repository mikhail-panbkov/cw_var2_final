#include "engine.hpp"
#include "net.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
struct Args {
    std::string host = "127.0.0.1";
    int port = 5429;
    std::string script;
};

void usage() {
    std::cerr << "Usage: cwdb-client [--host H] [--port P] [script.sql]\n";
}

// Returns parsed args, or throws std::invalid_argument on bad usage so that
// main can return a non-zero exit code through normal stack unwinding.
Args parseArgs(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s = argv[i];
        if (s == "--host" && i + 1 < argc) { a.host = argv[++i]; continue; }
        if (s == "--port" && i + 1 < argc) {
            int p = 0;
            try { p = std::stoi(argv[++i]); }
            catch (...) { throw std::invalid_argument("invalid --port value"); }
            if (p <= 0 || p > 65535) throw std::invalid_argument("--port out of range");
            a.port = p;
            continue;
        }
        if (!s.empty() && s[0] == '-') throw std::invalid_argument("unknown option: " + s);
        if (!a.script.empty()) throw std::invalid_argument("extra positional argument: " + s);
        a.script = s;
    }
    return a;
}

socket_t connectTo(const std::string& host, int port) {
    socket_t s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == invalidSock()) throw std::runtime_error("socket failed");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        sockClose(s);
        throw std::runtime_error("invalid host: " + host);
    }
    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        sockClose(s);
        throw std::runtime_error("connect failed to " + host + ":" + std::to_string(port));
    }
    return s;
}

bool roundTrip(socket_t s, const std::string& cmd, std::string& resp) {
    if (!sendFrame(s, cmd)) return false;
    if (!recvFrame(s, resp)) return false;
    return true;
}

bool isExitResponse(const std::string& cmd) {
    Lexer lx(cmd);
    if (!lx.has()) return false;
    std::string t = upper(lx.peek());
    return t == "EXIT" || t == "QUIT";
}
}

int main(int argc, char** argv) {
    Args a;
    try {
        a = parseArgs(argc, argv);
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << "\n";
        usage();
        return 2;
    }
    netInit();
    socket_t s;
    try {
        s = connectTo(a.host, a.port);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        netShutdown();
        return 1;
    }

    int rc = 0;
    try {
        if (!a.script.empty()) {
            std::ifstream f(a.script);
            if (!f) { std::cerr << "cannot open script: " << a.script << "\n"; sockClose(s); netShutdown(); return 2; }
            for (const auto& c : readCommands(f)) {
                std::string resp;
                if (!roundTrip(s, c, resp)) { std::cerr << "connection lost\n"; rc = 1; break; }
                std::cout << resp << "\n";
                if (isExitResponse(c)) break;
            }
        } else {
            std::cout << "cwdb-client variant 2 -> " << a.host << ":" << a.port
                      << ". End commands with ;  (EXIT; to quit)\n";
            std::string cur, line;
            bool inBlockComment = false;
            bool done = false;
            while (!done && std::cout << "> " && std::getline(std::cin, line)) {
                cur += stripSqlComments(line + "\n", inBlockComment);
                size_t p;
                while ((p = findCommandEnd(cur)) != std::string::npos) {
                    std::string cmd = cur.substr(0, p + 1);
                    cur.erase(0, p + 1);
                    std::string resp;
                    if (!roundTrip(s, cmd, resp)) { std::cerr << "connection lost\n"; rc = 1; done = true; break; }
                    std::cout << resp << "\n";
                    if (isExitResponse(cmd)) { done = true; break; }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "client error: " << e.what() << "\n";
        rc = 1;
    }

    sockClose(s);
    netShutdown();
    return rc;
}
