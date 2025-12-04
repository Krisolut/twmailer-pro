#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

// Schickt den gesamten String über den Socket (ggf. in mehreren send()-Aufrufen)
static bool sendAll(int sockfd, const string &data) {
    const char *buf = data.c_str();
    size_t total = 0;
    size_t len = data.size();
    while (total < len) {
        ssize_t n = send(sockfd, buf + total, len - total, 0);
        if (n <= 0) return false; // Fehler oder Verbindung abgebrochen
        total += static_cast<size_t>(n);
    }
    return true;
}

// Liest eine Zeile vom Socket (bis '\n'), entfernt optionales '\r'
static bool recvLine(int sockfd, string &line) {
    line.clear();
    char c;
    while (true) {
        ssize_t n = recv(sockfd, &c, 1, 0);
        if (n <= 0) {
            return false; // Fehler oder Verbindung weg
        }
        if (c == '\n') break;   // Zeile komplett
        line.push_back(c);
    }
    // CRLF → optionales '\r' entfernen
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

// Einfaches Textmenü anzeigen
static void menu(bool loggedIn) {
    cout << "\nTW-Mailer Client\n";
    cout << "1) LOGIN\n";
    cout << "2) SEND\n";
    cout << "3) LIST\n";
    cout << "4) READ\n";
    cout << "5) DEL\n";
    cout << "6) QUIT\n";
    cout << (loggedIn ? "(Angemeldet)" : "(Nicht angemeldet)") << "\n";
    cout << "Choice: ";
}

// LOGIN-Kommando: Username/Passwort lesen und an Server schicken
static bool doLOGIN(int sockfd, string &username, bool &loggedIn) {
    cout << "Username: ";
    getline(cin, username);
    cout << "Passwort: ";
    string password;
    getline(cin, password);

    // Protokoll: LOGIN\n<user>\n<pass>\n
    string req;
    req += "LOGIN\n";
    req += username + "\n";
    req += password + "\n";

    if (!sendAll(sockfd, req)) {
        cerr << "Error sending LOGIN request\n";
        return false;
    }

    string resp;
    if (!recvLine(sockfd, resp)) {
        cerr << "No response from server\n";
        return false;
    }

    if (resp == "OK") {
        cout << "Login erfolgreich.\n";
        loggedIn = true;
    } else {
        cout << "Login fehlgeschlagen.\n";
        loggedIn = false;
    }
    return true;
}

// SEND-Kommando: neue Nachricht erstellen und an Server schicken
static void doSEND(int sockfd, const string &username, bool loggedIn) {
    if (!loggedIn) {
        cout << "Bitte zuerst LOGIN ausführen.\n";
        return;
    }

    string receiver, subject;

    cout << "Receiver (max 8 chars, a-z,0-9): ";
    getline(cin, receiver);
    cout << "Subject (max 80 chars): ";
    getline(cin, subject);

    cout << "Message body (end with single '.' on a line):\n";
    string body;
    while (true) {
        string line;
        getline(cin, line);
        if (line == ".") break;   // Punkt alleine beendet Eingabe
        body += line + "\n";
    }

    // Protokoll: SEND\n<receiver>\n<subject>\n<body>.\n
    string req;
    req += "SEND\n";
    req += receiver + "\n";
    req += subject + "\n";
    req += body;
    req += ".\n";

    if (!sendAll(sockfd, req)) {
        cerr << "Error sending SEND request\n";
        return;
    }

    string resp;
    if (!recvLine(sockfd, resp)) {
        cerr << "No response from server\n";
        return;
    }

    cout << "Server: " << resp << endl;
}

// LIST-Kommando: Liste der Betreffzeilen anzeigen
static void doLIST(int sockfd, const string &username, bool loggedIn) {
    if (!loggedIn) {
        cout << "Bitte zuerst LOGIN ausführen.\n";
        return;
    }

    string req;
    req += "LIST\n";

    if (!sendAll(sockfd, req)) {
        cerr << "Error sending LIST request\n";
        return;
    }

    string line;
    if (!recvLine(sockfd, line)) {
        cerr << "No response from server\n";
        return;
    }

    int count = atoi(line.c_str()); // Anzahl der Nachrichten
    cout << "Messages for " << username << ": " << count << "\n";

    // Jede weitere Zeile ist eine Betreffzeile
    for (int i = 1; i <= count; ++i) {
        if (!recvLine(sockfd, line)) {
            cerr << "Unexpected end of response\n";
            return;
        }
        cout << i << ") " << line << "\n";
    }
}

// READ-Kommando: einzelne Nachricht mit Body anzeigen
static void doREAD(int sockfd, const string &username, bool loggedIn) {
    if (!loggedIn) {
        cout << "Bitte zuerst LOGIN ausführen.\n";
        return;
    }

    string num;
    cout << "Message number: ";
    getline(cin, num);

    // Protokoll: READ\n<num>\n
    string req;
    req += "READ\n";
    req += num + "\n";

    if (!sendAll(sockfd, req)) {
        cerr << "Error sending READ request\n";
        return;
    }

    string line;
    if (!recvLine(sockfd, line)) {
        cerr << "No response from server\n";
        return;
    }

    if (line == "ERR") {
        cout << "Server: ERR\n";
        return;
    }
    if (line != "OK") {
        cout << "Unexpected response: " << line << "\n";
        return;
    }

    // Header: Sender, Receiver, Subject
    string sender, receiver, subject;
    if (!recvLine(sockfd, sender) ||
        !recvLine(sockfd, receiver) ||
        !recvLine(sockfd, subject)) {
        cerr << "Incomplete message header\n";
        return;
    }

    cout << "Sender:   " << sender << "\n";
    cout << "Receiver: " << receiver << "\n";
    cout << "Subject:  " << subject << "\n";
    cout << "Body:\n";

    // Body bis Zeile mit "." lesen
    while (true) {
        if (!recvLine(sockfd, line)) {
            cerr << "Connection lost while reading body\n";
            return;
        }
        if (line == ".") break;
        cout << line << "\n";
    }
}

// DEL-Kommando: Nachricht löschen
static void doDEL(int sockfd, const string &username, bool loggedIn) {
    if (!loggedIn) {
        cout << "Bitte zuerst LOGIN ausführen.\n";
        return;
    }

    string num;
    cout << "Message number: ";
    getline(cin, num);

    // Protokoll: DEL\n<num>\n
    string req;
    req += "DEL\n";
    req += num + "\n";

    if (!sendAll(sockfd, req)) {
        cerr << "Error sending DEL request\n";
        return;
    }

    string line;
    if (!recvLine(sockfd, line)) {
        cerr << "No response from server\n";
        return;
    }

    cout << "Server: " << line << "\n";
}

int main(int argc, char *argv[]) {
    // Erwartet: IP und Port als Parameter
    if (argc != 3) {
        cerr << "Usage: ./twmailer-client <ip> <port>\n";
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    // TCP-Socket anlegen
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    // Zieladresse vorbereiten
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        cerr << "Invalid IP address\n";
        close(sockfd);
        return 1;
    }

    // Verbindung zum Server aufbauen
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    cout << "Connected to " << ip << ":" << port << "\n";

    bool loggedIn = false;
    string username;

    // Interaktive Hauptschleife
    while (true) {
        menu(loggedIn);
        string choice;
        getline(cin, choice);
        if (choice.empty()) continue;

        if (choice == "1") {
            doLOGIN(sockfd, username, loggedIn);
        } else if (choice == "2") {
            doSEND(sockfd, username, loggedIn);
        } else if (choice == "3") {
            doLIST(sockfd, username, loggedIn);
        } else if (choice == "4") {
            doREAD(sockfd, username, loggedIn);
        } else if (choice == "5") {
            doDEL(sockfd, username, loggedIn);
        } else if (choice == "6") {
            // QUIT an Server schicken und beenden
            string req = "QUIT\n";
            sendAll(sockfd, req);
            break;
        } else {
            cout << "Unknown choice.\n";
        }
    }

    close(sockfd);
    return 0;
}
