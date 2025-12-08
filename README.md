# TWMailer Pro – Architektur & Funktionsübersicht

Diese Dokumentation beschreibt die Architektur von **twmailer-pro**, den Ablauf
zwischen Client und Server sowie die wichtigsten Module, Datenstrukturen und
Protokollkommandos.

---

## 1. Gesamtüberblick

TWMailer Pro ist ein einfaches textbasiertes Mail-System im Client-Server-Modell:

- **twmailer-client**  
  Terminal-Programm, das Kommandos eingibt und Serverantworten ausgibt.

- **twmailer-server**  
  Multi-Thread-TCP-Server mit:
  - LDAP-Login
  - Mail-Speicherung im Dateisystem
  - IP-Blacklist nach Fehlversuchen
  - Session-basierter Verarbeitung

Die Kommunikation erfolgt über ein **zeilenbasiertes Textprotokoll** über TCP.
Jede Nachricht endet mit einem `\n`.

---

## 2. Client

### 2.1 Aufgabe des Clients

Der Client:

- baut eine TCP-Verbindung zum Server auf
- zeigt ein interaktives Menü
- interpretiert Nutzereingaben
- konvertiert diese in Protokollkommandos
- sendet diese an den Server
- empfängt Antworten und zeigt sie an

---

### 2.2 Wichtige Funktionen

#### main()

1. Erwartet Argumente:
   `./twmailer-client <IP> <PORT>`
2. Erstellt einen TCP-Socket:
   `socket(AF_INET, SOCK_STREAM, 0)`
3. Baut `sockaddr_in`:
   - `sin_family = AF_INET`
   - `sin_port = htons(port)`
   - `inet_pton(AF_INET, ip, &addr.sin_addr)`
4. Verbindung via `connect()`
5. Menü-Loop:
   - Eingabe lesen
   - passende doXYZ-Funktion aufrufen

---

#### sendAll(sockfd, data)

- Sendet einen String vollständig über TCP.
- Nutzt eine Schleife, um Teil-Sends auszugleichen.
- Gibt `true` oder `false` zurück.

---

#### recvLine(sockfd, line)

- Liest Byte für Byte, bis `\n` erreicht wird.
- Entfernt optional `\r` am Ende.
- Rückgabe: `true` oder `false`.

---

#### LOGIN – doLOGIN()

Ablauf:

1. Benutzer gibt Username + Passwort ein.
2. Client sendet:

   LOGIN  
   `<username>`  
   `<password>`

3. Server antwortet:
   - `OK` → Login erfolgreich
   - `ERR` → Login fehlgeschlagen

---

#### SEND – doSEND()

Body wird zeilenweise eingegeben, beendet durch eine einzelne Zeile mit `.`.

Request:

    SEND
    <receiver>
    <subject>
    <body-Zeile 1>
    <body-Zeile 2>
    ...
    .

Response:

    OK

oder

    ERR

---

#### LIST – doLIST()

Request:

    LIST

Response:

    <N>
    <subject1>
    <subject2>
    ...

---

#### READ – doREAD()

Request:

    READ
    <number>

Antwort bei Erfolg:

    OK
    <sender>
    <receiver>
    <subject>
    <body-Zeilen ...>
    .

Bei Fehler:

    ERR

---

#### DEL – doDEL()

Request:

    DEL
    <number>

Response:

    OK

oder

    ERR

---

## 3. Server

### 3.1 Komponentenübersicht

Der Server besteht aus fünf zentralen Modulen:

1. **Server**
   - verwaltet den TCP-Socket
   - akzeptiert Verbindungen
   - erzeugt Threads pro Client

2. **ClientSession**
   - verarbeitet Kommandos einer TCP-Verbindung
   - hält Authentifizierungsstatus
   - ruft MailStore, LDAP und Blacklist auf

3. **MailStore**
   - speichert Nachrichten als Dateien
   - listet Nachrichten
   - liest Nachrichten
   - löscht Nachrichten

4. **BlacklistManager**
   - sperrt IPs temporär nach zu vielen Fehl-Logins
   - speichert Sperren persistent in Datei

