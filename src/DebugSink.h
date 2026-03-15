#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>

class DebugSink {
public:
    static DebugSink& instance();

    void connect(const char* host = "127.0.0.1", unsigned short port = 9876);

    // 斷開連線並清理 Winsock 資源
    void disconnect();

    // 傳送一筆訊息：格式 "[TAG] TEXT\n"（以 UTF-8 編碼）
    void send(const wchar_t* tag, const std::wstring& text);
    void send(const wchar_t* tag, wchar_t ch);

    bool isConnected() const;

private:
    DebugSink() = default;
    ~DebugSink() {
        disconnect();
    }

    // 禁止複製 / 移動
    DebugSink(const DebugSink&) = delete;
    DebugSink& operator=(const DebugSink&) = delete;

    bool _wsaStarted = false;
    SOCKET _sock = INVALID_SOCKET;

    // 將 wstring 轉為 UTF-8 std::string
    static std::string toUtf8(const std::wstring& ws);
};
