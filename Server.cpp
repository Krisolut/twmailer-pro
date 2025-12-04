#include "Server.h"

#include "BlacklistManager.h"
#include "ClientSession.h"
#include "LdapAuthenticator.h"
#include "MailStore.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

using namespace std;

// Konstruktor: Port und Spool-Verzeichnis merken
Server::Server(int port, string spoolDir)
    : port_(port), spoolDir_(move(spoolDir)) {}

// TCP-Server-Socket einrichten (binden + listen)
bool Server::setupSocket(int &sockfd) {
    // IPv4, TCP
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return false;
    }

    // SO_REUSEADDR → Port kann nach Restart schneller wieder benutzt werden
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY; // auf allen Interfaces lauschen
    addr.sin_port = htons(static_cast<uint16_t>(port_)); // Port in Netzwerk-Byteorder

    // Socket an Port binden
    if (bind(sockfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return false;
    }

    // In den Listen-Mode gehen, max. 20 Verbindungen in der Queue
    if (listen(sockfd, 20) < 0) {
        perror("listen");
        close(sockfd);
        return false;
    }
    return true;
}

// Haupt-Serverloop
bool Server::run() {
    int serverSock = -1;
    if (!setupSocket(serverSock)) {
        return false;
    }

    // Zentrale Komponenten einmalig anlegen
    MailStore store(spoolDir_);                          // speichert Mails als Dateien
    BlacklistManager blacklist(spoolDir_ + "/blacklist.db"); // IP-Sperren
    LdapAuthenticator authenticator;                     // kümmert sich um LDAP-Login

    cout << "twmailer-server listening on port " << port_
         << ", spool dir: " << spoolDir_ << endl;

    // Endlosschleife: neue Clients annehmen
    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        // Blockiert, bis ein Client sich verbindet
        int clientSock = accept(serverSock,
                                reinterpret_cast<sockaddr *>(&clientAddr),
                                &clientLen);
        if (clientSock < 0) {
            perror("accept");
            continue;
        }

        // IP-Adresse des Clients als String holen
        string clientIp = inet_ntoa(clientAddr.sin_addr);

        // Direkt blocken, wenn IP bereits auf Blacklist
        if (blacklist.isBlacklisted(clientIp)) {
            send(clientSock, "ERR\n", 4, 0);
            close(clientSock);
            continue;
        }

        // Für jede Verbindung ein eigener Thread mit eigener ClientSession
        thread([clientSock, clientIp, &store, &blacklist, &authenticator]() {
            ClientSession session(clientSock, clientIp, store, blacklist, authenticator);
            session.run(); // bearbeitet Kommandos bis zum QUIT oder Verbindungsende
        }).detach(); // Thread loslösen, kein join nötig
    }

    // wird faktisch nie erreicht, da while(true)
    close(serverSock);
    return true;
}
