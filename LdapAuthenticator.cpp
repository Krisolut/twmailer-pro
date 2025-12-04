#include "LdapAuthenticator.h"

#include <iostream>
#include <ldap.h>

using namespace std;

LdapAuthenticator::LdapAuthenticator(string host, int port, string baseDn)
    : host_(move(host)), port_(port), baseDn_(move(baseDn)) {}
    // move() verhindert unnötige Kopien, weil die Strings übernommen werden.
    
bool LdapAuthenticator::authenticate(const string &username, const string &password) const {
    if (username.empty() || password.empty()) {
        return false;
    }

    // LDAP-URI aus Host + Port bauen
    string uri = "ldap://" + host_ + ":" + to_string(port_);
    LDAP *ld = nullptr;

    // LDAP-Handle initialisieren
    int rc = ldap_initialize(&ld, uri.c_str());
    if (rc != LDAP_SUCCESS || ld == nullptr) {
        cerr << "LDAP init fehlgeschlagen: " << ldap_err2string(rc) << endl;
        return false;
    }

     // LDAP v3 aktivieren
    int version = LDAP_VERSION3;
    ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &version);

    // Filter: suche nach user object mit uid=<username>
    string filter = "(uid=" + username + ")";
    LDAPMessage *searchResult = nullptr;

    // LDAP-Suche starten (SUBTREE = suche im ganzen Baum ab baseDn)
    rc = ldap_search_ext_s(ld, baseDn_.c_str(), LDAP_SCOPE_SUBTREE, filter.c_str(), nullptr, 0,
                           nullptr, nullptr, nullptr, 0, &searchResult);
    if (rc != LDAP_SUCCESS) {
        cerr << "LDAP search fehlgeschlagen: " << ldap_err2string(rc) << endl;
        if (searchResult) {
            ldap_msgfree(searchResult);
        }
        ldap_unbind_ext_s(ld, nullptr, nullptr);
        return false;
    }

    // Erstes Such-Resultat holen (sollte der User sein)
    LDAPMessage *entry = ldap_first_entry(ld, searchResult);
    if (!entry) {
        ldap_msgfree(searchResult);
        ldap_unbind_ext_s(ld, nullptr, nullptr);
        return false;
    }

    // Distinguished Name (DN) aus dem Such-Resultat herausziehen
    char *dn = ldap_get_dn(ld, entry);
    if (!dn) {
        ldap_msgfree(searchResult);
        ldap_unbind_ext_s(ld, nullptr, nullptr);
        return false;
    }

    // Passwort vorbereiten (SASL benötigt berval-Struktur)
    struct berval cred;
    cred.bv_val = const_cast<char*>(password.c_str());
    cred.bv_len = password.size();

    // Einfacher Bind-Versuch mit DN + Passwort
    rc = ldap_sasl_bind_s(
        ld,
        dn,
        LDAP_SASL_SIMPLE,
        &cred,
        nullptr,
        nullptr,
        nullptr
    );
    
    if (rc != LDAP_SUCCESS) {
        cerr << "LDAP bind fehlgeschlagen: " << ldap_err2string(rc) << endl;
    }

    // Aufräumen
    ldap_memfree(dn);
    ldap_msgfree(searchResult);
    ldap_unbind_ext_s(ld, nullptr, nullptr);

    // Auth erfolgreich, wenn Bind erfolgreich 
    return rc == LDAP_SUCCESS;
}
