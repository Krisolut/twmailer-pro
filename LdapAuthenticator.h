#pragma once

#include <string>

using namespace std;
class LdapAuthenticator {
public:
    /// @param host LDAP-Hostname.
    /// @param port LDAP-Port.
    /// @param baseDn Basis-DN für die Benutzersuche.
    LdapAuthenticator(string host = "ldap.technikum-wien.at",
                      int port = 389,
                      string baseDn = "dc=technikum-wien,dc=at");

    /// @param username Benutzername (uid).
    /// @param password Klartext-Passwort.
    /// @return true bei erfolgreicher Authentifizierung.
    bool authenticate(const string &username, const string &password) const;

private:
    string host_;
    int port_;
    string baseDn_;
};
