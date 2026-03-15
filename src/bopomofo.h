#pragma once

#include <windows.h>

#include <optional>
#include <unordered_map>

class Bopomofo {
private:
    inline static const std::unordered_map<int, wchar_t> vkToBopomofo{{
        // 聲母
        {'1', L'ㄅ'},
        {'Q', L'ㄆ'},
        {'A', L'ㄇ'},
        {'Z', L'ㄈ'},
        {'2', L'ㄉ'},
        {'W', L'ㄊ'},
        {'S', L'ㄋ'},
        {'X', L'ㄌ'},
        {'E', L'ㄍ'},
        {'D', L'ㄎ'},
        {'C', L'ㄏ'},
        {'R', L'ㄐ'},
        {'F', L'ㄑ'},
        {'V', L'ㄒ'},
        {'5', L'ㄓ'},
        {'T', L'ㄔ'},
        {'G', L'ㄕ'},
        {'B', L'ㄖ'},
        {'Y', L'ㄗ'},
        {'H', L'ㄘ'},
        {'N', L'ㄙ'},
        // 韻母
        {'A', L'ㄚ'},
        {'O', L'ㄛ'},
        {'P', L'ㄜ'},
        {';', L'ㄝ'},
        {'/', L'ㄞ'},
        {'.', L'ㄟ'},
        {'<', L'ㄠ'},
        {'L', L'ㄡ'},
        {',', L'ㄢ'},
        {'K', L'ㄣ'},
        {'J', L'ㄤ'},
        {'U', L'ㄥ'},
        {'M', L'ㄦ'},
        {'I', L'ㄧ'},
        {'U', L'ㄨ'},
        {'M', L'ㄩ'},
        // 聲調
        {VK_SPACE, L' '},  // 一聲（陰平）
        {'6', L'ˊ'},
        {'3', L'ˇ'},
        {'4', L'ˋ'},
        {'7', L'˙'},
    }};

public:
    static std::optional<wchar_t> lookup(int vk) {
        auto it = vkToBopomofo.find(vk);
        if (it == vkToBopomofo.end()) return std::nullopt;
        return it->second;
    }
};