5. **LdapAuthenticator**
   - führt Echt-Login gegen Technikum-LDAP durch
   - verwendet OpenLDAP-C-API

---

## 4. Server – Ablaufdetails

### 4.1 Start

Der Server wird gestartet als:

    ./twmailer-server <PORT> <spoolDir>

Beispiel:

    ./twmailer-server 2025 /var/spool/twmailer

---

### 4.2 Server::run()

1. Erstellt TCP-Socket (socket, bind, listen)
2. Erzeugt:
   - `MailStore store`
   - `BlacklistManager blacklist`
   - `LdapAuthenticator authenticator`
3. Endlosschleife:
   - `accept()` auf eingehende Verbindungen
   - IP des Clients auslesen
   - Blacklist prüfen
   - neuen Thread mit `ClientSession` starten

---

### 4.3 ClientSession

#### Zustände

- `authenticated_` (bool)
- `username_` (string)
- `clientIp_` (string)
- Referenzen auf:
  - `MailStore`
  - `BlacklistManager`
  - `LdapAuthenticator`

#### Befehlsschleife (run)

1. Zeile mit dem Kommando einlesen (`LOGIN`, `SEND`, `LIST`, `READ`, `DEL`, `QUIT`).
2. Je nach Kommando entsprechende Handler-Funktion aufrufen.
3. Bei `QUIT` oder Verbindungsfehler: Socket schließen und Thread beenden.

---

### 4.4 handleLogin()

1. Liest Username- und Passwort-Zeile.
2. Prüft, ob IP bereits gesperrt ist (`BlacklistManager::isBlacklisted`).
3. Ruft `LdapAuthenticator::authenticate(username, password)` auf.
4. Falls erfolgreich:
   - setzt `authenticated_ = true`
   - speichert `username_`
   - ruft `BlacklistManager::recordSuccess`
   - sendet `OK`
5. Falls fehlgeschlagen:
   - ruft `BlacklistManager::recordFailure`
   - bei Überschreiten der Maximalversuche wird IP gesperrt
   - sendet `ERR`

---

### 4.5 handleSend()

1. Nur erlaubt, wenn `authenticated_ == true`, sonst `ERR`.
2. Liest:
   - Empfänger
   - Betreff
   - Body-Zeilen bis zu einer Zeile mit `.`
3. Ruft `MailStore::storeMessage(sender, receiver, subject, body)` auf.
4. Sendet:
   - `OK` bei Erfolg
   - `ERR` bei Fehler

---

### 4.6 handleList()

1. Nur erlaubt bei authentifiziertem Benutzer.
2. Ruft `MailStore::listMessages(username_, subjects)` auf.
3. Sendet:

       <N>
       <subject1>
       <subject2>
       ...

   wobei `N` die Anzahl der Nachrichten ist.

---

### 4.7 handleRead()

1. Nur erlaubt bei authentifiziertem Benutzer.
2. Liest eine Nachrichten-Nummer.
3. Ruft `MailStore::readMessage(username_, num, sender, receiver, subject, body)` auf.
4. Bei Erfolg sendet:

       OK
       <sender>
       <receiver>
       <subject>
       <body>
       .

5. Bei Fehlschlag sendet:

       ERR

---

### 4.8 handleDelete()

1. Nur erlaubt bei authentifiziertem Benutzer.
2. Liest eine Nachrichten-Nummer.
3. Ruft `MailStore::deleteMessage(username_, num)` auf.
4. Antwort:

       OK

   oder

       ERR

---

## 5. MailStore

Der `MailStore` verwaltet die persistente Ablage der Nachrichten auf dem Dateisystem.

### 5.1 Verzeichnisstruktur

Im Spool-Verzeichnis (`spoolDir`) wird für jeden Benutzer ein Unterordner angelegt:

    <spoolDir>/
        alice/
            1.msg
            2.msg
        bob/
            1.msg

### 5.2 Dateiformat einer Nachricht

Die `.msg`-Datei hat folgendes Format:

1. Zeile: Sender  
2. Zeile: Empfänger  
3. Zeile: Betreff  
4. und folgende Zeilen: Body (Text der Nachricht)

### 5.3 Wichtige Methoden

