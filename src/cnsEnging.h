#pragma once
#include <filesystem>
#include <string>

class CnsEngine {
public:
    std::string unicode(const std::wstring& bopomofo) {}

private:
    CnsEngine(){std::filesystem::path bopomofo_to_cns = ".dataset\\Properties\\CNS_phonetic.txt"}
};