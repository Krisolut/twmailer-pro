#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static bool sendAll(int sockfd, const std::string &data) {
    const char *buf = data.c_str();
    size_t total = 0;
    size_t len = data.size();
    while (total < len) {
        ssize_t n = send(sockfd, buf + total, len - total, 0);
        if (n <= 0) return false;
        total += static_cast<size_t>(n);
    }
    return true;
}

static bool recvLine(int sockfd, std::string &line) {
    line.clear();
    char c;
    while (true) {
        ssize_t n = recv(sockfd, &c, 1, 0);
        if (n <= 0) {
            return false;
        }
        if (c == '\n') break;
        line.push_back(c);
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

static void menu(bool loggedIn) {
    std::cout << "\nTW-Mailer Client\n";
    std::cout << "1) LOGIN\n";
    std::cout << "2) SEND\n";
    std::cout << "3) LIST\n";
    std::cout << "4) READ\n";
    std::cout << "5) DEL\n";
    std::cout << "6) QUIT\n";
    std::cout << (loggedIn ? "(Angemeldet)" : "(Nicht angemeldet)") << "\n";
    std::cout << "Choice: ";
}

static bool doLOGIN(int sockfd, std::string &username, bool &loggedIn) {
    std::cout << "Username: ";
    std::getline(std::cin, username);
    std::cout << "Passwort: ";
    std::string password;
    std::getline(std::cin, password);

    std::string req;
    req += "LOGIN\n";
    req += username + "\n";
    req += password + "\n";

    if (!sendAll(sockfd, req)) {
        std::cerr << "Error sending LOGIN request\n";
        return false;
    }

    std::string resp;
    if (!recvLine(sockfd, resp)) {
        std::cerr << "No response from server\n";
        return false;
    }

    if (resp == "OK") {
        std::cout << "Login erfolgreich.\n";
        loggedIn = true;
    } else {
        std::cout << "Login fehlgeschlagen.\n";
        loggedIn = false;
    }
    return true;
}

static void doSEND(int sockfd, const std::string &username, bool loggedIn) {
    if (!loggedIn) {
        std::cout << "Bitte zuerst LOGIN ausführen.\n";
        return;
    }

    std::string receiver, subject;

    std::cout << "Receiver (max 8 chars, a-z,0-9): ";
    std::getline(std::cin, receiver);
    std::cout << "Subject (max 80 chars): ";
    std::getline(std::cin, subject);

    std::cout << "Message body (end with single '.' on a line):\n";
    std::string body;
    while (true) {
        std::string line;
        std::getline(std::cin, line);
        if (line == ".") break;
        body += line + "\n";
    }

    std::string req;
    req += "SEND\n";
    req += receiver + "\n";
    req += subject + "\n";
    req += body;
    req += ".\n";

    if (!sendAll(sockfd, req)) {
        std::cerr << "Error sending SEND request\n";
        return;
    }

    std::string resp;
    if (!recvLine(sockfd, resp)) {
        std::cerr << "No response from server\n";
        return;
    }

    std::cout << "Server: " << resp << std::endl;
}

static void doLIST(int sockfd, const std::string &username, bool loggedIn) {
    if (!loggedIn) {
        std::cout << "Bitte zuerst LOGIN ausführen.\n";
        return;
    }

    std::string req;
    req += "LIST\n";

    if (!sendAll(sockfd, req)) {
        std::cerr << "Error sending LIST request\n";
        return;
    }

    std::string line;
    if (!recvLine(sockfd, line)) {
        std::cerr << "No response from server\n";
        return;
    }

    int count = std::atoi(line.c_str());
    std::cout << "Messages for " << username << ": " << count << "\n";

    for (int i = 1; i <= count; ++i) {
        if (!recvLine(sockfd, line)) {
            std::cerr << "Unexpected end of response\n";
            return;
        }
        std::cout << i << ") " << line << "\n";
    }
}

static void doREAD(int sockfd, const std::string &username, bool loggedIn) {
    if (!loggedIn) {
        std::cout << "Bitte zuerst LOGIN ausführen.\n";
        return;
    }

    std::string num;
    std::cout << "Message number: ";
    std::getline(std::cin, num);

    std::string req;
    req += "READ\n";
    req += num + "\n";

    if (!sendAll(sockfd, req)) {
        std::cerr << "Error sending READ request\n";
        return;
    }

    std::string line;
    if (!recvLine(sockfd, line)) {
        std::cerr << "No response from server\n";
        return;
    }

    if (line == "ERR") {
        std::cout << "Server: ERR\n";
        return;
    }
    if (line != "OK") {
        std::cout << "Unexpected response: " << line << "\n";
        return;
    }

    std::string sender, receiver, subject;
    if (!recvLine(sockfd, sender) ||
        !recvLine(sockfd, receiver) ||
        !recvLine(sockfd, subject)) {
        std::cerr << "Incomplete message header\n";
        return;
    }

    std::cout << "Sender:   " << sender << "\n";
    std::cout << "Receiver: " << receiver << "\n";
    std::cout << "Subject:  " << subject << "\n";
    std::cout << "Body:\n";

    while (true) {
        if (!recvLine(sockfd, line)) {
            std::cerr << "Connection lost while reading body\n";
            return;
        }
        if (line == ".") break;
        std::cout << line << "\n";
    }
}

static void doDEL(int sockfd, const std::string &username, bool loggedIn) {
    if (!loggedIn) {
        std::cout << "Bitte zuerst LOGIN ausführen.\n";
        return;
    }

    std::string num;
    std::cout << "Message number: ";
    std::getline(std::cin, num);

    std::string req;
    req += "DEL\n";
    req += num + "\n";

    if (!sendAll(sockfd, req)) {
        std::cerr << "Error sending DEL request\n";
        return;
    }

    std::string line;
    if (!recvLine(sockfd, line)) {
        std::cerr << "No response from server\n";
        return;
    }

    std::cout << "Server: " << line << "\n";
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: ./twmailer-client <ip> <port>\n";
        return 1;
    }

    const char *ip = argv[1];
    int port = std::atoi(argv[2]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        std::cerr << "Invalid IP address\n";
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }

    std::cout << "Connected to " << ip << ":" << port << "\n";

    bool loggedIn = false;
    std::string username;

    while (true) {
        menu(loggedIn);
        std::string choice;
        std::getline(std::cin, choice);
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
            std::string req = "QUIT\n";
            sendAll(sockfd, req);
            break;
        } else {
            std::cout << "Unknown choice.\n";
        }
    }

    close(sockfd);
    return 0;
}
