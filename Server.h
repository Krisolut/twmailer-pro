#pragma once

#include <string>

class MailStore;
class BlacklistManager;
class LdapAuthenticator;

/// Hauptklasse für den TW-Mailer-Server.
/// Öffnet den Listening-Socket, akzeptiert Clients und startet Session-Threads.
class Server {
public:
    /// Erstellt den Server mit Port und Spool-Verzeichnis.
    /// @param port TCP-Port für eingehende Verbindungen.
    /// @param spoolDir Verzeichnis für alle Maildaten.
    Server(int port, std::string spoolDir);

    /// Startet den Accept-Loop und bedient Clients parallel.
    /// @return true, falls der Server erfolgreich beendet wurde.
    bool run();

private:
    int port_;
    std::string spoolDir_;

    bool setupSocket(int &sockfd);
};
