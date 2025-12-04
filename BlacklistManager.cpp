#include "BlacklistManager.h"

#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
    // Wie oft ein Login scheitern darf, bevor gesperrt wird
    constexpr int MAX_ATTEMPTS = 3;

    // Wie lange eine IP gesperrt bleibt (Sekunden)
    constexpr int BAN_SECONDS = 60;
}

using namespace std;

BlacklistManager::BlacklistManager(const string &storageFile)
    : storageFile_(storageFile) {
    load();           // gespeicherte Sperren laden
    cleanupExpired(); // abgelaufene Einträge sofort entfernen
}

// Prüfen, ob eine IP aktuell gesperrt ist
bool BlacklistManager::isBlacklisted(const string &ip) {
    lock_guard<mutex> lock(mtx_);
    cleanupExpired(); // alte Sperren entfernen
    auto it = blacklist_.find(ip);

    // Nur gesperrt, wenn Ablaufzeit in der Zukunft liegt
    return it != blacklist_.end() && it->second > time(nullptr);
}

// Fehlversuch protokollieren und ggf. sperren
bool BlacklistManager::recordFailure(const string &ip, const string &username) {
    lock_guard<mutex> lock(mtx_);
    cleanupExpired();

    // Key kombiniert IP + Username → verhindert Überschneidung
    string key = attemptKey(ip, username);

    // Anzahl der Fehlversuche hochzählen
    int count = ++attempts_[key];

    // Wenn max. Versuche überschritten sind → IP bannen
    if (count >= MAX_ATTEMPTS) {
        time_t until = time(nullptr) + BAN_SECONDS;
        blacklist_[ip] = until; // Sperre eintragen
        attempts_.erase(key);   // Fehlversuchs-Tracker zurücksetzen
        persist();              // Datei speichern
        return true;            // true → wurde gebannt
    }
    return false;               // false → noch kein Bann
}

// Erfolgreicher Login → Fehlversuche zurücksetzen
void BlacklistManager::recordSuccess(const string &ip, const string &username) {
    lock_guard<mutex> lock(mtx_);
    attempts_.erase(attemptKey(ip, username));
}

// Blacklist aus Datei einlesen
void BlacklistManager::load() {
    ifstream in(storageFile_);
    if (!in.is_open()) {
        return; // Datei existiert evtl. noch nicht → kein Problem
    }

    string ip;
    time_t until;

    // Format: "<ip> <timestamp>"
    while (in >> ip >> until) {
        // Nur aktive Sperren laden
        if (until > time(nullptr)) {
            blacklist_[ip] = until;
        }
    }
}

// Blacklist in Datei speichern
void BlacklistManager::persist() {
    ofstream out(storageFile_, ios::trunc);
    if (!out.is_open()) {
        cerr << "Kann Blacklist nicht schreiben: " << storageFile_ << endl;
        return;
    }

    // Nur aktive Sperren persistieren
    for (const auto &entry : blacklist_) {
        if (entry.second > time(nullptr)) {
            out << entry.first << ' ' << entry.second << '\n';
        }
    }
}

// Löscht alle Einträge, deren Bannzeit abgelaufen ist
void BlacklistManager::cleanupExpired() {
    bool changed = false;
    time_t now = time(nullptr);

    for (auto it = blacklist_.begin(); it != blacklist_.end();) {
        if (it->second <= now) {
            // Bann abgelaufen → löschen
            it = blacklist_.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }

    // Nur dann erneut speichern, wenn sich etwas geändert hat
    if (changed) {
        persist();
    }
}

// Baut einen eindeutigen Schlüssel für Fehlversuche
string BlacklistManager::attemptKey(const string &ip, const string &username) {
    return ip + "|" + username; // "|" trennt eindeutig
}
