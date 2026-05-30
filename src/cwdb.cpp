#include "engine.hpp"

int main(int argc, char** argv) {
    Engine e;
    try {
        if (argc > 2) { std::cerr << "Usage: cwdb [script.sql]\n"; return 2; }
        if (argc == 2) {
            std::ifstream f(argv[1]);
            if (!f) { std::cerr << "cannot open script: " << argv[1] << "\n"; return 2; }
            for (const auto& c : readCommands(f)) {
                std::cout << e.exec(c) << "\n";
                if (e.sessionEnded) break;
            }
            return 0;
        }
        std::cout << "cwdb variant 2 (B+-tree, standalone). End commands with ;  (EXIT; to quit)\n";
        std::string cur, line;
        bool inBlockComment = false;
        while (!e.sessionEnded && std::cout << "> " && std::getline(std::cin, line)) {
            cur += stripSqlComments(line + "\n", inBlockComment);
            size_t p;
            while ((p = findCommandEnd(cur)) != std::string::npos) {
                std::cout << e.exec(cur.substr(0, p + 1)) << "\n";
                cur.erase(0, p + 1);
                if (e.sessionEnded) break;
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "Fatal error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
