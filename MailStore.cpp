#include "MailStore.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
bool isDirectory(const std::string &path) {
    struct stat st {};
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
} // namespace

bool MailStore::isValidUsername(const std::string &u) {
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

MailStore::MailStore(const std::string &baseDir) : baseDir_(baseDir) {
    mkdirIfNotExists(baseDir_);
}

bool MailStore::storeMessage(const std::string &sender,
                             const std::string &receiver,
                             const std::string &subject,
                             const std::string &body) {
    if (!isValidUsername(receiver) || !isValidUsername(sender)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mtx_);

    std::string userDir = baseDir_ + "/" + receiver;
    mkdirIfNotExists(userDir);

    int nextId = getNextMessageId(userDir);
    if (nextId <= 0) {
        return false;
    }

    std::string filename = userDir + "/" + std::to_string(nextId) + ".msg";
    FILE *f = fopen(filename.c_str(), "w");
    if (!f) {
        return false;
    }

    fprintf(f, "%s\n", sender.c_str());
    fprintf(f, "%s\n", receiver.c_str());
    fprintf(f, "%s\n", subject.c_str());

    if (!body.empty()) {
        fprintf(f, "%s", body.c_str());
        if (body.back() != '\n') {
            fprintf(f, "\n");
        }
    }

    fclose(f);
    return true;
}

bool MailStore::listMessages(const std::string &username, std::vector<std::string> &subjects) {
    subjects.clear();

    if (!isValidUsername(username)) {
        return true; // kein Fehler → einfach 0 Mails
    }

    std::lock_guard<std::mutex> lock(mtx_);

    std::string userDir = baseDir_ + "/" + username;
    if (!isDirectory(userDir)) {
        return true;
    }

    DIR *dir = opendir(userDir.c_str());
    if (!dir) {
        return true;
    }

    std::vector<int> ids;
    struct dirent *entry;

    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_REG) {
            std::string name = entry->d_name;

            if (name.size() > 4 && name.substr(name.size() - 4) == ".msg") {
                int id = atoi(name.substr(0, name.size() - 4).c_str());
                if (id > 0) {
                    ids.push_back(id);
                }
            }
        }
    }
    closedir(dir);

    std::sort(ids.begin(), ids.end());

    for (int id : ids) {
        std::string filename = userDir + "/" + std::to_string(id) + ".msg";
        FILE *f = fopen(filename.c_str(), "r");
        if (!f) {
            continue;
        }

        char *line = nullptr;
        size_t len = 0;

        getline(&line, &len, f); // sender
        getline(&line, &len, f); // receiver
        ssize_t n = getline(&line, &len, f); // subject

        if (n > 0) {
            std::string subject(line, static_cast<size_t>(n));
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

bool MailStore::readMessage(const std::string &username,
                            int msgNumber,
                            std::string &sender,
                            std::string &receiver,
                            std::string &subject,
                            std::string &body) {

    sender.clear();
    receiver.clear();
    subject.clear();
    body.clear();

    if (!isValidUsername(username) || msgNumber <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mtx_);

    std::string filename = baseDir_ + "/" + username + "/" + std::to_string(msgNumber) + ".msg";
    FILE *f = fopen(filename.c_str(), "r");
    if (!f) {
        return false;
    }

    char *line = nullptr;
    size_t len = 0;
    ssize_t n;

    n = getline(&line, &len, f);
    if (n <= 0) {
        fclose(f);
        if (line) {
            free(line);
        }
        return false;
    }
    sender.assign(line, static_cast<size_t>(n));
    trimNewline(sender);

    n = getline(&line, &len, f);
    if (n <= 0) {
        fclose(f);
        if (line) {
            free(line);
        }
        return false;
    }
    receiver.assign(line, static_cast<size_t>(n));
    trimNewline(receiver);

    n = getline(&line, &len, f);
    if (n <= 0) {
        fclose(f);
        if (line) {
            free(line);
        }
        return false;
    }
    subject.assign(line, static_cast<size_t>(n));
    trimNewline(subject);

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

bool MailStore::deleteMessage(const std::string &username, int msgNumber) {
    if (!isValidUsername(username) || msgNumber <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mtx_);

    std::string filename = baseDir_ + "/" + username + "/" + std::to_string(msgNumber) + ".msg";
    int res = unlink(filename.c_str());

    return (res == 0);
}

void MailStore::trimNewline(std::string &s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
}

void MailStore::mkdirIfNotExists(const std::string &path) {
    struct stat st {};
    if (stat(path.c_str(), &st) == -1) {
        mkdir(path.c_str(), 0755);
    }
}

int MailStore::getNextMessageId(const std::string &userDir) {
    DIR *dir = opendir(userDir.c_str());

    int maxId = 0;
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_type == DT_REG) {
                std::string name = entry->d_name;
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
    return maxId + 1;
}
