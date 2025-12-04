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

Server::Server(int port, std::string spoolDir)
    : port_(port), spoolDir_(std::move(spoolDir)) {}

bool Server::setupSocket(int &sockfd) {
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return false;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<uint16_t>(port_));

    if (bind(sockfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return false;
    }

    if (listen(sockfd, 20) < 0) {
        perror("listen");
        close(sockfd);
        return false;
    }
    return true;
}

bool Server::run() {
    int serverSock = -1;
    if (!setupSocket(serverSock)) {
        return false;
    }

    MailStore store(spoolDir_);
    BlacklistManager blacklist(spoolDir_ + "/blacklist.db");
    LdapAuthenticator authenticator;

    std::cout << "twmailer-server listening on port " << port_
              << ", spool dir: " << spoolDir_ << std::endl;

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int clientSock = accept(serverSock, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
        if (clientSock < 0) {
            perror("accept");
            continue;
        }

        std::string clientIp = inet_ntoa(clientAddr.sin_addr);

        if (blacklist.isBlacklisted(clientIp)) {
            send(clientSock, "ERR\n", 4, 0);
            close(clientSock);
            continue;
        }

        std::thread([clientSock, clientIp, &store, &blacklist, &authenticator]() {
            ClientSession session(clientSock, clientIp, store, blacklist, authenticator);
            session.run();
        }).detach();
    }

    close(serverSock);
    return true;
}
