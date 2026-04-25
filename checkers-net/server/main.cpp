#include "GameServer.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[])
{
    int port = 54000;
    if (argc > 1) port = std::atoi(argv[1]);

    std::cout << "=== Networked Checkers Server ===\n";
    GameServer server(port);
    server.run();   // Blocks forever
    return 0;
}
