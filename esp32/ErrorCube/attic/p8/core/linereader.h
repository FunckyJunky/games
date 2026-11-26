// linereader.h (ERROR-CUBE): std::getline over a string without <sstream>
#pragma once
#include <string>
struct LineReader {
    const std::string& s; size_t p = 0;
    explicit LineReader(const std::string& str) : s(str) {}
    bool next(std::string& line) {
        if (p >= s.size()) return false;
        size_t e = s.find('\n', p);
        if (e == std::string::npos) e = s.size();
        line.assign(s, p, e - p);
        p = e + 1;
        return true;
    }
};
