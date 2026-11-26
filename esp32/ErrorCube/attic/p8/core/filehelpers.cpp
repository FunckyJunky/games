// ERROR-CUBE: plain C files instead of iostreams (iostreams cost the ESP32 build a lot of flash)
#include <string>
#include <vector>
#include <stdio.h>

static long fileSizeOf(FILE* f) { fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET); return n < 0 ? 0 : n; }
std::string get_file_contents(std::string filename){
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) return "";
    std::string contents; contents.resize(fileSizeOf(f));
    size_t got = contents.size() ? fread(&contents[0], 1, contents.size(), f) : 0;
    contents.resize(got);
    fclose(f);
    return contents;
}
std::vector<char> get_file_as_buffer(std::string filename){
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) return std::vector<char>(0);
    std::vector<char> buffer(fileSizeOf(f));
    size_t got = buffer.size() ? fread(buffer.data(), 1, buffer.size(), f) : 0;
    buffer.resize(got);
    fclose(f);
    return buffer;
}
std::string get_first_four_chars(std::string filename){
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) return "";
    char b[4]; size_t n = fread(b, 1, 4, f); fclose(f);
    return std::string(b, n);
}
std::vector<unsigned char> get_file_buffer(std::string filename){
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) return std::vector<unsigned char>();
    std::vector<unsigned char> buffer(fileSizeOf(f));
    size_t got = buffer.size() ? fread(buffer.data(), 1, buffer.size(), f) : 0;
    buffer.resize(got);
    fclose(f);
    return buffer;
}

//https://stackoverflow.com/a/8518855
std::string getDirectory(const std::string& fname){
    size_t pos = fname.find_last_of("\\/");
    return (std::string::npos == pos)
        ? ""
        : fname.substr(0, pos);
}

bool isAbsolutePath (std::string const &path) {
    if (path.length() == 0) {
        return false;
    }

    if (path[0] == '/') {
        return true;
    }
    
    size_t colonPos = path.find_first_of(":");
    size_t slashPos = path.find_first_of("/");
    size_t backSlashPos = path.find_first_of("\\");

    if (colonPos != std::string::npos && 
        (slashPos == (colonPos + 1) || backSlashPos == (colonPos + 1))){
        return true;
    }
    
    return false;
}

std::string getFileExtension(std::string const &path) {
    size_t pos = path.find_last_of(".");
     return (std::string::npos == pos)
         ? ""
         : path.substr(pos);
}

bool hasEnding (std::string const &fullString, std::string const &ending) {
    if (fullString.length() >= ending.length()) {
        return (0 == fullString.compare (fullString.length() - ending.length(), ending.length(), ending));
    } else {
        return false;
    }
}

bool isHiddenFile (std::string const &fullString) {
    if (fullString.length() >= 0) {
        return (0 == fullString.compare (0, 2, "._"));
    } else {
        return false;
    }
}

bool isCartFile (std::string const &fullString) {
    return !isHiddenFile(fullString) && 
        (hasEnding(fullString, ".p8") || hasEnding(fullString, ".png"));
}

bool isCPostFile (std::string const &fullString) {
    return !isHiddenFile(fullString) && 
       fullString.rfind("cpost", 0) == 0;
}