- `storeMessage(sender, receiver, subject, body)`  
  - validiert Benutzernamen
  - erzeugt Verzeichnis für den Empfänger (falls nötig)
  - ermittelt nächste freie Nachrichtennummer
  - schreibt eine neue `.msg`-Datei

- `listMessages(username, subjects)`  
  - durchsucht das Benutzerverzeichnis
  - liest zu jeder Nachricht den Betreff
  - füllt einen Vektor mit allen Betreffzeilen

- `readMessage(username, num, sender, receiver, subject, body)`  
  - öffnet die Datei `<num>.msg`
  - liest Sender, Empfänger, Betreff und Body
  - gibt die Daten über Referenzen zurück

- `deleteMessage(username, num)`  
  - löscht die Datei `<num>.msg`

Alle schreibenden Zugriffe werden typischerweise durch einen Mutex geschützt, um Thread-Sicherheit zu gewährleisten.

---

## 6. BlacklistManager

Der `BlacklistManager` schützt den Server vor Brute-Force-Logins.

### 6.1 Prinzip

- Jeder Fehlversuch wird pro Kombination aus IP und Benutzer gezählt.
- Nach einer definierten Anzahl von Fehlversuchen (z.B. 3) wird die IP für eine gewisse Zeit (z.B. 60 Sekunden) gesperrt.
- Sperren werden in einer Datei gespeichert, sodass sie Server-Neustarts überleben.

### 6.2 Wichtige Methoden

- `isBlacklisted(ip)`  
  - prüft, ob die IP aktuell gesperrt ist.

- `recordFailure(ip, username)`  
  - erhöht den Zähler für `(ip, username)`.
  - sperrt die IP, wenn das Limit erreicht ist.

- `recordSuccess(ip, username)`  
  - setzt den Fehlversuchs-Zähler für `(ip, username)` zurück.

- `load()` / `persist()`  
  - Laden/Speichern der Sperr-Liste aus/in einer Datei.

---

## 7. LdapAuthenticator

Der `LdapAuthenticator` kapselt die Authentifizierung gegen den LDAP-Server.

### 7.1 Konfiguration

Typische Parameter:

- LDAP-Host: z.B. `ldap.technikum-wien.at`
- Port: `389`
- Base-DN: z.B. `dc=technikum-wien,dc=at`

### 7.2 authenticate(username, password)

1. Initialisiert eine LDAP-Verbindung.
2. Sucht den DN des Benutzers über einen Filter wie `(uid=<username>)`.
3. Führt einen einfachen Bind mit diesem DN und dem Passwort durch.
4. Rückgabe:
   - `true` bei Erfolg
   - `false` bei Fehler oder ungültigen Credentials

Intern werden Funktionen der OpenLDAP-C-API verwendet (z.B. `ldap_initialize`, `ldap_search_ext_s`, `ldap_sasl_bind_s`).

---

## 8. Gesamtfluss (High-Level)

1. Client verbindet sich mit dem Server.
2. Benutzer meldet sich per `LOGIN` an.
3. Server prüft Blacklist und LDAP.
4. Nach erfolgreichem Login kann der Benutzer:
   - Nachrichten mit `SEND` verschicken,
   - mit `LIST` Betreffzeilen anzeigen,
   - mit `READ` Nachrichten lesen,
   - mit `DEL` Nachrichten löschen.
5. Alle Nachrichten werden im Dateisystem im Spool-Verzeichnis abgelegt.
6. Zu viele falsche Logins führen zu einer temporären Sperre der IP.
7. Mit `QUIT` beendet der Client die Session.

---

## 9. Zusammenfassung

TWMailer Pro implementiert ein einfaches, aber sauberes Messaging-System mit:

- klarer Trennung von Client und Server
- zeilenbasiertem Textprotokoll
- LDAP-Authentifizierung
- dateibasierter Mail-Persistenz
- Thread-basiertem Session-Handling
- Sicherheitsmechanismus durch IP-Blacklist

Dank der modularen Struktur (Server, ClientSession, MailStore, BlacklistManager, LdapAuthenticator)
ist das System gut erweiterbar und verständlich.
