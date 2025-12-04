#include "MailStore.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
    // Hilfsfunktion: prüft, ob ein Pfad ein Verzeichnis ist
    bool isDirectory(const string &path) {
        struct stat st {};
        return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    }
} 

using namespace std;

// Username-Regeln: nicht leer, max 8 Zeichen, nur [a-z0-9]
bool MailStore::isValidUsername(const string &u) {
    if (u.empty() || u.size() > 8) {
        return false;
    }
    for (char c : u) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) {
            return false;
        }
    }
    return true;
}

// Konstruktor: Basisverzeichnis setzen und sicherstellen, dass es existiert
MailStore::MailStore(const string &baseDir) : baseDir_(baseDir) {
    mkdirIfNotExists(baseDir_);
}

// Nachricht als Datei speichern: eine .msg Datei pro Mail
bool MailStore::storeMessage(const string &sender,
                             const string &receiver,
                             const string &subject,
                             const string &body) {
    // Sender/Empfänger validieren
    if (!isValidUsername(receiver) || !isValidUsername(sender)) {
        return false;
    }

    lock_guard<mutex> lock(mtx_);

    // Benutzerverzeichnis anlegen (falls noch nicht vorhanden)
    string userDir = baseDir_ + "/" + receiver;
    mkdirIfNotExists(userDir);

    // Nächste freie Message-ID ermitteln
    int nextId = getNextMessageId(userDir);
    if (nextId <= 0) {
        return false;
    }

    // Dateiname z.B. "base/receiver/1.msg"
    string filename = userDir + "/" + to_string(nextId) + ".msg";
    FILE *f = fopen(filename.c_str(), "w");
    if (!f) {
        return false;
    }

    // Format:
    // 1: Sender
    // 2: Empfänger
    // 3: Betreff
    // 4+: Body
    fprintf(f, "%s\n", sender.c_str());
    fprintf(f, "%s\n", receiver.c_str());
    fprintf(f, "%s\n", subject.c_str());

    if (!body.empty()) {
        fprintf(f, "%s", body.c_str());
        // Sicherstellen, dass der Body mit \n endet
        if (body.back() != '\n') {
            fprintf(f, "\n");
        }
    }

    fclose(f);
    return true;
}

// Liste der Betreffzeilen eines Users holen
bool MailStore::listMessages(const string &username, vector<string> &subjects) {
    subjects.clear();

    if (!isValidUsername(username)) {
        return true; // Kein Fehler → einfach keine Mails
    }

    lock_guard<mutex> lock(mtx_);

    string userDir = baseDir_ + "/" + username;
    if (!isDirectory(userDir)) {
        return true; // User hat (noch) keinen Mail-Ordner
    }

    DIR *dir = opendir(userDir.c_str());
    if (!dir) {
        return true;
    }

    vector<int> ids;
    struct dirent *entry;

    // Alle *.msg Dateien einsammeln und IDs extrahieren
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) { // reguläre Datei
            string name = entry->d_name;

            if (name.size() > 4 && name.substr(name.size() - 4) == ".msg") {
                int id = atoi(name.substr(0, name.size() - 4).c_str());
                if (id > 0) {
                    ids.push_back(id);
                }
            }
        }
    }
    closedir(dir);

    // Nachrichten IDs sortieren (aufsteigend)
    sort(ids.begin(), ids.end());

    // Für jede ID die Datei öffnen und nur den Betreff lesen
    for (int id : ids) {
        string filename = userDir + "/" + to_string(id) + ".msg";
        FILE *f = fopen(filename.c_str(), "r");
        if (!f) {
            continue;
        }

        char *line = nullptr;
        size_t len = 0;

        getline(&line, &len, f); // sender (ignoriert)
        getline(&line, &len, f); // receiver (ignoriert)
        ssize_t n = getline(&line, &len, f); // subject

        if (n > 0) {
            string subject(line, static_cast<size_t>(n));
            trimNewline(subject);
            subjects.push_back(subject);
        }

        if (line) {
            free(line);
        }
        fclose(f);
    }

    return true;
}

// Komplette Nachricht lesen (Sender, Empfänger, Betreff, Body)
bool MailStore::readMessage(const string &username,
                            int msgNumber,
                            string &sender,
                            string &receiver,
                            string &subject,
                            string &body) {

    // Outputs vorab leeren
    sender.clear();
    receiver.clear();
    subject.clear();
    body.clear();

    if (!isValidUsername(username) || msgNumber <= 0) {
        return false;
    }

    lock_guard<mutex> lock(mtx_);

    // Pfad zur konkreten Nachricht
    string filename = baseDir_ + "/" + username + "/" + to_string(msgNumber) + ".msg";
    FILE *f = fopen(filename.c_str(), "r");
    if (!f) {
        return false;
    }

    char *line = nullptr;
    size_t len = 0;
    ssize_t n;

    // Zeile 1: Sender
    n = getline(&line, &len, f);
    if (n <= 0) {
        fclose(f);
        if (line) free(line);
        return false;
    }
    sender.assign(line, static_cast<size_t>(n));
    trimNewline(sender);

    // Zeile 2: Empfänger
    n = getline(&line, &len, f);
    if (n <= 0) {
        fclose(f);
        if (line) free(line);
        return false;
    }
    receiver.assign(line, static_cast<size_t>(n));
    trimNewline(receiver);

    // Zeile 3: Betreff
    n = getline(&line, &len, f);
    if (n <= 0) {
        fclose(f);
        if (line) free(line);
        return false;
    }
    subject.assign(line, static_cast<size_t>(n));
    trimNewline(subject);

    // Rest: Body (kann auch leer sein)
    body.clear();
    while ((n = getline(&line, &len, f)) > 0) {
        body.append(line, static_cast<size_t>(n));
    }

    if (line) {
        free(line);
    }
    fclose(f);
    return true;
}

// Nachricht löschen (entsprechende .msg Datei entfernen)
bool MailStore::deleteMessage(const string &username, int msgNumber) {
    if (!isValidUsername(username) || msgNumber <= 0) {
        return false;
    }

    lock_guard<mutex> lock(mtx_);

    string filename = baseDir_ + "/" + username + "/" + to_string(msgNumber) + ".msg";
    int res = unlink(filename.c_str());

    return (res == 0);
}

// Entfernt trailing \n / \r aus einem String
void MailStore::trimNewline(string &s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
}

// Legt ein Verzeichnis an, falls es noch nicht existiert
void MailStore::mkdirIfNotExists(const string &path) {
    struct stat st {};
    if (stat(path.c_str(), &st) == -1) {
        mkdir(path.c_str(), 0755);
    }
}

// Ermittelt die nächste freie Message-ID im Nutzerverzeichnis
int MailStore::getNextMessageId(const string &userDir) {
    DIR *dir = opendir(userDir.c_str());

    int maxId = 0;
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_REG) {
                string name = entry->d_name;
                if (name.size() > 4 && name.substr(name.size() - 4) == ".msg") {
                    int id = atoi(name.substr(0, name.size() - 4).c_str());
                    if (id > maxId) {
                        maxId = id;
                    }
                }
            }
        }
        closedir(dir);
    }
    // Nächste ID ist maxId + 1 (startet bei 1)
    return maxId + 1;
}
