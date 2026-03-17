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

    inline void connect(const char* host = "127.0.0.1", unsigned short port = 9876) {
        if (_sock != INVALID_SOCKET) return;

        if (!_wsaStarted) {
            WSADATA wsaData{};
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;
            _wsaStarted = true;
        }

        SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return;

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
            ::closesocket(sock);
            return;
        }

        // 非阻塞連線（避免在 IME 啟動時卡住主執行緒）
        u_long nonBlocking = 1;
        ioctlsocket(sock, FIONBIO, &nonBlocking);

        ::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

        // 等待最多 200ms
        fd_set wset{};
        FD_ZERO(&wset);
        FD_SET(sock, &wset);
        timeval tv{0, 200'000};  // 200ms
        if (::select(0, nullptr, &wset, nullptr, &tv) > 0) {
            // 連線成功，切回阻塞模式
            nonBlocking = 0;
            ioctlsocket(sock, FIONBIO, &nonBlocking);
            _sock = sock;
        } else {
            ::closesocket(sock);
        }
    }

    // 斷開連線並清理 Winsock 資源
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

    // 傳送一筆訊息：格式 "[TAG] TEXT\n"（以 UTF-8 編碼）
    inline void send(const wchar_t* tag, const std::wstring& text) {
        if (_sock == INVALID_SOCKET) return;

        // 組合訊息：[TAG] TEXT\n
        std::string msg = "[";
        msg += toUtf8(tag);
        msg += "] ";
        msg += toUtf8(text);
        msg += "\n";

        // 全部送出（簡單 loop 處理短資料）
        const char* ptr = msg.c_str();
        int remaining = static_cast<int>(msg.size());
        while (remaining > 0) {
            int sent = ::send(_sock, ptr, remaining, 0);
            if (sent <= 0) {
                // 連線中斷，清理
                ::closesocket(_sock);
                _sock = INVALID_SOCKET;
                return;
            }
            ptr += sent;
            remaining -= sent;
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
