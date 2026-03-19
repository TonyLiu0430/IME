#pragma once
#include <string>

namespace tsf {

class Engine {
public:
    std::wstring add(const std::wstring& composition, wchar_t new_char) {
        if (composition.empty()) {
            return std::wstring{new_char};
        }
        if (composition.back() == new_char) {
            return composition;
        }
        std::wstring res = composition;
        if (composition.back() == L'ㄋ' && new_char == L'ㄧ') {
            res.back() = L'你';
        } else {
            res.push_back(new_char);
        }
        return res;
    };
    static Engine& instance() {
        static Engine engine;
        return engine;
    }

private:
    Engine() = default;
};
}  // namespace tsf