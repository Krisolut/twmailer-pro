#pragma once

#include <ctime>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

/// Klasse zur Verwaltung einer zeitbasierten IP-Blacklist mit Persistenz.
/// Verwaltet fehlgeschlagene Logins, sperrt IPs temporär und speichert die Daten auf Platte.
class BlacklistManager {
public:
    /// Erstellt den Manager und lädt bestehende Einträge.
    /// @param storageFile Pfad zur Persistenz-Datei der Blacklist.
    explicit BlacklistManager(const std::string &storageFile);

    /// Prüft, ob die IP aktuell gesperrt ist und entfernt abgelaufene Einträge.
    /// @param ip Zu prüfende IPv4-Adresse.
    /// @return true, falls die IP gesperrt ist.
    bool isBlacklisted(const std::string &ip);

    /// Protokolliert einen fehlgeschlagenen Login-Versuch.
    /// @param ip Adresse des Clients.
    /// @param username Benutzername, gegen den versucht wurde.
    /// @return true, falls dadurch eine neue Sperre ausgelöst wurde.
    bool recordFailure(const std::string &ip, const std::string &username);

    /// Setzt die Fehlversuche nach erfolgreichem Login zurück.
    /// @param ip Adresse des Clients.
    /// @param username Benutzername der erfolgreichen Anmeldung.
    void recordSuccess(const std::string &ip, const std::string &username);

private:
    std::string storageFile_;
    std::unordered_map<std::string, std::time_t> blacklist_;
    std::unordered_map<std::string, int> attempts_;
    std::mutex mtx_;

    void load();
    void persist();
    void cleanupExpired();
    static std::string attemptKey(const std::string &ip, const std::string &username);
};
