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
    constexpr size_t MAX_SUBJECT = 80; // maximale Betrefflänge
}

using namespace std;

ClientSession::ClientSession(int socketFD,
                             string clientIp,
                             MailStore &store,
                             BlacklistManager &blacklist,
                             LdapAuthenticator &authenticator)
    : sockfd_(socketFD),
      clientIp_(move(clientIp)),
      store_(store),
      blacklist_(blacklist),
      authenticator_(authenticator) {}

// Schickt eine beliebige Menge an Bytes über den Socket
bool ClientSession::sendAll(const string &data) const {
    const char *buf = data.c_str();
    size_t total = 0;
    size_t len = data.size();

    // Solange weiterschicken, bis alles raus ist
    while (total < len) {
        ssize_t n = send(sockfd_, buf + total, len - total, 0);
        if (n <= 0) {
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

// Liest eine Zeile (\n-terminiert) vom Socket
bool ClientSession::recvLine(string &line) const {
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

    // Entferne \r falls vorhanden (Windows-Style)
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }

    return true;
}

// LOGIN-Befehl: User & Passwort lesen und authentifizieren
bool ClientSession::handleLogin() {
    string user;
    string pass;

    // Erwartet 2 Zeilen: Username, Passwort
    if (!recvLine(user) || !recvLine(pass)) {
        return false;
    }

    // Falls IP geblacklistet → sofortiger Fehler
    if (blacklist_.isBlacklisted(clientIp_)) {
        sendAll("ERR\n");
        return true;
    }

    // LDAP-Auth
    if (authenticator_.authenticate(user, pass)) {
        authenticated_ = true;
        username_ = user;
        blacklist_.recordSuccess(clientIp_, user);
        sendAll("OK\n");
    } else {
        bool banned = blacklist_.recordFailure(clientIp_, user);
        sendAll("ERR\n");
        if (banned) {
            cerr << "IP " << clientIp_ << " gesperrt nach Fehlversuchen" << endl;
        }
    }
    return true;
}

// SEND-Befehl: Nachricht absenden
void ClientSession::handleSend() {
    if (!authenticated_) {
        sendAll("ERR\n");
        return;
    }

    string receiver;
    string subject;

    // Empfänger & Betreff lesen
    if (!recvLine(receiver) || !recvLine(subject)) {
        return;
    }

    // Betreff ggf. kürzen
    if (subject.size() > MAX_SUBJECT) {
        subject = subject.substr(0, MAX_SUBJECT);
    }

    // Body lesen, bis "." allein steht
    string body;
    while (true) {
        string line;
        if (!recvLine(line)) {
            return;
        }
        if (line == ".") {
            break;
        }
        body += line + "\n";
    }

    // Nachricht speichern
    bool ok = store_.storeMessage(username_, receiver, subject, body);
    sendAll(ok ? "OK\n" : "ERR\n");
}

// LIST-Befehl: Liste aller Betreffzeilen senden
void ClientSession::handleList() {
    if (!authenticated_) {
        sendAll("ERR\n");
        return;
    }

    vector<string> subjects;
    store_.listMessages(username_, subjects);

    // Anzahl + jede Zeile
    string resp = to_string(subjects.size()) + "\n";
    for (const auto &s : subjects) {
        resp += s + "\n";
    }
    sendAll(resp);
}

// READ-Befehl: eine Nachricht vollständig ausgeben
void ClientSession::handleRead() {
    if (!authenticated_) {
        sendAll("ERR\n");
        return;
    }

    string msgNumStr;
    if (!recvLine(msgNumStr)) {
        return;
    }

    int msgNum = atoi(msgNumStr.c_str());
    string sender, receiver, subject, body;

    // Nachricht aus dem Store holen
    if (!store_.readMessage(username_, msgNum, sender, receiver, subject, body)) {
        sendAll("ERR\n");
        return;
    }

    // Ausgabeformat
    string resp = "OK\n";
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

// DEL-Befehl: Nachricht löschen
void ClientSession::handleDelete() {
    if (!authenticated_) {
        sendAll("ERR\n");
        return;
    }

    string msgNumStr;
    if (!recvLine(msgNumStr)) {
        return;
    }

    int msgNum = atoi(msgNumStr.c_str());
    bool ok = store_.deleteMessage(username_, msgNum);
    sendAll(ok ? "OK\n" : "ERR\n");
}

// Haupt-Loop der Session
void ClientSession::run() {
    // Sofortiger Block falls IP gesperrt
    if (blacklist_.isBlacklisted(clientIp_)) {
        sendAll("ERR\n");
        close(sockfd_);
        return;
    }

    while (true) {
        string cmd;
        if (!recvLine(cmd)) {
            break;
        }

        // Kommandos
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

    // Verbindung sauber schließen
    close(sockfd_);
}
