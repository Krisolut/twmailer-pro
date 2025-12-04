#include <cstdlib>
#include <iostream>
#include <string>

#include "Server.h"

using namespace std;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: ./twmailer-server <port> <mail-spool-directory>\n";
        return 1;
    }

    int port = atoi(argv[1]);
    string spoolDir = argv[2];

    Server server(port, spoolDir);
    if (!server.run()) {
        cerr << "Server konnte nicht gestartet werden." << endl;
        return 1;
    }

    return 0;
}
