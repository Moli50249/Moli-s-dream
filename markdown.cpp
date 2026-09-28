#include "markdown.hpp"

#include <sstream>

namespace ssg {

namespace {

std::string escapeHtml(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (const char c : s) {
        switch (c) {
            case '&':  o += "&amp;";  break;
            case '<':  o += "&lt;";   break;
            case '>':  o += "&gt;";   break;
            case '"':  o += "&quot;"; break;
            case '\'': o += "&#39;";  break;
            default:   o += c;        break;
        }
    }
    return o;
}

bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

std::string strip(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string headingLevel(const std::string& line, int& level) {
    size_t i = 0;
    while (i < line.size() && line[i] == '#') ++i;
    // 支持 "# 标题" 和中文里常见的无空格写法 "#标题"
    if (i == 0 || i > 3 || i >= line.size()) return "";
    level = static_cast<int>(i);
    return strip(line.substr(i));
}

}  // namespace

std::string inlineMd(const std::string& text) {
    const std::string s = escapeHtml(text);
    std::string out;
    out.reserve(s.size() + 32);

    for (size_t i = 0; i < s.size();) {
        // **粗体**
        if (s.compare(i, 2, "**") == 0) {
            const size_t e = s.find("**", i + 2);
            if (e != std::string::npos) {
                out += "<strong>" + s.substr(i + 2, e - i - 2) + "</strong>";
                i = e + 2;
                continue;
            }
        }
        // *斜体*（**粗体** 已在前面处理并跳过，这里只可能是单星号）
        if (s[i] == '*') {
            const size_t e = s.find('*', i + 1);
            if (e != std::string::npos && e > i + 1) {
                out += "<em>" + s.substr(i + 1, e - i - 1) + "</em>";
                i = e + 1;
                continue;
            }
        }
        // `代码`
        if (s[i] == '`') {
            const size_t e = s.find('`', i + 1);
            if (e != std::string::npos) {
                out += "<code>" + s.substr(i + 1, e - i - 1) + "</code>";
                i = e + 1;
                continue;
            }
        }
        // [文字](链接)
        if (s[i] == '[') {
            const size_t cb = s.find(']', i + 1);
            if (cb != std::string::npos && cb + 1 < s.size() && s[cb + 1] == '(') {
                const size_t pe = s.find(')', cb + 2);
                if (pe != std::string::npos) {
                    const std::string label = s.substr(i + 1, cb - i - 1);
                    const std::string href = s.substr(cb + 2, pe - cb - 2);
                    out += "<a href=\"" + href +
                           "\" target=\"_blank\" rel=\"noopener\">" + label + "</a>";
                    i = pe + 1;
                    continue;
                }
            }
        }
        out += s[i];
        ++i;
    }
    return out;
}

std::string mdToHtml(const std::string& md) {
    std::istringstream in(md);
    std::string raw;
    std::string out;

    bool inList = false;
    bool inPara = false;

    auto closeList = [&]() {
        if (inList) { out += "</ul>\n"; inList = false; }
    };
    auto closePara = [&]() {
        if (inPara) { out += "</p>\n"; inPara = false; }
    };

    while (std::getline(in, raw)) {
        const std::string line = strip(raw);

        if (line.empty()) {
            closeList();
            closePara();
            continue;
        }

        if (line == "---" || line == "***") {
            closeList();
            closePara();
            out += "<hr>\n";
            continue;
        }

        int level = 0;
        const std::string heading = headingLevel(line, level);
        if (!heading.empty()) {
            closeList();
            closePara();
            // h1 是姓名，h2 是区块标题，正文里最高只到 h3
            const int tag = level + 2;
            out += "<h" + std::to_string(tag) + ">" + inlineMd(heading) +
                   "</h" + std::to_string(tag) + ">\n";
            continue;
        }

        if (startsWith(line, "- ") || startsWith(line, "* ")) {
            closePara();
            if (!inList) { out += "<ul>\n"; inList = true; }
            out += "  <li>" + inlineMd(strip(line.substr(2))) + "</li>\n";
            continue;
        }

        closeList();
        if (!inPara) { out += "<p>"; inPara = true; } else { out += " "; }
        out += inlineMd(line);
    }

    closeList();
    closePara();
    return out;
}

}  // namespace ssg
