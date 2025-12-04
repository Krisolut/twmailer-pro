#include "ClientSession.h"

#include "BlacklistManager.h"
#include "LdapAuthenticator.h"
#include "MailStore.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {
constexpr size_t MAX_SUBJECT = 80;
}

ClientSession::ClientSession(int socketFD,
                             std::string clientIp,
                             MailStore &store,
                             BlacklistManager &blacklist,
                             LdapAuthenticator &authenticator)
    : sockfd_(socketFD),
      clientIp_(std::move(clientIp)),
      store_(store),
      blacklist_(blacklist),
      authenticator_(authenticator) {}

bool ClientSession::sendAll(const std::string &data) const {
    const char *buf = data.c_str();
    size_t total = 0;
    size_t len = data.size();

    while (total < len) {
        ssize_t n = send(sockfd_, buf + total, len - total, 0);
        if (n <= 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

bool ClientSession::recvLine(std::string &line) const {
    line.clear();
    char c;

    while (true) {
        ssize_t n = recv(sockfd_, &c, 1, 0);
        if (n <= 0) {
            return false;
        }

        if (c == '\n') {
            break;
        }
        line.push_back(c);
    }

    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    return true;
}

bool ClientSession::handleLogin() {
    std::string user;
    std::string pass;
    if (!recvLine(user) || !recvLine(pass)) {
        return false;
    }

    if (blacklist_.isBlacklisted(clientIp_)) {
        sendAll("ERR\n");
        return true;
    }

    if (authenticator_.authenticate(user, pass)) {
        authenticated_ = true;
        username_ = user;
        blacklist_.recordSuccess(clientIp_, user);
        sendAll("OK\n");
    } else {
        bool banned = blacklist_.recordFailure(clientIp_, user);
        sendAll("ERR\n");
        if (banned) {
            std::cerr << "IP " << clientIp_ << " gesperrt nach Fehlversuchen" << std::endl;
        }
    }
    return true;
}

void ClientSession::handleSend() {
    if (!authenticated_) {
        sendAll("ERR\n");
        return;
    }

    std::string receiver;
    std::string subject;

    if (!recvLine(receiver) || !recvLine(subject)) {
        return;
    }

    if (subject.size() > MAX_SUBJECT) {
        subject = subject.substr(0, MAX_SUBJECT);
    }

    std::string body;
    while (true) {
        std::string line;
        if (!recvLine(line)) {
            return;
        }
        if (line == ".") {
            break;
        }
        body += line + "\n";
    }

    bool ok = store_.storeMessage(username_, receiver, subject, body);
    sendAll(ok ? "OK\n" : "ERR\n");
}

void ClientSession::handleList() {
    if (!authenticated_) {
        sendAll("ERR\n");
        return;
    }

    std::vector<std::string> subjects;
    store_.listMessages(username_, subjects);

    std::string resp = std::to_string(subjects.size()) + "\n";
    for (const auto &s : subjects) {
        resp += s + "\n";
    }
    sendAll(resp);
}

void ClientSession::handleRead() {
    if (!authenticated_) {
        sendAll("ERR\n");
        return;
    }

    std::string msgNumStr;
    if (!recvLine(msgNumStr)) {
        return;
    }

    int msgNum = std::atoi(msgNumStr.c_str());
    std::string sender, receiver, subject, body;

    if (!store_.readMessage(username_, msgNum, sender, receiver, subject, body)) {
        sendAll("ERR\n");
        return;
    }

    std::string resp = "OK\n";
    resp += sender + "\n";
    resp += receiver + "\n";
    resp += subject + "\n";
    if (!body.empty()) {
        resp += body;
        if (resp.back() != '\n') {
            resp += "\n";
        }
    }
    resp += ".\n";

    sendAll(resp);
}

void ClientSession::handleDelete() {
    if (!authenticated_) {
        sendAll("ERR\n");
        return;
    }

    std::string msgNumStr;
    if (!recvLine(msgNumStr)) {
        return;
    }

    int msgNum = std::atoi(msgNumStr.c_str());
    bool ok = store_.deleteMessage(username_, msgNum);
    sendAll(ok ? "OK\n" : "ERR\n");
}

void ClientSession::run() {
    if (blacklist_.isBlacklisted(clientIp_)) {
        sendAll("ERR\n");
        close(sockfd_);
        return;
    }

    while (true) {
        std::string cmd;
        if (!recvLine(cmd)) {
            break;
        }

        if (cmd == "LOGIN") {
            if (!handleLogin()) {
                break;
            }
        } else if (cmd == "SEND") {
            handleSend();
        } else if (cmd == "LIST") {
            handleList();
        } else if (cmd == "READ") {
            handleRead();
        } else if (cmd == "DEL") {
            handleDelete();
        } else if (cmd == "QUIT") {
            break;
        } else {
            sendAll("ERR\n");
        }
    }

    close(sockfd_);
}
