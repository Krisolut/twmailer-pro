#pragma once

#include <memory>
#include <string>

class MailStore;
class BlacklistManager;
class LdapAuthenticator;

/// Klasse, die eine einzelne Client-Verbindung repräsentiert und alle Befehle abwickelt.
/// Verwaltet den Login-Status, liest Befehle und ruft die benötigten Services auf.
class ClientSession {
public:
    /// Erstellt eine Session für einen akzeptierten Socket.
    /// @param socketFD File-Descriptor der Client-Verbindung.
    /// @param clientIp Textuelle IPv4-Adresse des Clients.
    /// @param store Gemeinsamer MailStore.
    /// @param blacklist Gemeinsame Blacklist-Verwaltung.
    /// @param authenticator LDAP-Authentifikator.
    ClientSession(int socketFD,
                  std::string clientIp,
                  MailStore &store,
                  BlacklistManager &blacklist,
                  LdapAuthenticator &authenticator);

    /// Startet die Verarbeitungsschleife für den Client.
    void run();

private:
    int sockfd_;
    std::string clientIp_;
    MailStore &store_;
    BlacklistManager &blacklist_;
    LdapAuthenticator &authenticator_;

    bool authenticated_ = false;
    std::string username_;

    bool sendAll(const std::string &data) const;
    bool recvLine(std::string &line) const;

    bool handleLogin();
    void handleSend();
    void handleList();
    void handleRead();
    void handleDelete();
};
