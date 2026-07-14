#include "config.h"
#include <fstream>
#include <sstream>
#include <algorithm>

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static uint32_t parseColor(const std::string& hex) {
    std::string h = hex;
    if (!h.empty() && h[0] == '#') h = h.substr(1);
    if (h.size() == 3) {
        // Short form: #RGB -> #RRGGBB
        h = std::string(1, h[0]) + h[0] + h[1] + h[1] + h[2] + h[2];
    }
    if (h.size() != 6) return 0;
    uint32_t r = std::stoul(h.substr(0, 2), nullptr, 16);
    uint32_t g = std::stoul(h.substr(2, 2), nullptr, 16);
    uint32_t b = std::stoul(h.substr(4, 2), nullptr, 16);
    return (r << 16) | (g << 8) | b;
}

bool Config::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') continue; // Section header

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));

        // Remove quotes from value
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        if (key == "family") fontFamily = value;
        else if (key == "size") fontSize = std::stoi(value);
        else if (key == "background") background = parseColor(value);
        else if (key == "foreground") foreground = parseColor(value);
        else if (key == "cursor") cursor = parseColor(value);
        else if (key == "selection") selection = parseColor(value);
        else if (key == "opacity") opacity = std::stof(value);
        else if (key.substr(0, 5) == "color") {
            int idx = std::stoi(key.substr(5));
            if (idx >= 0 && idx < 16) {
                ansiColors[idx] = parseColor(value);
            }
        }
    }

    return true;
}
