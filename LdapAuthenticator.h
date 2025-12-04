#pragma once

#include <string>

/// Klasse, die die LDAP-Authentifizierung gegen den Hochschulserver kapselt.
/// Bietet eine einfache Schnittstelle zum Prüfen von Benutzername/Passwort.
class LdapAuthenticator {
public:
    /// Erstellt einen Authentifikator mit optional angepassten Verbindungsdaten.
    /// @param host LDAP-Hostname.
    /// @param port LDAP-Port.
    /// @param baseDn Basis-DN für die Benutzersuche.
    LdapAuthenticator(std::string host = "ldap.technikum-wien.at",
                      int port = 389,
                      std::string baseDn = "dc=technikum-wien,dc=at");

    /// Führt die Anmeldung durch und prüft die Zugangsdaten.
    /// @param username Benutzername (uid).
    /// @param password Klartext-Passwort.
    /// @return true bei erfolgreicher Authentifizierung.
    bool authenticate(const std::string &username, const std::string &password) const;

private:
    std::string host_;
    int port_;
    std::string baseDn_;
};
