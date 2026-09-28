#include "template.hpp"

namespace ssg {

namespace {

std::string trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

}  // namespace

std::string render(const std::string& tpl, const Context& ctx) {
    std::string out;
    out.reserve(tpl.size() * 2);

    for (size_t i = 0; i < tpl.size();) {
        if (i + 2 <= tpl.size() && tpl[i] == '{' && tpl[i + 1] == '{') {
            const size_t end = tpl.find("}}", i + 2);
            if (end != std::string::npos) {
                const std::string key = trim(tpl.substr(i + 2, end - i - 2));
                const auto it = ctx.find(key);
                if (it != ctx.end()) out += it->second;
                i = end + 2;
                continue;
            }
        }
        out += tpl[i];
        ++i;
    }
    return out;
}

}  // namespace ssg
