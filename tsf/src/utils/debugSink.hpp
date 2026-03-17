#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>

class DebugSink {
public:
    static DebugSink& instance() {
        static DebugSink s;
        return s;
    }

    inline void connect(const char* host = "239.255.42.99", unsigned short port = 9876) {
        if (_sock != INVALID_SOCKET) return;

        if (!_wsaStarted) {
            WSADATA wsaData{};
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;
            _wsaStarted = true;
        }

        SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET) return;

        in_addr multicastAddr{};
        if (inet_pton(AF_INET, host, &multicastAddr) != 1) {
            ::closesocket(sock);
            return;
        }

        _dest = {};
        _dest.sin_family = AF_INET;
        _dest.sin_port = htons(port);
        _dest.sin_addr = multicastAddr;

        // Keep multicast traffic local and visible to local receivers.
        const int ttl = 1;
        setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));

        const int loop = 1;
        setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char*>(&loop), sizeof(loop));

        in_addr loopbackIf{};
        if (inet_pton(AF_INET, "127.0.0.1", &loopbackIf) == 1) {
            setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char*>(&loopbackIf), sizeof(loopbackIf));
        }

        _sock = sock;
    }

    // Close socket and clean up Winsock.
    inline void disconnect() {
        if (_sock != INVALID_SOCKET) {
            ::closesocket(_sock);
            _sock = INVALID_SOCKET;
        }
        if (_wsaStarted) {
            WSACleanup();
            _wsaStarted = false;
        }
    }

    // Send one line in the format "[TAG] TEXT\n" (UTF-8).
    inline void send(const wchar_t* tag, const std::wstring& text) {
        if (_sock == INVALID_SOCKET) {
            connect();
            if (_sock == INVALID_SOCKET) return;
        }

        std::string msg = "[";
        msg += toUtf8(tag);
        msg += "] ";
        msg += toUtf8(text);
        msg += "\n";

        const int sent = ::sendto(
            _sock,
            msg.data(),
            static_cast<int>(msg.size()),
            0,
            reinterpret_cast<const sockaddr*>(&_dest),
            static_cast<int>(sizeof(_dest)));

        if (sent == SOCKET_ERROR) {
            ::closesocket(_sock);
            _sock = INVALID_SOCKET;
        }
    }

    inline void send(const wchar_t* tag, wchar_t ch) {
        send(tag, std::wstring(1, ch));
    }

    bool isConnected() const {
        return _sock != INVALID_SOCKET;
    }

private:
    DebugSink() = default;
    ~DebugSink() {
        disconnect();
    }

    DebugSink(const DebugSink&) = delete;
    DebugSink& operator=(const DebugSink&) = delete;

    bool _wsaStarted = false;
    SOCKET _sock = INVALID_SOCKET;
    sockaddr_in _dest{};

    static std::string toUtf8(const std::wstring& ws) {
        if (ws.empty()) return {};
        int len =
            WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string out(static_cast<size_t>(len), '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), static_cast<int>(ws.size()), out.data(), len, nullptr, nullptr);
        return out;
    }
};
