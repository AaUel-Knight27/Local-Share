// Local File Share - single-file C++ HTTP server for Linux
// Build with: g++ -O2 -std=c++17 -o fileshare main.cpp
//
// Usage:
//   ./fileshare /path/to/file

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <cstring>
#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <ctime>
#include <vector>
#include <climits>
#include <cstdlib>

#include "embedded_assets.h"

static std::string g_filePath;
static std::string g_fileName;

std::string humanSize(long long bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = (double)bytes;
    int unitIdx = 0;
    while (size >= 1024.0 && unitIdx < 4) {
        size /= 1024.0;
        unitIdx++;
    }
    std::ostringstream oss;
    if (unitIdx == 0) {
        oss << (long long)size << " " << units[unitIdx];
    } else {
        oss.precision(1);
        oss << std::fixed << size << " " << units[unitIdx];
    }
    return oss.str();
}

std::string fileModifiedDate(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return "unknown";
    time_t t = st.st_mtime;
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[64];
    strftime(buf, sizeof(buf), "%b %d, %Y", &tmv);
    return std::string(buf);
}

long long fileSizeBytes(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return -1;
    return (long long)st.st_size;
}

std::string baseName(const std::string& path) {
    size_t pos = path.find_last_of("/");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::string replaceAll(std::string s, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
    return s;
}

std::vector<std::string> getLocalIPs() {
    std::vector<std::string> ips;
    struct ifaddrs* ifaddr = nullptr;
    if (getifaddrs(&ifaddr) == -1) return ips;

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;

        if (ifa->ifa_flags & IFF_LOOPBACK) continue;

        char ipStr[INET_ADDRSTRLEN];
        void* addrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
        inet_ntop(AF_INET, addrPtr, ipStr, sizeof(ipStr));
        ips.push_back(std::string(ipStr));
    }
    freeifaddrs(ifaddr);
    return ips;
}

void waitForEnter() {
    std::cout << "\nPress Enter to close...";
    std::cin.get();
}

void sendResponse(int client, const std::string& status, const std::string& contentType,
                   const std::string& body, const std::string& extraHeaders = "") {
    std::ostringstream resp;
    resp << "HTTP/1.1 " << status << "\r\n";
    resp << "Content-Type: " << contentType << "\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "Connection: close\r\n";
    resp << extraHeaders;
    resp << "\r\n";
    std::string headerStr = resp.str();
    send(client, headerStr.c_str(), headerStr.size(), 0);
    if (!body.empty()) {
        send(client, body.data(), body.size(), 0);
    }
}

void sendFileResponse(int client, const std::string& path, const std::string& downloadName) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        sendResponse(client, "404 Not Found", "text/plain", "File not found.");
        return;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();

    std::ostringstream headers;
    headers << "Content-Disposition: attachment; filename=\"" << downloadName << "\"\r\n";

    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n";
    resp << "Content-Type: application/octet-stream\r\n";
    resp << "Content-Length: " << content.size() << "\r\n";
    resp << headers.str();
    resp << "Connection: close\r\n\r\n";
    std::string headerStr = resp.str();
    send(client, headerStr.c_str(), headerStr.size(), 0);

    const size_t CHUNK = 65536;
    size_t sent = 0;
    while (sent < content.size()) {
        size_t toSend = std::min(CHUNK, content.size() - sent);
        ssize_t r = send(client, content.data() + sent, toSend, 0);
        if (r <= 0) break;
        sent += r;
    }
}

void handleClient(int client) {
    char buf[8192];
    ssize_t received = recv(client, buf, sizeof(buf) - 1, 0);
    if (received <= 0) {
        close(client);
        return;
    }
    buf[received] = '\0';
    std::string request(buf);

    std::istringstream iss(request);
    std::string method, path, version;
    iss >> method >> path >> version;

    if (path == "/") {
        long long sz = fileSizeBytes(g_filePath);
        if (sz < 0) {
            sendResponse(client, "404 Not Found", "text/plain", "File no longer available.");
        } else {
            std::string html = INDEX_HTML;
            html = replaceAll(html, "{{ .filename }}", g_fileName);
            html = replaceAll(html, "{{ .filesize }}", humanSize(sz));
            html = replaceAll(html, "{{ .modified }}", fileModifiedDate(g_filePath));
            sendResponse(client, "200 OK", "text/html; charset=utf-8", html);
        }
    } else if (path == "/style.css") {
        sendResponse(client, "200 OK", "text/css; charset=utf-8", STYLE_CSS);
    } else if (path == "/download") {
        sendFileResponse(client, g_filePath, g_fileName);
    } else {
        sendResponse(client, "404 Not Found", "text/plain", "Not found.");
    }

    close(client);
}

int main(int argc, char* argv[]) {
    std::cout << "==================================================\n";
    std::cout << " Local File Share (C++/Linux)\n";
    std::cout << "==================================================\n";

    if (argc < 2) {
        std::cout << "\nUsage:\n";
        std::cout << "  ./fileshare /path/to/file\n";
        return 1;
    }

    char resolved[PATH_MAX];
    if (!realpath(argv[1], resolved)) {
        std::cout << "Error: could not resolve path: " << argv[1] << "\n";
        return 1;
    }

    struct stat st;
    if (stat(resolved, &st) != 0 || S_ISDIR(st.st_mode)) {
        std::cout << "Error: no such file: " << resolved << "\n";
        return 1;
    }

    g_filePath = resolved;
    g_fileName = baseName(resolved);

    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        std::cout << "Could not create socket.\n";
        return 1;
    }

    int opt = 1;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(5000);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cout << "Could not bind to port 5000 (already in use?).\n";
        close(listenSocket);
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) < 0) {
        std::cout << "Could not listen on socket.\n";
        close(listenSocket);
        return 1;
    }

    std::cout << "\nSharing: " << g_filePath << "\n\n";
    std::cout << "On your brother's device (same WiFi), open one of:\n";
    for (auto& ip : getLocalIPs()) {
        std::cout << "    http://" << ip << ":5000\n";
    }
    std::cout << "\nKeep this terminal open while sharing.\n";
    std::cout << "Press Ctrl+C to stop.\n";
    std::cout << "==================================================\n";

    while (true) {
        sockaddr_in clientAddr;
        socklen_t clientAddrSize = sizeof(clientAddr);
        int clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &clientAddrSize);
        if (clientSocket < 0) continue;
        handleClient(clientSocket);
    }

    close(listenSocket);
    return 0;
}
