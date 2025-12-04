#include "LdapAuthenticator.h"

#include <iostream>
#include <ldap.h>

LdapAuthenticator::LdapAuthenticator(std::string host, int port, std::string baseDn)
    : host_(std::move(host)), port_(port), baseDn_(std::move(baseDn)) {}

bool LdapAuthenticator::authenticate(const std::string &username, const std::string &password) const {
    if (username.empty() || password.empty()) {
        return false;
    }

    std::string uri = "ldap://" + host_ + ":" + std::to_string(port_);
    LDAP *ld = nullptr;
    int rc = ldap_initialize(&ld, uri.c_str());
    if (rc != LDAP_SUCCESS || ld == nullptr) {
        std::cerr << "LDAP init fehlgeschlagen: " << ldap_err2string(rc) << std::endl;
        return false;
    }

    int version = LDAP_VERSION3;
    ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &version);

    std::string filter = "(uid=" + username + ")";
    LDAPMessage *searchResult = nullptr;
    rc = ldap_search_ext_s(ld, baseDn_.c_str(), LDAP_SCOPE_SUBTREE, filter.c_str(), nullptr, 0,
                           nullptr, nullptr, nullptr, 0, &searchResult);
    if (rc != LDAP_SUCCESS) {
        std::cerr << "LDAP search fehlgeschlagen: " << ldap_err2string(rc) << std::endl;
        if (searchResult) {
            ldap_msgfree(searchResult);
        }
        ldap_unbind_ext_s(ld, nullptr, nullptr);
        return false;
    }

    LDAPMessage *entry = ldap_first_entry(ld, searchResult);
    if (!entry) {
        ldap_msgfree(searchResult);
        ldap_unbind_ext_s(ld, nullptr, nullptr);
        return false;
    }

    char *dn = ldap_get_dn(ld, entry);
    if (!dn) {
        ldap_msgfree(searchResult);
        ldap_unbind_ext_s(ld, nullptr, nullptr);
        return false;
    }

    rc = ldap_simple_bind_s(ld, dn, password.c_str());
    if (rc != LDAP_SUCCESS) {
        std::cerr << "LDAP bind fehlgeschlagen: " << ldap_err2string(rc) << std::endl;
    }

    ldap_memfree(dn);
    ldap_msgfree(searchResult);
    ldap_unbind_ext_s(ld, nullptr, nullptr);

    return rc == LDAP_SUCCESS;
}
