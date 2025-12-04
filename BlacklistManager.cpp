#include "BlacklistManager.h"

#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {
constexpr int MAX_ATTEMPTS = 3;
constexpr int BAN_SECONDS = 60;
}

BlacklistManager::BlacklistManager(const std::string &storageFile)
    : storageFile_(storageFile) {
    load();
    cleanupExpired();
}

bool BlacklistManager::isBlacklisted(const std::string &ip) {
    std::lock_guard<std::mutex> lock(mtx_);
    cleanupExpired();
    auto it = blacklist_.find(ip);
    return it != blacklist_.end() && it->second > std::time(nullptr);
}

bool BlacklistManager::recordFailure(const std::string &ip, const std::string &username) {
    std::lock_guard<std::mutex> lock(mtx_);
    cleanupExpired();

    std::string key = attemptKey(ip, username);
    int count = ++attempts_[key];

    if (count >= MAX_ATTEMPTS) {
        std::time_t until = std::time(nullptr) + BAN_SECONDS;
        blacklist_[ip] = until;
        attempts_.erase(key);
        persist();
        return true;
    }
    return false;
}

void BlacklistManager::recordSuccess(const std::string &ip, const std::string &username) {
    std::lock_guard<std::mutex> lock(mtx_);
    attempts_.erase(attemptKey(ip, username));
}

void BlacklistManager::load() {
    std::ifstream in(storageFile_);
    if (!in.is_open()) {
        return;
    }

    std::string ip;
    std::time_t until;
    while (in >> ip >> until) {
        if (until > std::time(nullptr)) {
            blacklist_[ip] = until;
        }
    }
}

void BlacklistManager::persist() {
    std::ofstream out(storageFile_, std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Kann Blacklist nicht schreiben: " << storageFile_ << std::endl;
        return;
    }

    for (const auto &entry : blacklist_) {
        if (entry.second > std::time(nullptr)) {
            out << entry.first << ' ' << entry.second << '\n';
        }
    }
}

void BlacklistManager::cleanupExpired() {
    bool changed = false;
    std::time_t now = std::time(nullptr);
    for (auto it = blacklist_.begin(); it != blacklist_.end();) {
        if (it->second <= now) {
            it = blacklist_.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) {
        persist();
    }
}

std::string BlacklistManager::attemptKey(const std::string &ip, const std::string &username) {
    return ip + "|" + username;
}
