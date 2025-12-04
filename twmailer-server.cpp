#include <cstdlib>
#include <iostream>
#include <string>

#include "Server.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./twmailer-server <port> <mail-spool-directory>\n";
        return 1;
    }

    int port = std::atoi(argv[1]);
    std::string spoolDir = argv[2];

    Server server(port, spoolDir);
    if (!server.run()) {
        std::cerr << "Server konnte nicht gestartet werden." << std::endl;
        return 1;
    }

    return 0;
}
