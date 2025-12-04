#pragma once

#include <mutex>
#include <string>
#include <vector>

/// Klasse zur Verwaltung der Mail-Speicherung im Dateisystem.
/// Verantwortlich für das Anlegen, Auflisten, Lesen und Löschen von Nachrichten je Benutzer.
class MailStore {
public:
    /// Erzeugt einen MailStore unterhalb des angegebenen Basisverzeichnisses.
    /// @param baseDir Verzeichnis, in dem alle Benutzerdaten abgelegt werden.
    explicit MailStore(const std::string &baseDir);

    /// Speichert eine Nachricht im Postfach des Empfängers.
    /// @param sender Absenderkennung (aus der eingeloggten Sitzung).
    /// @param receiver Empfängername.
    /// @param subject Betreffzeile der Nachricht.
    /// @param body Kompletter Nachrichtentext.
    /// @return true bei Erfolg, sonst false.
    bool storeMessage(const std::string &sender,
                      const std::string &receiver,
                      const std::string &subject,
                      const std::string &body);

    /// Listet alle Betreffzeilen des Benutzers auf.
    /// @param username Benutzer, dessen Posteingang gelesen werden soll.
    /// @param subjects Ausgabevektor für die Betreffzeilen.
    /// @return true, wenn das Listing erfolgreich erstellt werden konnte.
    bool listMessages(const std::string &username,
                      std::vector<std::string> &subjects);

    /// Liest eine einzelne Nachricht aus dem Postfach.
    /// @param username Benutzer, dessen Postfach durchsucht wird.
    /// @param msgNumber Nummer der Nachricht (1-basiert entsprechend Dateibenennung).
    /// @param sender Ausgabefeld für den Absender.
    /// @param receiver Ausgabefeld für den Empfänger.
    /// @param subject Ausgabefeld für den Betreff.
    /// @param body Ausgabefeld für den Nachrichtentext.
    /// @return true, wenn die Nachricht gelesen werden konnte.
    bool readMessage(const std::string &username,
                     int msgNumber,
                     std::string &sender,
                     std::string &receiver,
                     std::string &subject,
                     std::string &body);

    /// Löscht eine Nachricht dauerhaft.
    /// @param username Benutzer, dessen Nachricht entfernt werden soll.
    /// @param msgNumber Nummer der Nachricht.
    /// @return true bei erfolgreichem Löschen.
    bool deleteMessage(const std::string &username,
                       int msgNumber);

private:
    std::string baseDir_;
    mutable std::mutex mtx_;

    static bool isValidUsername(const std::string &u);
    static void trimNewline(std::string &s);
    static void mkdirIfNotExists(const std::string &path);
    static int getNextMessageId(const std::string &userDir);
};
